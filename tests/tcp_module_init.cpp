#include <cassert>
#include <csignal>
#include <stdio.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <string.h>
#include "tcp.h"

RawSocket *sock = NULL;

void signalHandler(int signum) {
	if (sock == NULL) {
		return;
	}
	sock->shutdown();
}

int main(int argc, char** argv) {
    signal(SIGINT, signalHandler);
    sock = new RawSocket();
    sock->init();
    int fd = sock->open();
    struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(12345);
    inet_pton(AF_INET, "10.0.0.4", &saddr.sin_addr);
    sock->connect(fd, saddr);
    while (1) {
        sock->send(fd, (uint8_t *)argv[1], strlen(argv[1]) + 1);
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(5s);
    }
    
    sock->close(fd);
    return 0;
}