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

typedef struct accept_order_message_t {
    char    type;
    int     timestamp;
    char    order_token[14];
    char    indicator;
    int     shares;
    char    stock[8];
    int     price;
    int     tif; /* Time in force */
    char    firm[4];
    char    display;
    int     order_reference_number;
    char    capacity;
    char    ise; /* Intermarket Sweep Eligibility */
    int     min_quantity;
    char    cross_type;
    char    order_state;
    char    bbo_weight;
} accept_order_message;

typedef struct reject_order_message_t {
    char    type;
    int     timestamp;
    char    order_token[14];
    char    reason;
} reject_order_message;

typedef struct executed_order_message_t {
    char    type;
    int     timestamp;
    char    order_token[14];
    int     shares;
    int     execution_price;
    char    liquidity_flag;
    int     match_number;
} executed_order_message_t;

typedef struct modify_order_message_t {
    char    type;
    char    order_token[14];
    char    indicator;
    int     shares;
} modify_order_message;

typedef struct order_modified_message_t {
    char    type;
    int     timestamp;
    char    order_token[14];
    char    indicator;
    int     shares;
} order_modified_message_t;

typedef struct cancel_order_message_t {
    char    type;
    char    order_token[14];
    int     shares;
} cancel_order_message;

typedef struct canceled_order_message_t {
    char    type;
    int     timestamp;
    char    order_token[14];
    int     shares;
    char    reason;
} canceled_order_message;

