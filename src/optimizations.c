#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/prefetch.h>
#include <linux/numa.h>
#include "../include/nanonet.h"

DEFINE_PER_CPU(struct ull_stats, per_cpu_stats);
#define RING_BUFFER_SIZE 1024

struct packet_ring_buffer {
    volatile unsigned long head;
    volatile unsigned long tail;
    struct sk_buff *packets[RING_BUFFER_SIZE];
} ____cacheline_aligned;

static DEFINE_PER_CPU(struct packet_ring_buffer, packet_ring);

void nanonet_set_cpu_affinity(void) {
    struct cpumask mask;
    cpumask_clear(&mask);
    cpumask_set_cpu(0, &mask);
    set_cpus_allowed_ptr(current, &mask);
}

static inline int nanonet_parse_packet_optimized(struct sk_buff *skb, struct ull_iphdr **ip_hdr,
                                                void **payload, int *payload_len) {

    void *data = skb->data;
    struct ull_ethhdr *eth_hdr;
    struct ull_iphdr *ip;
    int ip_hdr_len;
    void *transport_hdr;
    int transport_hdr_len = 0;

    prefetch(data + 64);
    prefetch(data + 128);

    if (unlikely(skb->len < sizeof(struct ull_ethhdr) + sizeof(struct ull_iphdr))) {
        nanonet_log_error("Invalid packet size: %d", skb->len);
        return -EINVAL;
    }

    eth_hdr = (struct ull_ethhdr *)data;
    if (unlikely(eth_hdr->h_proto != htons(ETH_P_IP))) {
        nanonet_log_error("Unsupported protocol: %x", ntohs(eth_hdr->h_proto));
        return -EPROTONOSUPPORT;
    }

    ip = (struct ull_iphdr *)(data + sizeof(struct ull_ethhdr));
    *ip_hdr = ip;

    ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
    if (unlikely(ip_hdr_len < sizeof(struct ull_iphdr))) {
        nanonet_log_error("Invalid IP header length: %d", ip_hdr_len);
        return -EINVAL;
    }

    transport_hdr = (void *)ip + ip_hdr_len;
    if (ip->protocol == IPPROTO_TCP) {
        if (unlikely(skb->len < sizeof(struct ull_ethhdr) + ip_hdr_len + sizeof(struct ull_tcphdr))) {
            nanonet_log_error("Invalid TCP packet size: %d", skb->len);
            return -EINVAL;
        }
        transport_hdr_len = ((struct ull_tcphdr *)transport_hdr)->doff * 4;
        if (unlikely(transport_hdr_len < sizeof(struct ull_tcphdr))) {
            nanonet_log_error("Invalid TCP header length: %d", transport_hdr_len);
            return -EINVAL;
        }
    } else if (ip->protocol == IPPROTO_UDP) {
        if (unlikely(skb->len < sizeof(struct ull_ethhdr) + ip_hdr_len + sizeof(struct ull_udphdr))) {
            nanonet_log_error("Invalid UDP packet size: %d", skb->len);
            return -EINVAL;
        }
        transport_hdr_len = sizeof(struct ull_udphdr);
    } else {
        nanonet_log_error("Unsupported transport protocol: %d", ip->protocol);
        return -EPROTONOSUPPORT;
    }

    *payload = transport_hdr + transport_hdr_len;
    *payload_len = skb->len - sizeof(struct ull_ethhdr) - ip_hdr_len - transport_hdr_len;

    if (unlikely(*payload_len < 0)) {
        *payload_len = 0;
        *payload = NULL;
        nanonet_log_error("Negative payload length calculated");
    }

    return 0;
}

#define RESPONSE_POOL_SIZE 256
static struct {
    struct sk_buff *pool[RESPONSE_POOL_SIZE];
    atomic_t head;
    atomic_t tail;
    spinlock_t lock;
} response_pool __aligned(PAGE_SIZE);

int nanonet_init_response_pool(void) {
    int i;
    unsigned long flags;

    spin_lock_init(&response_pool.lock);
    atomic_set(&response_pool.head, 0);
    atomic_set(&response_pool.tail, 0);

    for (i = 0; i < RESPONSE_POOL_SIZE; i++) {
        response_pool.pool[i] = alloc_skb(1500, GFP_KERNEL | __GFP_NUMA);
        if (!response_pool.pool[i]) {
            nanonet_log_error("Failed to allocate skb for response pool at index %d", i);
            while (--i >= 0) {
                kfree_skb(response_pool.pool[i]);
            }
            return -ENOMEM;
        }
    }

    return 0;
}

void nanonet_cleanup_response_pool(void) {
    int i;
    unsigned long flags;

    spin_lock_irqsave(&response_pool.lock, flags);
    for (i = 0; i < RESPONSE_POOL_SIZE; i++) {
        if (response_pool.pool[i]) {
            kfree_skb(response_pool.pool[i]);
            response_pool.pool[i] = NULL;
        }
    }

    atomic_set(&response_pool.head, 0);
    atomic_set(&response_pool.tail, 0);
    spin_unlock_irqrestore(&response_pool.lock, flags);
}

struct sk_buff *nanonet_get_response_skb(void) {
    unsigned long flags;
    int head, tail;

    spin_lock_irqsave(&response_pool.lock, flags);
    head = atomic_read(&response_pool.head);
    tail = atomic_read(&response_pool.tail);

    if (((head + 1) % RESPONSE_POOL_SIZE) == tail) {
        spin_unlock_irqrestore(&response_pool.lock, flags);
        nanonet_log_error("Response pool empty");
        return NULL;
    }

    struct sk_buff *skb = response_pool.pool[head];
    response_pool.pool[head] = NULL;    // Clear to prevent double-free
    atomic_set(&response_pool.head, (head + 1) % RESPONSE_POOL_SIZE);
    spin_unlock_irqrestore(&response_pool.lock, flags);

    return skb;
}

int nanonet_raw_send(struct sk_buff *skb, struct net_device *dev) {
    if (!skb || !dev) {
        if (skb) kfree_skb(skb);
        nanonet_log_error("Invalid skb or device for raw send");
        return -EINVAL;
    }

    skb->dev = dev;
    skb->protocol = htons(ETH_P_IP);
    return dev_queue_xmit(skb);
}