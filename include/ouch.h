#include <stdint.h>

typedef struct order_message_t {
    char    type;
    char    order_token[14];
    char    indicator;
    int     shares;
    char    stock[8];
    int     price;
    int     tif; /* Time in force */
    char    firm[4];
    char    display;
    char    capacity;
    char    ise; /* Intermarket Sweep Eligibility */
    int     min_quantity;
    char    cross_type;
    char    customer_type;
} order_message;

typedef struct cancel_order_message_t {
    char    type;
    char    order_token[14];
    int     shares;
} cancel_order_message;