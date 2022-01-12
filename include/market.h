#pragma once

#include "order.h"
#include <map>
#include <queue>
#include <string>
#include <functional>

class Market {
    public:
        bool insert( const Order& order );
        void erase( const Order& order );
        Order& find( Order::Side side, std::string id );
        bool match( std::queue < Order > & );
        void display() const;
    private:
        typedef std::multimap<int, Order, std::greater<int>> BidOrders;
        typedef std::multimap<int, Order, std::less<int>> AskOrders;

        void match(Order& bid, Order& ask);

        std::queue<Order> m_orderUpdates;
        BidOrders m_bidOrders;
        AskOrders m_askOrders;
};