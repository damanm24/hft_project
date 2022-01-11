#include <cassert>
#include <stdio.h>
#include "tcp.h"

int main() {
    RawSocket *sock = new RawSocket();
    sock->init();
    int fd = sock->open();
    tcp_pcb* pcb = sock->m_pcb->get(fd);   
    assert(fd == sock->m_pcb->id(pcb));
    sock->shutdown();
}