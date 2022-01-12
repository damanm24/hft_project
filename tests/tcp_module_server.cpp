#include <cassert>
#include <csignal>
#include <stdio.h>
#include <arpa/inet.h>
#include <thread>
#include "tcp.h"

RawSocket *sock = NULL;

void signalHandler(int signum) {
	if (sock == NULL) {
		return;
	}
	sock->shutdown();
}

static void handler(int fd) {
	uint8_t buff[8192];
	while (1) {
		ssize_t len = sock->recieve(fd, buff, 8192);
		if (len == 0) {
			sock->close(fd);
			break;
		}
		buff[len] = 0;
		printf("%s", buff);
	}
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
    std::thread t1;
	std::thread t2;
	int i = 0;
	while (1) {
		int newfd = sock->accept(fd, NULL);
		if (i == 0) {
			t1 = std::thread(handler, newfd);
			i++;
		} else {
			t2 = std::thread(handler, newfd);
		}
	}
    return 0;
}