#pragma once

#include <string>
#include <iomanip>
#include <ostream>

class Order {
    friend std::ostream& operator<<(std::ostream&, const Order&);

    public:
        enum Side { buy, sell };
        enum Type { market, limit };

        Order( const std::string& orderToken, const std::string& symbol,
                const std::string& firm,
                Side side, Type type, double price, long quantity )
        : m_orderToken( orderToken ), m_symbol( symbol ), m_firm( firm )
        , m_side( side ), m_type( type ), m_price( price ),
        m_quantity( quantity )
        {
            m_openQuantity = m_quantity;
            m_executedQuantity = 0;
            m_avgExecutedPrice = 0;
            m_lastExecutedPrice = 0;
            m_lastExecutedQuantity = 0;
        }

        const std::string& getOrderToken() const { return m_orderToken; }
        const std::string& getSymbol() const { return m_symbol; }
        const std::string& getOwner() const { return m_firm; }
        Side getSide() const { return m_side; }
        Type getType() const { return m_type; }
        double getPrice() const { return m_price; }
        long getQuantity() const { return m_quantity; }

        long getOpenQuantity() const { return m_openQuantity; }
        long getExecutedQuantity() const { return m_executedQuantity; }
        double getAvgExecutedPrice() const { return m_avgExecutedPrice; }
        double getLastExecutedPrice() const { return m_lastExecutedPrice; }
        long getLastExecutedQuantity() const { return m_lastExecutedQuantity; }

        bool isFilled() const { return m_quantity == m_executedQuantity; }
        bool isClosed() const { return m_openQuantity == 0; }

        void execute( double price, long quantity )
        {
            m_avgExecutedPrice =
            ( ( quantity * price ) + ( m_avgExecutedPrice * m_executedQuantity ) )
            / ( quantity + m_executedQuantity );

            m_openQuantity -= quantity;
            m_executedQuantity += quantity;
            m_lastExecutedPrice = price;
            m_lastExecutedQuantity = quantity;
        }
        void cancel()
        {
            m_openQuantity = 0;
        }

    private:
        std::string m_orderToken; //firm
        std::string m_symbol; // stock
        std::string m_firm;
        Side m_side;
        Type m_type;
        int m_price;
        int m_quantity;

        long m_openQuantity;
        long m_executedQuantity;
        double m_avgExecutedPrice;
        double m_lastExecutedPrice;
        long m_lastExecutedQuantity;
};

inline std::ostream& operator<<( std::ostream& ostream, const Order& order )
{
  return ostream
         << "ID: " << std::setw( 10 ) << "," << order.getOrderToken()
         << " OWNER: " << std::setw( 10 ) << "," << order.getOwner()
         << " PRICE: " << std::setw( 10 ) << "," << order.getPrice()
         << " QUANTITY: " << std::setw( 10 ) << "," << order.getQuantity();
}