#pragma once

#include "ordermatcher.h"
#include "order.h"
#include "tcp.h"
#include <queue>
#include <iostream>
#include <vector>
#include <map>

class Application {

    enum OrderStatus {REJECTED, NEW, FILLED, PARTIALLY_FILLED, CANCELED};

    static void onMessage(int fd, Application *app);
    void sendMessage(int fd, uint8_t* buffer, size_t len);

    void processOrder(const Order&);
    // void processCancel(const std::string& id, const std::string& symbol, Order::Side);

    // void updateOrder(const Order&, OrderStatus status);
    void rejectOrder(const Order& order);
    void acceptOrder(const Order& order);
    void fillOrder(const Order& order);
    // void cancelOrder(const Order& order);
    
    OrderMatcher m_orderMatcher;
    RawSocket *m_sock;
    std::vector<std::thread> m_threads;
    std::map<std::string, int> m_accounts; //Firms->socket fd

    public:
        void start();
        const OrderMatcher& orderMatcher() { return m_orderMatcher; }
};