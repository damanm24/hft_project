#include "application.h"

#include <thread>
#include <arpa/inet.h>

void Application::onMessage(int fd, RawSocket *sock) {
    uint8_t buffer[1024];
    while (1) {
        ssize_t len = sock->recieve(fd, buffer, sizeof(buffer));
        if (len == 0) {
            sock->close(fd);
        }

        if (buffer[0] == 'O') {
            //New Order

        } else if (buffer[0] == 'X') {
            //Cancel Order

        } else if (buffer[0] == 'M') {
            //Modify Order

        } else {
            std::cout << "Bad Message Format" << std::endl;
        }
    }
}

void Application::start() {
    RawSocket *sock = new RawSocket();
    sock->init();
    int fd = sock->open();
    struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(12345);
    inet_pton(AF_INET, "10.0.0.4", &saddr.sin_addr);
    m_threads = std::vector<std::thread>();
    sock->bind(fd, saddr);
    sock->listen(fd);

    while (true) {
        struct sockaddr_in daddr;
        int newfd = sock->accept(fd, &daddr);
        m_threads.push_back(std::thread(onMessage, newfd));
    }
}