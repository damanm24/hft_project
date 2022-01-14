#pragma once

#include "stdint.h"
#include "pthread.h"
#include <netinet/ip.h>
#include <vector>
#include <thread>
#include <atomic>
#include <queue>

#define TCP_FLG_FIN 0x01
#define TCP_FLG_SYN 0x02
#define TCP_FLG_RST 0x04
#define TCP_FLG_PSH 0x08
#define TCP_FLG_ACK 0x10
#define TCP_FLG_URG 0x20

#define TCP_FLG_IS(x, y) ((x & 0x3f) == (y))
#define TCP_FLG_ISSET(x, y) ((x & 0x3f) & (y) ? 1 : 0)

#define TCP_STATE_FREE         0
#define TCP_STATE_CLOSED       1
#define TCP_STATE_LISTEN       2
#define TCP_STATE_SYN_SENT     3
#define TCP_STATE_SYN_RECEIVED 4
#define TCP_STATE_ESTABLISHED  5
#define TCP_STATE_FIN_WAIT1    6
#define TCP_STATE_FIN_WAIT2    7
#define TCP_STATE_CLOSING      8
#define TCP_STATE_TIME_WAIT    9
#define TCP_STATE_CLOSE_WAIT  10
#define TCP_STATE_LAST_ACK    11

#define TCP_DEFAULT_RTO 200000 /* micro seconds */
#define TCP_RETRANSMIT_DEADLINE 12 /* seconds */
#define TCP_TIMEWAIT_SEC 30 /* substitute for 2MSL */

#define DATAGRAM_LEN 1460
#define OPT_SIZE 20

struct pseudo_hdr {
    uint32_t src;
    uint32_t dest;
    uint8_t zero;
    uint8_t protocol;
    uint16_t len;
} typedef pseudo_hdr;

struct tcp_segment_info {
    uint32_t seq;
    uint32_t ack;
    uint16_t len;
    uint16_t wnd;
    uint16_t up;
} typedef tcp_segment_info;


#define timeval_add_usec(x, y)         \
    do {                               \
        (x)->tv_sec += y / 1000000;    \
        (x)->tv_usec += y % 1000000;   \
        if ((x)->tv_usec >= 1000000) { \
            (x)->tv_sec += 1;          \
            (x)->tv_usec -= 1000000;   \
        }                              \
    } while(0);

#define timespec_add_nsec(x, y)           \
    do {                                  \
        (x)->tv_sec += y / 1000000000;    \
        (x)->tv_nsec += y % 1000000000;   \
        if ((x)->tv_nsec >= 1000000000) { \
            (x)->tv_sec += 1;             \
            (x)->tv_nsec -= 1000000000;   \
        }                                 \
    } while(0);


#define MAX_CONNECTIONS 16

struct tcp_retransmit_entry {
    struct timeval first;
    struct timeval last;
    unsigned int rto;
    uint32_t seq;
    uint8_t flg;
    size_t len;
    uint8_t *data;
} typedef tcp_retransmit_entry;

struct tcp_pcb {
    int state;
    struct sockaddr_in local;
    struct sockaddr_in foreign;
    struct {
        uint32_t nxt;
        uint32_t una;
        uint16_t wnd;
        uint16_t up;
        uint32_t wl1;
        uint32_t wl2;
    } snd;
    uint32_t iss;
    struct {
        uint32_t nxt;
        uint16_t wnd;
        uint16_t up;
    } rcv;
    uint32_t irs;
    uint16_t mtu;
    uint16_t mss;
    uint8_t m_buf[65535];
    pthread_cond_t m_cond;
    int m_wait;
    std::vector<tcp_retransmit_entry *> m_retransmit;
    std::vector<struct tcp_pcb *> m_backlog;
    struct tcp_pcb *parent;
    struct timeval tw_timer;
} typedef tcp_pcb;
class pcb {
    public:
        pcb();
        ~pcb();

        tcp_pcb* alloc();
        void release(tcp_pcb* pcb);
        tcp_pcb* select(struct sockaddr_in* local, struct sockaddr_in* foreign);
        tcp_pcb* select_on_local_port(unsigned short local_port);
        int id(tcp_pcb* pcb);
        tcp_pcb* get(int id);
        tcp_pcb* m_connections[MAX_CONNECTIONS];
    private:

};

class RawSocket {
    public:
        RawSocket();
        ~RawSocket();

        int init();
        int shutdown();

        //Connection Methods
        int open();
        int connect(int id, struct sockaddr_in foreign);
        int bind(int id, struct sockaddr_in local);
        int listen(int id);
        int accept(int id, struct sockaddr_in* foreign);
        int close(int id);

        //Data Transfer Methods
        ssize_t send(int id, uint8_t *data, size_t len);
        ssize_t recieve(int id, uint8_t *buf, size_t size);
        pcb* m_pcb;

    private:
        int output(tcp_pcb* pcb, uint8_t flags, uint8_t *data, size_t len);
        ssize_t output_segment(uint32_t seq, uint32_t ack, uint8_t flags, uint16_t wnd, uint8_t *data, size_t len, struct sockaddr_in local, struct sockaddr_in foreign);

        static void input(RawSocket *sock);
        void input_segment_arrives(tcp_segment_info* seg, uint8_t* buffer, int len, struct sockaddr_in* local, struct sockaddr_in* foreign);

        static void retransmit(RawSocket *sock);
        int retransmit_queue_add(tcp_pcb* pcb, uint32_t seq, uint8_t flags, uint8_t* data, size_t len);
        void retransmit_queue_cleanup(tcp_pcb* pcb);
        void retransmit_queue_emit(tcp_pcb* pcb);

        void set_timewait_timer(tcp_pcb* pcb);

        int m_sockfd;
        bool m_init;
        bool m_interrupt;
        std::thread m_recieve;
        std::thread m_retransmit;
        pthread_mutex_t m_mutex;
};
