#pragma once

#include "ordermatcher.h"
#include "order.h"
#include <queue>
#include <iostream>

class Application {

    enum OrderStatus {REJECTED, NEW, FILLED, PARTIALLY_FILLED, CANCELED};

    void processOrder(const Order&);
    void processCancel(const std::string& id, const std::string& symbol, Order::Side);

    void updateOrder(const Order&, char status);
    void rejectOrder(const Order& order);
    void acceptOrder(const Order& order);
    void fillOrder(const Order& order);
    void cancelOrder(const Order& order);
    void rejectOrder();
    
    OrderMatcher m_orderMatcher;

    public:
        const OrderMatcher& orderMatcher() { return m_orderMatcher; }
};