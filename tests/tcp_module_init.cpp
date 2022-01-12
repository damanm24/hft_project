#include <cassert>
#include <csignal>
#include <stdio.h>
#include <arpa/inet.h>
#include "tcp.h"

RawSocket *sock = NULL;

void signalHandler(int signum) {
	if (sock == NULL) {
		return;
	}
	sock->shutdown();
}

int main() {
    signal(SIGINT, signalHandler);
    sock = new RawSocket();
    sock->init();
    int fd = sock->open();
    struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(12345);
    inet_pton(AF_INET, "10.0.0.4", &saddr.sin_addr);
    sock->connect(fd, saddr);
	char request[] = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    sock->send(fd, (uint8_t *)request, sizeof(request) - 1/sizeof(char));
    sock->close(fd);
    return 0;
}