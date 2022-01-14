#include <cassert>
#include <csignal>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <string.h>
#include <iostream>
#include "tcp.h"
#include "ouch.h"

RawSocket *sock = NULL;

int main(int argc, char** argv) {
    sock = new RawSocket();
    sock->init();
    int fd = sock->open();
    struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(12345);
    inet_pton(AF_INET, "10.0.0.4", &saddr.sin_addr);
    sock->connect(fd, saddr);
    order_message message;
    message.type = 'O';
    memcpy(&message.order_token, "ABCDEFGHIJKLM", 14);
    message.indicator = 'B';
    message.shares = 100;
    memcpy(&message.stock, "TSLA", 5);
    message.price = 500;
    memcpy(&message.firm, "DRW", 4);
    sock->send(fd, (uint8_t *)&message, sizeof(message));
    uint8_t buffer[100];
    sock->recieve(fd, buffer, sizeof(accept_order_message));
    std::cout << buffer[0] << std::endl;
    printf("%c", buffer[0]);
    message.indicator = 'S';
    sock->send(fd, (uint8_t *)&message, sizeof(message));
    sock->recieve(fd, buffer, sizeof(accept_order_message));
    std::cout << buffer[0] << std::endl;

    sock->recieve(fd, buffer, sizeof(executed_order_message));
    std::cout << buffer[0] << std::endl;
    sock->close(fd);
    return 0;
}