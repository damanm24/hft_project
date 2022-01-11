#include "sys/time.h"

#include <string.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tcp.h"

unsigned short checksum(const char *buf, unsigned size)
{
	unsigned sum = 0, i;

	/* Accumulate checksum */
	for (i = 0; i < size - 1; i += 2)
	{
		unsigned short word16 = *(unsigned short *) &buf[i];
		sum += word16;
	}

	/* Handle odd-sized case */
	if (size & 1)
	{
		unsigned short word16 = (unsigned char) buf[i];
		sum += word16;
	}

	/* Fold to get the ones-complement result */
	while (sum >> 16) sum = (sum & 0xFFFF)+(sum >> 16);

	/* Invert to get the negative in ones-complement arithmetic */
	return ~sum;
}

RawSocket::RawSocket() {
    m_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (m_sockfd == -1)
	{
		throw std::runtime_error("socket creation failed");
	}

	// tell the kernel that headers are included in the packet
	int one = 1;
	const int *val = &one;
	if (setsockopt(m_sockfd, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) == -1)
	{
		throw std::runtime_error("setsockopt(IP_HDRINCL, 1) failed");
	}

    m_mutex = PTHREAD_MUTEX_INITIALIZER;
    m_pcb = new pcb();
    m_init = false;
	m_interrupt = false;
}

RawSocket::~RawSocket() {
    if (m_recieve.joinable()) {
        m_recieve.join();
    }
    if (m_retransmit.joinable()) {
        m_retransmit.join();
    }
    delete m_pcb;
    pthread_mutex_destroy(&m_mutex);
}

void RawSocket::input(RawSocket *sock) {
	unsigned short dst_port;
	struct sockaddr from;
	unsigned int addrlen;
	addrlen = sizeof(from);

	struct sockaddr_in local;
	int received;
	size_t len = 1500;
	do
	{
		if (sock->m_interrupt == true) {
			return;
		}
		uint8_t *buffer = (uint8_t *)calloc(1, len);
		received = recvfrom(sock->m_sockfd, buffer, len, 0, &from, &addrlen);
		if (received < 0)
			break;
		struct iphdr *iph = (struct iphdr*)buffer;

		memcpy(&dst_port, buffer + 22, sizeof(dst_port));
		local.sin_port = dst_port;
		local.sin_addr.s_addr = iph->daddr;
		if (from.sa_family == AF_INET && sock->m_pcb->select(&local, NULL) != NULL) {
			tcp_segment_info seg;
			struct tcphdr *tcph = (struct tcphdr*)(buffer + sizeof(struct iphdr));
			seg.seq = ntohl(tcph->seq);
			seg.ack = ntohl(tcph->ack_seq);
			seg.len = received - sizeof(iphdr) - sizeof(tcphdr);
			if (tcph->syn) {
				seg.len++;
			}
			if (tcph->fin) {
				seg.len++;
			}
			seg.wnd = ntohs(tcph->window);
			seg.up = ntohs(tcph->urg_ptr);
            pthread_mutex_lock(&(sock->m_mutex));
			struct sockaddr_in *temp = (struct sockaddr_in *)&from;
			temp->sin_port = tcph->source;
			sock->input_segment_arrives(&seg, buffer + sizeof(iphdr), received - sizeof(iphdr) - sizeof(tcphdr), &local, (struct sockaddr_in *)&from);
            pthread_mutex_unlock(&(sock->m_mutex));
		} else {
			free(buffer);
		}
	}
	while (1);   
}

void RawSocket::input_segment_arrives(tcp_segment_info* seg, uint8_t* buffer, int len, struct sockaddr_in* local, struct sockaddr_in* foreign) {
    tcp_pcb *pcb, *new_pcb;
    int acceptable = 0;
    struct tcphdr *tcph = (struct tcphdr*)(buffer);
	uint8_t *data = (buffer + sizeof(tcphdr));

    pcb = m_pcb->select(local, foreign);
	if (!pcb || pcb->state == TCP_STATE_CLOSED) {
		if (tcph->rst) {
			return;
		}
		if (!tcph->ack) {
			output_segment(0, seg->seq + seg->len, TCP_FLG_RST | TCP_FLG_ACK, 0, NULL, 0, pcb->local, pcb->foreign);
		} else {
			output_segment(seg->ack, 0, TCP_FLG_RST, 0, NULL, 0, pcb->local, pcb->foreign);
		}
		return;
	}

	switch (pcb->state) {
	case TCP_STATE_LISTEN:
		if (tcph->rst) {
			return;
		}

		if (tcph->ack) {
			output_segment(seg->ack, 0, TCP_FLG_RST, 0, NULL, 0, pcb->local, pcb->foreign);
			return;
		}

		if (tcph->syn) {
			new_pcb = m_pcb->alloc();
			if (!new_pcb) {
				printf("pcb alloc failed");
				return;
			}
			new_pcb->parent = pcb;
			pcb = new_pcb;
			struct iphdr *iphdr = (struct iphdr*) ((uint8_t*) buffer - sizeof(tcphdr) - sizeof(iphdr));
			pcb->foreign.sin_family = AF_INET;
			pcb->foreign.sin_addr.s_addr = inet_addr("10.0.0.4"); //HARD CODED FOR NOW
			pcb->foreign.sin_port = tcph->source;
			memcpy(&pcb->local, local, sizeof(pcb->local));
			pcb->rcv.wnd = sizeof(pcb->m_buf);
			pcb->rcv.nxt = seg->seq + 1;
			pcb->irs = seg->seq;
			pcb->iss = random();
			output(pcb, TCP_FLG_SYN | TCP_FLG_ACK, NULL, 0);
			pcb->snd.nxt = pcb->iss + 1;
			pcb->snd.una = pcb->iss;
			pcb->state = TCP_STATE_SYN_RECEIVED;
			return;
		}
		return;
	case TCP_STATE_SYN_SENT:
		if (tcph->ack) {
			if (seg->ack <= pcb->iss || seg->ack > pcb->snd.nxt) {
				output_segment(seg->ack, 0, TCP_FLG_RST, 0, NULL, 0, pcb->local, pcb->foreign);
				return;
			}
			if (pcb->snd.una <= seg->ack && seg->ack <= pcb->snd.nxt) {
				acceptable = 1;
			}
		}

		if (tcph->rst) {
			if (acceptable) {
				pcb->state = TCP_STATE_CLOSED;
				printf("RECIEVED RESET WHEN IN SYN SENT STATE");
				m_pcb->release(pcb);
			}
			return;
		}

		if (tcph->syn) {
			pcb->rcv.nxt = seg->seq + 1;
			pcb->irs = seg->seq;
			if (acceptable) {
				pcb->snd.una = seg->ack;
				retransmit_queue_cleanup(pcb);
			}

			if (pcb->snd.una > pcb->iss) {
				pcb->state = TCP_STATE_ESTABLISHED;
				output(pcb, TCP_FLG_ACK, NULL, 0);
				pcb->snd.wnd = seg->wnd;
				pcb->snd.wl1 = seg->seq;
				pcb->snd.wl2 = seg->ack;
				pthread_cond_broadcast(&pcb->m_cond);
				return;
			} else {
				pcb->state = TCP_STATE_SYN_RECEIVED;
				output(pcb, TCP_FLG_SYN | TCP_FLG_ACK, NULL, 0);
				return;
			}
		}
		return;
	}

	switch (pcb->state) {
		case TCP_STATE_SYN_RECEIVED:
		case TCP_STATE_ESTABLISHED:
		case TCP_STATE_FIN_WAIT1:
		case TCP_STATE_FIN_WAIT2:
		case TCP_STATE_CLOSE_WAIT:
		case TCP_STATE_CLOSING:
		case TCP_STATE_LAST_ACK:
		case TCP_STATE_TIME_WAIT:
			if (!seg->len) {
				if (!pcb->rcv.wnd) {
					if (seg->seq == pcb->rcv.nxt) {
						acceptable = 1;
					}
				} else {
					if (pcb->rcv.nxt <= seg->seq && seg->seq < pcb->rcv.nxt + pcb->rcv.wnd) {
						acceptable = 1;
					}
				}
			} else {
				if (!pcb->rcv.wnd) {
					//not acceptable state
				} else {
					if ((pcb->rcv.nxt <= seg->seq && seg->seq < pcb->rcv.nxt + pcb->rcv.wnd) ||
						(pcb->rcv.nxt <= seg->seq + seg->len - 1 && seg->seq + seg->len - 1 < pcb->rcv.nxt + pcb->rcv.wnd)) {
							acceptable = 1;
						}
				}
			}
			if (!acceptable) {
				if (tcph->rst) {
					output(pcb, TCP_FLG_ACK, NULL, 0);
				}
				return;
			}
	}

	switch (pcb->state) {
	case TCP_STATE_SYN_RECEIVED:
		if (tcph->rst) {
			pcb->state = TCP_STATE_CLOSED;
			m_pcb->release(pcb);
			return;
		}
		break;
	case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT1:
    case TCP_STATE_FIN_WAIT2:
    case TCP_STATE_CLOSE_WAIT:
		if (tcph->rst) {
			pcb->state = TCP_STATE_CLOSED;
			m_pcb->release(pcb);
			return;
		}
		break;
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
		if (tcph->rst) {
			pcb->state = TCP_STATE_CLOSED;
			m_pcb->release(pcb);
			return;
		}
		break;
	}

	switch (pcb->state) {
    case TCP_STATE_SYN_RECEIVED:
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT1:
    case TCP_STATE_FIN_WAIT2:
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
		if (tcph->syn) {
			output(pcb, TCP_FLG_RST, NULL, 0);
			pcb->state = TCP_STATE_CLOSED;
			m_pcb->release(pcb);
			return;
		}	
	}

	if (!tcph->ack) {
		return;
	}

	switch (pcb->state) {
	case TCP_STATE_SYN_RECEIVED:
		if (pcb->snd.una <= seg->ack && seg->ack <= pcb->snd.nxt) {
			pcb->state = TCP_STATE_ESTABLISHED;
			pthread_cond_broadcast(&pcb->m_cond);
			//add more here
			if (pcb->parent) {
				pcb->parent->m_backlog.push_back(pcb);
				pthread_cond_broadcast(&pcb->parent->m_cond);
			}
		} else {
			output_segment(seg->ack, 0, TCP_FLG_RST, 0, NULL, 0, pcb->local, pcb->foreign);
			return;
		}
	case TCP_STATE_ESTABLISHED:
	case TCP_STATE_FIN_WAIT1:
	case TCP_STATE_FIN_WAIT2:
	case TCP_STATE_CLOSE_WAIT:
	case TCP_STATE_CLOSING:
		if (pcb->snd.una < seg->ack && seg->ack <= pcb->snd.nxt) {
			pcb->snd.una = seg->ack;
			retransmit_queue_cleanup(pcb);
			if (pcb->snd.wl1 < seg->seq || (pcb->snd.wl1 == seg->seq && pcb->snd.wl2 <= seg->ack)) {
				pcb->snd.wnd = seg->wnd;
                pcb->snd.wl1 = seg->seq;
                pcb->snd.wl2 = seg->ack;
			}
		} else if (seg->ack < pcb->snd.una) {
            /* ignore */
        } else if (seg->ack > pcb->snd.nxt) {
            output(pcb, TCP_FLG_ACK, NULL, 0);
            return;
        }
		switch (pcb->state) {
		case TCP_STATE_FIN_WAIT1:
			if (seg->ack == pcb->snd.nxt) {
				pcb->state = TCP_STATE_FIN_WAIT2;
			}
			break;
		case TCP_STATE_FIN_WAIT2:
			break;
		case TCP_STATE_CLOSE_WAIT:
			break;
		case TCP_STATE_CLOSING:
			if (seg->ack == pcb->snd.nxt) {
				pcb->state = TCP_STATE_TIME_WAIT;
				set_timewait_timer(pcb);
				pthread_cond_broadcast(&pcb->m_cond);
			}
			break;
		}
		break;
	case TCP_STATE_LAST_ACK:
		if (seg->ack == pcb->snd.nxt) {
			pcb->state = TCP_STATE_CLOSED;
			m_pcb->release(pcb);
		}
		return;
	case TCP_STATE_TIME_WAIT:
		if (tcph->fin) {
			set_timewait_timer(pcb);
		}
		break;
	}
	
	switch (pcb->state) {
	case TCP_STATE_ESTABLISHED:
	case TCP_STATE_FIN_WAIT1:
	case TCP_STATE_FIN_WAIT2:
		if (len) {
			memcpy(pcb->m_buf + sizeof(pcb->m_buf) - pcb->rcv.wnd, data, len);
			pcb->rcv.nxt = seg->seq + seg->len;
			pcb->rcv.wnd -= len;
			output(pcb, TCP_FLG_ACK, NULL, 0);
			pthread_cond_broadcast(&pcb->m_cond);
		}
		break;
	case TCP_STATE_CLOSE_WAIT:
	case TCP_STATE_CLOSING:
	case TCP_STATE_LAST_ACK:
	case TCP_STATE_TIME_WAIT:
		break;
	}

	if (tcph->fin) {
		switch (pcb->state) {
		case TCP_STATE_CLOSED:
		case TCP_STATE_LISTEN:
		case TCP_STATE_SYN_SENT:
		return;	
		}
		pcb->rcv.nxt = seg->seq + 1;
		output(pcb, TCP_FLG_ACK, NULL, 0);
		switch (pcb->state) {
		case TCP_STATE_SYN_RECEIVED:
		case TCP_STATE_ESTABLISHED:
			pcb->state = TCP_STATE_CLOSE_WAIT;
			pthread_cond_broadcast(&pcb->m_cond);
			break;
		case TCP_STATE_FIN_WAIT1:
			if(seg->ack == pcb->snd.nxt) {
				pcb->state = TCP_STATE_TIME_WAIT;
				set_timewait_timer(pcb);
			} else {
				pcb->state = TCP_STATE_CLOSING;
			}
			break;
		case TCP_STATE_FIN_WAIT2:
			pcb->state = TCP_STATE_TIME_WAIT;
			set_timewait_timer(pcb);
			break;
		case TCP_STATE_CLOSE_WAIT:
			break;
		case TCP_STATE_CLOSING:
			break;
		case TCP_STATE_LAST_ACK:
			break;
		case TCP_STATE_TIME_WAIT:
			set_timewait_timer(pcb);
			break;
		}
	}
	return;
}

void RawSocket::set_timewait_timer(tcp_pcb* pcb) {
    gettimeofday(&pcb->tw_timer, NULL);
	pcb->tw_timer.tv_sec += TCP_TIMEWAIT_SEC;
}

int RawSocket::output(tcp_pcb* pcb, uint8_t flags, uint8_t *data, size_t len) {
    uint32_t seq;
	seq = pcb->snd.nxt;
	if (TCP_FLG_ISSET(flags, TCP_FLG_SYN)) {
		seq = pcb->iss;
	}
	if (TCP_FLG_ISSET(flags, TCP_FLG_SYN | TCP_FLG_FIN) || len) {
		retransmit_queue_add(pcb, seq, flags, data, len);
	}
	return output_segment(seq, pcb->rcv.nxt, flags, pcb->rcv.wnd, data, len, pcb->local, pcb->foreign);
}

ssize_t RawSocket::output_segment(uint32_t seq, uint32_t ack, uint8_t flags, uint16_t wnd, uint8_t *data, size_t len, struct sockaddr_in local, struct sockaddr_in foreign) {
	uint8_t *buf = (uint8_t *) calloc(DATAGRAM_LEN, sizeof(char));
	uint16_t psum;
	uint16_t total;

	struct iphdr *iph = (struct iphdr*)buf;

	// IP header configuration
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;
	iph->id = htonl(rand() % 65535); // id of this packet
	iph->frag_off = 0;
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + OPT_SIZE;
	iph->saddr = local.sin_addr.s_addr;
	iph->daddr = foreign.sin_addr.s_addr; 

	//Fields should be set by caller
	iph->check = 0; 
 
	struct tcphdr *hdr = (struct tcphdr*)(buf + sizeof(struct iphdr));
	struct pseudo_hdr psh;

	hdr->source = local.sin_port;
	hdr->dest = foreign.sin_port;
	hdr->seq = htonl(seq);
	hdr->ack_seq = htonl(ack);
	hdr->doff = 10; // tcp header size
	hdr->th_flags = flags;
	hdr->check = 0; // correct calculation follows later
	hdr->window = htons(wnd); // window size
	hdr->urg_ptr = 0;

	int psize = 0;
	psh.src = local.sin_addr.s_addr;
	psh.dest = foreign.sin_addr.s_addr;
	psh.zero = 0;
	psh.protocol = IPPROTO_TCP;

	if (len != 0) {
		iph->tot_len += len;
		psh.len = htons(sizeof(struct tcphdr) + OPT_SIZE + len);
		psize = sizeof(struct pseudo_hdr) + sizeof(struct tcphdr) + OPT_SIZE + len;
	} else {
		psh.len = htons(sizeof(struct tcphdr) + OPT_SIZE);
		psize = sizeof(struct pseudo_hdr) + sizeof(struct tcphdr) + OPT_SIZE;
	}
	// fill pseudo packet
	char* pseudogram = (char *)malloc(psize);
	memcpy(pseudogram, (char*)&psh, sizeof(struct pseudo_hdr));

	if (len != 0) {
		memcpy(pseudogram + sizeof(struct pseudo_hdr), hdr, sizeof(struct tcphdr) + OPT_SIZE + len);
	} else {
		memcpy(pseudogram + sizeof(struct pseudo_hdr), hdr, sizeof(struct tcphdr) + OPT_SIZE);
	}
	
	
	if (flags & TCP_FLG_SYN) {
		// TCP options are only set in the SYN packet
		// ---- set mss ----
		buf[40] = 0x02;
		buf[41] = 0x04;
		int16_t mss = htons(48); // mss value
		memcpy(buf + 42, &mss, sizeof(int16_t));
		// ---- enable SACK ----
		buf[44] = 0x04;
		buf[45] = 0x02;
		// do the same for the pseudo header
		pseudogram[32] = 0x02;
		pseudogram[33] = 0x04;
		memcpy(pseudogram + 34, &mss, sizeof(int16_t));
		pseudogram[36] = 0x04;
		pseudogram[37] = 0x02;
	}


	hdr->check = checksum((const char*)pseudogram, psize);
	iph->check = checksum((const char*)buf, iph->tot_len);

	free(pseudogram);
	return sendto(m_sockfd, buf, iph->tot_len, 0, (struct sockaddr*) &foreign, sizeof(struct sockaddr));
}

int RawSocket::retransmit_queue_add(tcp_pcb* pcb, uint32_t seq, uint8_t flags, uint8_t* data, size_t len) {
	tcp_retransmit_entry *entry;
	uint8_t *datacpy;
	
	entry = (tcp_retransmit_entry*) calloc(1, sizeof(*entry));
	if (!entry) {
		std::runtime_error("couldnot create retransmit entry");
		return -1;
	}
	datacpy = (uint8_t *) malloc(len);
	if (!datacpy) {
		free(entry);
		std::runtime_error("couldnot create retransmit entry");
		return -1;
	}
	entry->rto = TCP_DEFAULT_RTO;
	entry->seq = seq;
	entry->flg = flags;
	entry->len = len;
	memcpy(datacpy, data, len);
	gettimeofday(&entry->first, NULL);
	entry->last = entry->first;
	pcb->m_retransmit.push_back(entry);
	return 0;
}

void RawSocket::retransmit_queue_cleanup(tcp_pcb* pcb) {
	tcp_retransmit_entry *entry;
	while (!pcb->m_retransmit.empty()) {
		entry = pcb->m_retransmit.front();
		if (entry->seq >= pcb->snd.una) {
			break;
		}
		pcb->m_retransmit.erase(pcb->m_retransmit.begin());
		free(entry->data);
		free(entry);
	}
	return;
}

void RawSocket::retransmit_queue_emit(tcp_pcb* pcb) {
	struct timeval now, diff, timeout;
	gettimeofday(&now, NULL);
	
	for (auto itr : pcb->m_retransmit) {
		timersub(&now, &itr->first, &diff);
		if (diff.tv_sec >= TCP_RETRANSMIT_DEADLINE) {
			pcb->state = TCP_STATE_CLOSED;
			pthread_cond_broadcast(&pcb->m_cond);
			return;
		}
		timeout = itr->last;
		timeval_add_usec(&timeout, itr->rto);
		if (timercmp(&now, &timeout, >)) {
			output_segment(itr->seq, pcb->rcv.nxt, itr->flg, pcb->rcv.wnd, itr->data, itr->len, pcb->local, pcb->foreign);
			itr->last = now;
			itr->rto *= 2;
		}
	}
}

void RawSocket::retransmit(RawSocket *sock) {
    struct timeval now;
    tcp_pcb *pcb;
    
    while (1) {
		if (sock->m_interrupt) {
			return;
		}
        pthread_mutex_lock(&sock->m_mutex);
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            pcb = sock->m_pcb->m_connections[i];
            if (pcb->state == TCP_STATE_FREE) {
                continue;
            }
            if (pcb->state == TCP_STATE_TIME_WAIT) {
                gettimeofday(&now, NULL);
                if (timercmp(&now, &pcb->tw_timer, >) != 0) {
                    sock->m_pcb->release(sock->m_pcb->m_connections[i]);
                    continue;
                } 
            }
            sock->retransmit_queue_emit(pcb);
        }
        pthread_mutex_unlock(&sock->m_mutex);
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(0.5s);
    }
}

int RawSocket::init() {
    m_recieve = std::thread(input, this);
    m_retransmit = std::thread(retransmit, this);
    m_init = true;
    return 0;
}

int RawSocket::shutdown() {
	m_interrupt = true;
    if (m_recieve.joinable()) {
        m_recieve.join();
    }
    if (m_retransmit.joinable()) {
        m_retransmit.join();
    }
    m_init = false;
    return 0;
}

int RawSocket::open() {
    pthread_mutex_lock(&m_mutex);
    tcp_pcb* pcb = m_pcb->alloc();
    if (!pcb) {
        printf("Failed to create new tcp_pcb\n");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }
    int id = m_pcb->id(pcb);
    pthread_mutex_unlock(&m_mutex);
    return id;
}

int RawSocket::connect(int id, struct sockaddr_in foreign) {
    tcp_pcb* pcb;
    int p;
    int state;
    struct timespec timeout;

    if (!m_init) {
        printf("TCP Module not initialized. Run init()\n");
        return -1;
    }

    pthread_mutex_lock(&m_mutex);
    pcb = m_pcb->get(id);

    if (!pcb) {
        printf("pcb not found\n");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }

    pcb->foreign = foreign;
    pcb->rcv.wnd = sizeof(pcb->m_buf);
    pcb->iss = random();

	if (output(pcb, TCP_FLG_SYN, NULL, 0) == -1) {
		pcb->state = TCP_STATE_CLOSED;
		pthread_mutex_unlock(&m_mutex);
		throw std::runtime_error("Send SYN Failed.");
	}
	pcb->snd.una = pcb->iss;
	pcb->snd.nxt = pcb->iss + 1;
	pcb->state = TCP_STATE_SYN_SENT;
AGAIN:
    state = pcb->state;
	while ((pcb->state == state)) {
		clock_gettime(CLOCK_REALTIME, &timeout);
		timespec_add_nsec(&timeout, 10000000);
		pcb->m_wait++;
		pthread_cond_timedwait(&pcb->m_cond, &m_mutex, &timeout);
		pcb->m_wait--;
	}
	if (pcb->state != TCP_STATE_ESTABLISHED) {
		if (pcb->state == TCP_STATE_SYN_RECEIVED) {
			goto AGAIN;
		}
		printf("connect error");
		pcb->state = TCP_STATE_CLOSED;
        m_pcb->release(pcb);
		pthread_mutex_unlock(&m_mutex);
		return -1;
	}
    id = m_pcb->id(pcb);
	pthread_mutex_unlock(&m_mutex);
	return id;
}

int RawSocket::bind(int id, struct sockaddr_in local) {
    struct tcp_pcb *pcb, *exist;
    if (!m_init) {
        printf("TCP Module not initialized. Run init()\n");
        return -1;
    }
    //Make sure there is only one call for bind in a program otherwise this don't work properly
    pthread_mutex_lock(&m_mutex);
    pcb = m_pcb->get(id);
    if (!pcb) {
        printf("pcb not found\n");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }

    pcb->local = local;
    pthread_mutex_unlock(&m_mutex);
    return 0;
}

int RawSocket::listen(int id) {
    struct tcp_pcb *pcb;

    if (!m_init) {
        printf("TCP Module not initialized. Run init()\n");
        return -1;
    }

    pthread_mutex_lock(&m_mutex);
    pcb = m_pcb->get(id);
    if (!pcb) {
        printf("pcb not found\n");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }

    pcb->state = TCP_STATE_LISTEN;
    pthread_mutex_unlock(&m_mutex);
    return 0;
}

int RawSocket::accept(int id, struct sockaddr_in* foreign) {
    struct tcp_pcb *pcb, *new_pcb;
    int new_id;
    struct timespec timeout;

    if (!m_init) {
        printf("TCP Module not initialized. Run init()\n");
        return -1;
    }

    pthread_mutex_lock(&m_mutex);
    pcb = m_pcb->get(id);
    if (!pcb) {
        printf("pcb not found\n");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }

    if (pcb->state != TCP_STATE_LISTEN) {
		printf("not in listen state.");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	}

    while (pcb->m_backlog.empty()) {
		clock_gettime(CLOCK_REALTIME, &timeout);
		timespec_add_nsec(&timeout, 10000000);
		pcb->m_wait++;
		pthread_cond_timedwait(&pcb->m_cond, &m_mutex, &timeout);
		pcb->m_wait--;
	}

	new_pcb = pcb->m_backlog.back();
	pcb->m_backlog.pop_back();
	new_id = m_pcb->id(new_pcb);

    if (foreign) {
        foreign->sin_family = pcb->foreign.sin_family;
        foreign->sin_addr.s_addr = pcb->foreign.sin_addr.s_addr;
        foreign->sin_port = pcb->foreign.sin_port;
    }

	pthread_mutex_unlock(&m_mutex);
	return new_id;
}

int RawSocket::close(int id) {
    struct tcp_pcb *pcb;
    struct timespec timeout;
    if (!m_init) {
        printf("TCP Module not initialized. Run init()\n");
        return -1;
    }

    pthread_mutex_lock(&m_mutex);
    pcb = m_pcb->get(id);
    if (!pcb) {
        printf("pcb not found\n");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }

	switch (pcb->state) {
	case TCP_STATE_CLOSED:
		printf("connection doesn't exist");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	case TCP_STATE_LISTEN:
	case TCP_STATE_SYN_SENT:
		pcb->state = TCP_STATE_CLOSED;
		break;
	case TCP_STATE_SYN_RECEIVED:
	case TCP_STATE_ESTABLISHED:
		output(pcb, TCP_FLG_ACK | TCP_FLG_FIN, NULL, 0);
		pcb->snd.nxt++;
		pcb->state = TCP_STATE_FIN_WAIT1;
		break;
	case TCP_STATE_FIN_WAIT1:
	case TCP_STATE_FIN_WAIT2:
		printf("connection already closing");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	case TCP_STATE_CLOSE_WAIT:
		output(pcb, TCP_FLG_ACK | TCP_FLG_FIN, NULL, 0);
		pcb->snd.nxt++;
		pcb->state = TCP_STATE_LAST_ACK;
		break;
	case TCP_STATE_CLOSING:
	case TCP_STATE_LAST_ACK:
	case TCP_STATE_TIME_WAIT:
		printf("connection already closing");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	default:
		printf("unknown state");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	}
	if (pcb->state == TCP_STATE_CLOSED) {
		m_pcb->release(pcb);
	} else {
		pthread_cond_broadcast(&pcb->m_cond);
	}
	while (pcb->state != TCP_STATE_CLOSED) {
		clock_gettime(CLOCK_REALTIME, &timeout);
		timespec_add_nsec(&timeout, 10000000);
		pcb->m_wait++;
		pthread_cond_timedwait(&pcb->m_cond, &m_mutex, &timeout);
		pcb->m_wait--;
	}
	pthread_mutex_unlock(&m_mutex);
	return 0;  
}

ssize_t RawSocket::send(int id, uint8_t* data, size_t len) {
    tcp_pcb* pcb;
	ssize_t sent = 0;
	size_t mss, cap, slen;
	struct timespec timeout;
    if (!m_init) {
        printf("TCP Module not initialized. Run init()\n");
        return -1;
    }

    pthread_mutex_lock(&m_mutex);
    pcb = m_pcb->get(id);
    if (!pcb) {
        printf("pcb not found\n");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }
RETRY:
	switch (pcb->state) {
	case TCP_STATE_CLOSED:
		printf("connection does not exist");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	case TCP_STATE_LISTEN:
		printf("this connection is passive");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	case TCP_STATE_SYN_SENT:
	case TCP_STATE_SYN_RECEIVED:
		printf("connection has not been established yet");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	case TCP_STATE_ESTABLISHED:
	case TCP_STATE_CLOSE_WAIT:
		while (sent < (ssize_t) len) {
			cap = pcb->snd.wnd - (pcb->snd.nxt - pcb->snd.una);
			if (!cap) {
				clock_gettime(CLOCK_REALTIME, &timeout);
				timespec_add_nsec(&timeout, 10000000);
				pcb->m_wait++;
				pthread_cond_timedwait(&pcb->m_cond, &m_mutex, &timeout);
				pcb->m_wait--;
				goto RETRY;
			}
			slen = std::min(std::min((size_t) DATAGRAM_LEN, len - sent), cap);
			if (output(pcb, TCP_FLG_ACK | TCP_FLG_PSH, data + sent, slen) == -1) {
				printf("tcp_output failure");
				pcb->state = TCP_STATE_CLOSED;
				m_pcb->release(pcb);
				pthread_mutex_unlock(&m_mutex);
				return -1;
			}
			pcb->snd.nxt += slen;
			sent += slen;
		}
		break;
	case TCP_STATE_FIN_WAIT1:
	case TCP_STATE_FIN_WAIT2:
	case TCP_STATE_CLOSING:
	case TCP_STATE_LAST_ACK:
	case TCP_STATE_TIME_WAIT:
		printf("connection closing");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	default:
		printf("unknown state");
		pthread_mutex_unlock(&m_mutex);
		return -1;
	
	}
	pthread_mutex_unlock(&m_mutex);
	return sent;
}

ssize_t RawSocket::recieve(int id, uint8_t* buf, size_t size) {
    tcp_pcb* pcb;
    size_t remain, len;
    struct timespec timeout;
    if (!m_init) {
        printf("TCP Module not initialized. Run init()\n");
        return -1;
    }

    pthread_mutex_lock(&m_mutex);
    pcb = m_pcb->get(id);
    if (!pcb) {
        printf("pcb not found\n");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }
RETRY:
    switch (pcb->state) {
    case TCP_STATE_CLOSED:
        printf("connection does not exist");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    case TCP_STATE_LISTEN:
    case TCP_STATE_SYN_SENT:
    case TCP_STATE_SYN_RECEIVED:
        /* ignore: Queue for processing after entering ESTABLISHED state */
        printf("insufficient resources");
        pthread_mutex_unlock(&m_mutex);
        return -1;
    case TCP_STATE_ESTABLISHED:
    case TCP_STATE_FIN_WAIT1:
    case TCP_STATE_FIN_WAIT2:
        remain = sizeof(pcb->m_buf) - pcb->rcv.wnd;
        if (!remain) {
            clock_gettime(CLOCK_REALTIME, &timeout);
            timespec_add_nsec(&timeout, 10000000); /* 100ms */
            pcb->m_wait++;
            pthread_cond_timedwait(&pcb->m_cond, &m_mutex, &timeout);
            pcb->m_wait--;
            goto RETRY;
        }
        break;
    case TCP_STATE_CLOSE_WAIT:
        remain = sizeof(pcb->m_buf) - pcb->rcv.wnd;
        if (remain) {
            break;
        }
        /* fall through */
    case TCP_STATE_CLOSING:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_TIME_WAIT:
        printf("connection closing");
        pthread_mutex_unlock(&m_mutex);
        return 0;
    default:
        printf("unknown state '%u'", pcb->state);
        pthread_mutex_unlock(&m_mutex);
        return -1;
    }
    len = std::min(size, remain);
    memcpy(buf, pcb->m_buf, len);
    memmove(pcb->m_buf, pcb->m_buf + len, remain - len);
    pcb->rcv.wnd += len;
    pthread_mutex_unlock(&m_mutex);
    return len;
}