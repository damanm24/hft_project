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
    sock->bind(fd, saddr);
    sock->listen(fd);
	char request[] = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int newfd = sock->accept(fd, NULL);
	uint8_t buff[8192];
	while (1) {
		ssize_t len = sock->recieve(newfd, buff, 8192);
		if (len == 0) {
			sock->close(newfd);
			break;
		}
		buff[len] = 0;
		printf("%s", buff);
		sock->send(newfd, (uint8_t *)request, sizeof(request) - 1/sizeof(char));
	}
    return 0;
}