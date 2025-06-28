#ifndef __NANONET_H__
#define __NANONET_H__

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/if_ether.h>
#include <linux/jhash.h>
#include <linux/atomic.h>

#define ATOMIC64_INIT(i) { (i) }

// Ethernet header
struct ull_ethhdr {
    unsigned char h_dest[ETH_ALEN];
    unsigned char h_source[ETH_ALEN];
    __be16 h_proto;
} __packed;

// IP header (simplified)
struct ull_iphdr {
    __u8 version_ihl;
    __u8 tos;
    __be16 tot_len;
    __be16 id;
    __be16 frag_off;
    __u8 ttl;
    __u8 protocol;
    __sum16 check;
    __be32 saddr;
    __be32 daddr;
} __packed;

// TCP header (simplified)
struct ull_tcphdr {
    __be16 source;
    __be16 dest;
    __be32 seq;
    __be32 ack_seq;
    __u16 res1:4, doff:4, fin:1, syn:1, rst:1, psh:1, ack:1, urg:1, ece:1, cwr:1;
    __be16 window;
    __sum16 check;
    __be16 urg_ptr;
} __packed;

// UDP header
struct ull_udphdr {
    __be16 source;
    __be16 dest;
    __be16 len;
    __sum16 check;
} __packed;

// TCP connection state
struct ull_tcp_conn {
    __be32 src_ip;
    __be32 dst_ip;
    __be16 src_port;
    __be16 dst_port;
    __be32 seq_num;
    __be32 ack_num;
    u8 state;               // 0: Closed, 1: Syn-Sent, 2: Established, etc.
    u64 last_seen;
    struct hlist_node hash_node;
};

// Configuration structure
struct ull_config {
    bool enabled;
    __be32 target_ip;
    __be16 target_port;
    __u8 protocol;          // IPPROTO_TCP or IPPROTO_UDP
    __be32 response_ip;
    __be16 response_port;
    __be32 seq_num;
    __u8 application_logic_type;
    bool multicast;
    __be32 multicast_group;
};

// Statistics structure
struct ull_stats {
    atomic64_t packets_processed;
    atomic64_t packets_bypassed;
    atomic64_t responses_sent;
    atomic64_t errors;
    u64 last_process_time_ns;
    u64 min_process_time_ns;
    u64 max_process_time_ns;
    u64 avg_process_time_ns;
    atomic64_t connections_active;
    atomic64_t connections_dropped;
};

// Function prototypes
int ull_parse_packet(struct sk_buff *skb, struct ull_iphdr **ip_hdr, struct ull_tcphdr **tcp_hdr, struct ull_udphdr **udp_hdr,
                    void **payload, int *payload_len);
__sum16 nanonet_compute_checksum(void *data, int len);
int nanonet_validate_packet(struct sk_buff *skb, struct ull_iphdr *ip_hdr);
int nanonet_check_permissions(void);
int nanonet_validate_config(struct ull_config *config);
int nanonet_track_tcp_connection(struct ull_iphdr *ip_hdr, struct ull_tcphdr *tcp_hdr);
void nanonet_clear_tcp_connections(void);
void nanonet_log_error(const char *fmt, ...);
int nanonet_process_application_logic(void *payload, int payload_len, struct ull_config *config);
int nanonet_send_response(struct sk_buff *orig_skb, void *response_data, int response_len,
                         struct ull_config *config);
void nanonet_set_cpu_affinity(void);
int nanonet_parse_packet_optimized(struct sk_buff *skb, struct ull_iphdr **ip_hdr,
                                  void **payload, int *payload_len);
int nanonet_init_response_pool(void);
void nanonet_cleanup_response_pool(void);
struct sk_buff *nanonet_get_response_skb(void);
int nanonet_raw_send(struct sk_buff *skb, struct net_device *dev);
int nanonet_control_init(void);
void nanonet_control_cleanup(void);
int nanonet_debug_init(void);
void nanonet_debug_cleanup(void);

#endif /* __NANONET_H__ */