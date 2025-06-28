#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/time.h>
#include "../include/nanonet.h"

struct market_data {
    char symbol[8];
    u32 price;          // Price in cents
    u32 quantity;
    u64 timestamp;
} __packed;

struct trading_order {
    char symbol[8];
    u32 price;
    u32 quantity;
    u8 side;            // 0 = buy, 1 = sell
    u64 timestamp;
    char clOrdId[16];   // Client order ID
} __packed;

static u64 get_timestamp_ns(void) {
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int process_market_data(void *payload, int payload_len, struct ull_config *config, void **response_data, int *response_len) {
   
    struct market_data *market;
    struct trading_order *order;

    if (!payload || payload_len < sizeof(struct market_data)) {
        nanonet_log_error("Invalid market data size: %d", payload_len);
        return -EINVAL;
    }

    market = (struct market_data *)payload;
    if (!market) {
        nanonet_log_error("Null market data pointer");
        return -EINVAL;
    }

    if (market->price < 10000) {            // $100.00 threshold
        order = kmalloc(sizeof(struct trading_order), GFP_ATOMIC);
        if (!order) {
            nanonet_log_error("Failed to allocate memory for order");
            return -ENOMEM;
        }

        memcpy(order->symbol, market->symbol, 8);
        order->price = market->price + 1; // Bid 1 cent higher
        order->quantity = 100;
        order->side = 0;                  // Buy
        order->timestamp = get_timestamp_ns();
        snprintf(order->clOrdId, sizeof(order->clOrdId), "ORD%llu", order->timestamp);

        *response_data = order;
        *response_len = sizeof(struct trading_order);

        return 1;
    }

    return 0;
}

int nanonet_process_application_logic(void *payload, int payload_len, struct ull_config *config) {
    void *response_data = NULL;
    int response_len = 0;
    int result = 0;

    if (!payload || payload_len <= 0) {
        return 0;
    }

    switch (config->application_logic_type) {
        case 0:                         // Market data processing
            result = process_market_data(payload, payload_len, config,
                                        &response_data, &response_len);
            break;

        default:
            nanonet_log_error("Unknown application logic type: %d", config->application_logic_type);
            return -EINVAL;
    }

    if (result > 0 && response_data) {
        result = nanonet_send_response(NULL, response_data, response_len, config);
        kfree(response_data);
        if (result < 0) {
            nanonet_log_error("Failed to send response: %d", result);
        }
    }

    return result;
}