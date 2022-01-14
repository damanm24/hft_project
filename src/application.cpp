#include "application.h"
#include "ouch.h"

#include <thread>
#include <string>
#include <chrono>
#include <ctime>
#include <string.h>
#include <arpa/inet.h>

void Application::onMessage(int fd, Application *app) {
    uint8_t buffer[1024];
    RawSocket *sock = app->m_sock;
    while (1) {
        ssize_t len = sock->recieve(fd, buffer, sizeof(buffer));
        if (len <= 0) {
            sock->close(fd);
            return;
        }

        if (buffer[0] == 'O') {
            order_message* message = (order_message*) buffer;
            Order::Side side = Order::buy;
            Order::Type type = Order::limit;
            if (message->tif == 0) {
                type == Order::market;
            }
            if (message->indicator == 'S') {
                side = Order::sell;
            }
            std::string token = message->order_token;
            std::string symbol = message->stock;
            std::string firm = message->firm;
            long price = message->price;
            long quantity = message->shares;

            if (app->m_accounts.find(firm) == app->m_accounts.end()) {
                app->m_accounts.insert({firm, fd});
            }

            Order order(token, symbol, firm, side, type, price, quantity);
            app->processOrder(order);

        } else if (buffer[0] == 'X') {
            //Cancel Order
            cancel_order_message* message = (cancel_order_message*) buffer;
            std::string token = message->order_token;
            long shares = message->shares;

        } else {
            std::cout << "Bad Message Format" << std::endl;
            sock->close(fd);
            return;
        }
    }
}

std::chrono::system_clock::duration duration_since_midnight() {
    auto now = std::chrono::system_clock::now();

    time_t tnow = std::chrono::system_clock::to_time_t(now);
    tm *date = std::localtime(&tnow);
    date->tm_hour = 0;
    date->tm_min = 0;
    date->tm_sec = 0;
    auto midnight = std::chrono::system_clock::from_time_t(std::mktime(date));

    return now-midnight;
}

void Application::acceptOrder(const Order& order) {
    accept_order_message message;
    message.type = 'A';
    message.timestamp = duration_since_midnight().count();
    memcpy(&message.order_token, order.getOrderToken().data(), std::min(order.getOrderToken().length(), (size_t)14));
    if (order.getType() == Order::Type::market) {
        message.tif = 0;
    } else {
        message.tif = 0x1869C;
    }
    if (order.getSide() == Order::Side::buy) {
        message.indicator = 'B';
    } else {
        message.indicator = 'S';
    }
    message.shares = order.getQuantity();
    memcpy(&message.stock, order.getSymbol().data(), std::min(order.getSymbol().length(), (size_t) 8));
    message.price = order.getPrice();
    memcpy(&message.firm, order.getOwner().data(), std::min(order.getSymbol().length(), (size_t) 4));

    m_sock->send(m_accounts.find(order.getOwner())->second, (uint8_t *) &message, sizeof(message));
}

void Application::rejectOrder(const Order& order) {
    reject_order_message message;
    message.type = 'J';
    message.reason = 'T';
    message.timestamp = duration_since_midnight().count();
    memcpy(&message.order_token, order.getOrderToken().data(), std::min(order.getOrderToken().length(), (size_t) 14));

    m_sock->send(m_accounts.find(order.getOwner())->second, (uint8_t *) &message, sizeof(message));
}

void Application::processOrder(const Order& order) {
    if (m_orderMatcher.insert(order)) {
        acceptOrder(order);
        std::queue<Order> orders;
        m_orderMatcher.match(order.getSymbol(), orders);
        while (orders.size()) {
            fillOrder(orders.front());
            orders.pop();
        }
    } else {
        rejectOrder(order);
    }
}

void Application::fillOrder(const Order& order) {
    executed_order_message message;
    message.type = 'E';
    message.timestamp = duration_since_midnight().count();
    memcpy(&message.order_token, order.getOrderToken().data(), std::min(order.getOrderToken().length(), (size_t)14));
    message.shares = order.getLastExecutedQuantity();
    message.execution_price = order.getLastExecutedPrice();
    message.liquidity_flag = 'A';
    message.match_number = 12345;

    m_sock->send(m_accounts.find(order.getOwner())->second, (uint8_t *) &message, sizeof(message));
}

void Application::start() {
    m_sock = new RawSocket();
    m_sock->init();
    int fd = m_sock->open();
    struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(12345);
    inet_pton(AF_INET, "10.0.0.4", &saddr.sin_addr);
    m_threads = std::vector<std::thread>();
    m_sock->bind(fd, saddr);
    m_sock->listen(fd);

    while (true) {
        struct sockaddr_in daddr;
        int newfd = m_sock->accept(fd, &daddr);
        m_threads.push_back(std::thread(onMessage, newfd, this));
    }
}