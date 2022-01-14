#pragma once
#include <stdint.h>

typedef struct order_message_t
{
    char type;
    char order_token[14];
    char indicator;
    long shares;
    char stock[8];
    long price;
    long tif; /* Time in force */
    char firm[4];
    char display;
    char capacity;
    char ise; /* long ermarket Sweep Eligibility */
    long min_quantity;
    char cross_type;
    char customer_type;
} order_message;

typedef struct accept_order_message_t
{
    char type;
    long timestamp;
    char order_token[14];
    char indicator;
    long shares;
    char stock[8];
    long price;
    long tif; /* Time in force */
    char firm[4];
    char display;
    long order_reference_number;
    char capacity;
    char ise; /* long ermarket Sweep Eligibility */
    long min_quantity;
    char cross_type;
    char order_state;
    char bbo_weight;
} accept_order_message;

typedef struct reject_order_message_t
{
    char type;
    long timestamp;
    char order_token[14];
    char reason;
} reject_order_message;

typedef struct executed_order_message_t
{
    char type;
    long timestamp;
    char order_token[14];
    long shares;
    long execution_price;
    char liquidity_flag;
    long match_number;
} executed_order_message;

typedef struct modify_order_message_t
{
    char type;
    char order_token[14];
    char indicator;
    long shares;
} modify_order_message;

typedef struct order_modified_message_t
{
    char type;
    long timestamp;
    char order_token[14];
    char indicator;
    long shares;
} order_modified_message_t;

typedef struct cancel_order_message_t
{
    char type;
    char order_token[14];
    long shares;
} cancel_order_message;

typedef struct canceled_order_message_t
{
    char type;
    long timestamp;
    char order_token[14];
    long shares;
    char reason;
} canceled_order_message;
