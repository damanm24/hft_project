#include "market.h"
#include <iostream>

bool Market::insert(const Order &order)
{
    if (order.getSide() == Order::buy)
    {
        m_bidOrders.insert(BidOrders::value_type(order.getPrice(), order));
    }
    else
    {
        m_askOrders.insert(AskOrders::value_type(order.getPrice(), order));
    }
    return true;
}

void Market::erase(const Order &order)
{
    std::string id = order.getOrderToken();
    if (order.getSide() == Order::buy)
    {
        BidOrders::iterator itr;
        for (itr = m_bidOrders.begin(); itr != m_bidOrders.end(); ++itr)
        {
            if (itr->second.getOrderToken() == id)
            {
                m_bidOrders.erase(itr);
                return;
            }
        }
    }
    else if (order.getSide() == Order::sell)
    {
        AskOrders::iterator itr;
        for (itr = m_askOrders.begin(); itr != m_askOrders.end(); ++itr)
        {
            if (itr->second.getOrderToken() == id)
            {
                m_askOrders.erase(itr);
                return;
            }
        }
    }
}

bool Market::match(std::queue<Order> &orders)
{
    while (true)
    {
        if (!m_bidOrders.size() || !m_askOrders.size())
        {
            return orders.size() != 0;
        }

        BidOrders::iterator iBid = m_bidOrders.begin();
        AskOrders::iterator iAsk = m_askOrders.begin();

        if (iBid->second.getPrice() >= iAsk->second.getPrice())
        {
            Order &bid = iBid->second;
            Order &ask = iAsk->second;
            match(bid, ask);
            orders.push(bid);
            orders.push(ask);

            if (bid.isClosed())
                m_bidOrders.erase(iBid);
            if (ask.isClosed())
                m_askOrders.erase(iAsk);
        }
        else
        {
            return orders.size() != 0;
        }
    }
}

Order &Market::find(Order::Side side, std::string id)
{
    if (side == Order::buy)
    {
        BidOrders::iterator itr;
        for (itr = m_bidOrders.begin(); itr != m_bidOrders.end(); ++itr)
            if (itr->second.getOrderToken() == id)
                return itr->second;
    }
    else if (side == Order::sell)
    {
        AskOrders::iterator itr;
        for (itr = m_askOrders.begin(); itr != m_askOrders.end(); ++itr)
            if (itr->second.getOrderToken() == id)
                return itr->second;
    }
    throw std::exception();
}

void Market::match(Order& bid, Order& ask) {
    double price = ask.getPrice();
    long quantity = 0;

    if (bid.getOpenQuantity() > ask.getOpenQuantity())
        quantity = ask.getOpenQuantity();
    else
        quantity = bid.getOpenQuantity();

    bid.execute( price, quantity );
    ask.execute( price, quantity );
}

void Market::display() const
{
    BidOrders::const_iterator iBid;
    AskOrders::const_iterator iAsk;

    std::cout << "BIDS:" << std::endl;
    std::cout << "-----" << std::endl << std::endl;
    for ( iBid = m_bidOrders.begin(); iBid != m_bidOrders.end(); ++iBid )
        std::cout << iBid->second << std::endl;

    std::cout << std::endl << std::endl;

    std::cout << "ASKS:" << std::endl;
    std::cout << "-----" << std::endl << std::endl;
    for ( iAsk = m_askOrders.begin(); iAsk != m_askOrders.end(); ++iAsk )
        std::cout << iAsk->second << std::endl;
}