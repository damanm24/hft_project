#include <stdlib.h>
#include <string.h>
#include "tcp.h"

const in_addr_t IP_ADDR_ANY = 0x00000000;

pcb::pcb() {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        m_connections[i] = (tcp_pcb*)malloc(sizeof(tcp_pcb));
        memset(m_connections[i], 0, sizeof(*m_connections[i]));
    }

}

pcb::~pcb() {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        tcp_pcb *curr = m_connections[i];
        while (!curr->m_retransmit.empty()) {
            tcp_retransmit_entry *back = curr->m_retransmit.back();
            free(back->data);
            free(back);
            curr->m_retransmit.pop_back();
        }
        free(curr);
    }
}

tcp_pcb* pcb::alloc() {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (m_connections[i]->state == TCP_STATE_FREE) {
            m_connections[i]->state = TCP_STATE_CLOSED;
            pthread_cond_init(&m_connections[i]->m_cond, NULL);
            return m_connections[i];
        }
    }
    return NULL;
}

void pcb::release(tcp_pcb* pcb) {
    if (pcb->m_wait) {
        pthread_cond_broadcast(&pcb->m_cond);
        return;
    }
    while (!pcb->m_retransmit.empty()) {
        tcp_retransmit_entry *back = pcb->m_retransmit.back();
        free(back->data);
        free(back);
        pcb->m_retransmit.pop_back();
    }
    pthread_cond_destroy(&pcb->m_cond);
    memset(pcb, 0, sizeof(*pcb));
}

//SELECT TO DO
tcp_pcb* pcb::select(struct sockaddr_in* local, struct sockaddr_in* foreign) {
    tcp_pcb *pcb, *listen_pcb = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        pcb = m_connections[i];
        if ((pcb->local.sin_addr.s_addr == IP_ADDR_ANY || pcb->local.sin_addr.s_addr == local->sin_addr.s_addr) && pcb->local.sin_port == local->sin_port) {
            if (!foreign) {
                return pcb;
            }
            if (pcb->foreign.sin_addr.s_addr == foreign->sin_addr.s_addr && pcb->foreign.sin_port == foreign->sin_port) {
                return pcb;
            }
            if (pcb->state == TCP_STATE_LISTEN) {
                if (pcb->foreign.sin_addr.s_addr == IP_ADDR_ANY && pcb->foreign.sin_port == 0) {
                    listen_pcb = pcb;
                }
            }
        } 
    }
    return listen_pcb;
}

tcp_pcb* pcb::select_on_local_port(unsigned short foreign_port) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (m_connections[i]->local.sin_port == foreign_port) {
            return m_connections[i];
        }
    }

    return NULL;
}

int pcb::id(tcp_pcb* pcb) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (m_connections[i] == pcb) {
            return i;
        }
    }
    return -1;
}

tcp_pcb* pcb::get(int id) {
    if (id < 0 || id >= MAX_CONNECTIONS) {
        return NULL;
    }

    if (m_connections[id]->state == TCP_STATE_FREE) {
        return NULL;
    }
    return m_connections[id];
}