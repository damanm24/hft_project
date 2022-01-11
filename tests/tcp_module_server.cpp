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
	saddr.sin_port = htons(54321);
    inet_pton(AF_INET, "10.0.0.4", &saddr.sin_addr);
    sock->bind(fd, saddr);
    sock->listen(fd);
    int newfd = sock->accept(fd, NULL);
    printf("hello");
	uint8_t buff[8192];
	while (1) {
		ssize_t len = sock->recieve(newfd, buff, 8192);
		buff[len] = 0;
		printf("%s", buff);
	}
    return 0;
}