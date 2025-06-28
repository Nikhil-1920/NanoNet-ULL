#include <linux/kernel.h>
#include <linux/ratelimit.h>
#include <linux/jhash.h>
#include <linux/capability.h>
#include <linux/jiffies.h>
#include "../include/nanonet.h"

static DEFINE_RATELIMIT_STATE(nanonet_ratelimit, 5 * HZ, 20);

#define CONN_HASH_SIZE 1024
static struct hlist_head connection_hash[CONN_HASH_SIZE];
static DEFINE_SPINLOCK(conn_hash_lock);

static u32 nanonet_conn_hash(struct ull_tcp_conn *conn) {
    return jhash_3words(conn->src_ip, conn->dst_ip, (conn->src_port << 16) | conn->dst_port, 0) % CONN_HASH_SIZE;
}

int nanonet_track_tcp_connection(struct ull_iphdr *ip_hdr, struct ull_tcphdr *tcp_hdr) {
    struct ull_tcp_conn *conn;
    u32 hash;
    bool found = false;
    unsigned long flags;

    hash = jhash_3words(ip_hdr->saddr, ip_hdr->daddr,
                        (ntohs(tcp_hdr->source) << 16) | ntohs(tcp_hdr->dest), 0) % CONN_HASH_SIZE;

    spin_lock_irqsave(&conn_hash_lock, flags);
    hlist_for_each_entry(conn, &connection_hash[hash], hash_node) {
        if (conn->src_ip == ip_hdr->saddr && conn->dst_ip == ip_hdr->daddr &&
            conn->src_port == tcp_hdr->source && conn->dst_port == tcp_hdr->dest) {
            found = true;
            conn->last_seen = jiffies;
            if (tcp_hdr->syn && !tcp_hdr->ack) {
                conn->state = 1; // Syn-Sent
            } else if (tcp_hdr->syn && tcp_hdr->ack) {
                conn->state = 2; // Established
                conn->seq_num = ntohl(tcp_hdr->seq);
                conn->ack_num = ntohl(tcp_hdr->ack_seq);
            }
            break;
        }
    }

    if (!found && tcp_hdr->syn && !tcp_hdr->ack) {
        conn = kmalloc(sizeof(*conn), GFP_ATOMIC);
        if (!conn) {
            spin_unlock_irqrestore(&conn_hash_lock, flags);
            nanonet_log_error("Failed to allocate memory for TCP connection");
            return -ENOMEM;
        }
        conn->src_ip = ip_hdr->saddr;
        conn->dst_ip = ip_hdr->daddr;
        conn->src_port = tcp_hdr->source;
        conn->dst_port = tcp_hdr->dest;
        conn->state = 1;
        conn->seq_num = ntohl(tcp_hdr->seq);
        conn->ack_num = 0;
        conn->last_seen = jiffies;
        hlist_add_head(&conn->hash_node, &connection_hash[hash]);
        atomic64_inc(&global_stats.connections_active);
    }

    spin_unlock_irqrestore(&conn_hash_lock, flags);
    return found || (tcp_hdr->syn && !tcp_hdr->ack) ? 0 : -EINVAL;
}

void nanonet_clear_tcp_connections(void) {
    int i;
    struct ull_tcp_conn *conn, *tmp;
    unsigned long flags;

    spin_lock_irqsave(&conn_hash_lock, flags);
    for (i = 0; i < CONN_HASH_SIZE; i++) {
        hlist_for_each_entry_safe(conn, tmp, &connection_hash[i], hash_node) {
            hlist_del(&conn->hash_node);
            kfree(conn);
            atomic64_dec(&global_stats.connections_active);
            atomic64_inc(&global_stats.connections_dropped);
        }
    }
    spin_unlock_irqrestore(&conn_hash_lock, flags);
}

int nanonet_validate_packet(struct sk_buff *skb, struct ull_iphdr *ip_hdr) {
    if (!__ratelimit(&nanonet_ratelimit)) {
        printk_ratelimited(KERN_WARNING "NANONET: Rate limit exceeded\n");
        return -EBUSY;
    }

    if (ip_hdr->saddr == 0 || ip_hdr->tot_len < sizeof(struct ull_iphdr)) {
        nanonet_log_error("Invalid packet: zero source IP or insufficient length");
        return -EINVAL;
    }

    return 0;
}

int nanonet_check_permissions(void) {
    return capable(CAP_NET_ADMIN) ? 0 : -EPERM;
}

int nanonet_validate_config(struct ull_config *config) {
    if (config->target_ip == 0 || config->response_ip == 0 ||
        config->target_port == 0 || config->response_port == 0) {
        nanonet_log_error("Invalid config: zero IP or port");
        return -EINVAL;
    }

    if (config->protocol != IPPROTO_TCP && config->protocol != IPPROTO_UDP) {
        nanonet_log_error("Invalid protocol: %d", config->protocol);
        return -EINVAL;
    }

    if (config->multicast && config->multicast_group == 0) {
        nanonet_log_error("Invalid multicast group");
        return -EINVAL;
    }

    return 0;
}