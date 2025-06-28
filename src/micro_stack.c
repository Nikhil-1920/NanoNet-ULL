#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/if_ether.h>
#include <linux/time.h>
#include "../include/nanonet.h"

static inline u64 get_timestamp_ns(void) {
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static inline int validate_checksum(void *data, int len, __sum16 check) {
    __sum16 calc_check = nanonet_compute_checksum(data, len);
    return calc_check == check ? 0 : -EINVAL;
}

int ull_parse_packet(struct sk_buff *skb, struct ull_iphdr **ip_hdr, struct ull_tcphdr **tcp_hdr, struct ull_udphdr **udp_hdr,
                     void **payload, int *payload_len) {
                        
    struct ull_ethhdr *eth_hdr;
    struct ull_iphdr *ip;
    int ip_hdr_len;
    void *transport_hdr;
    int transport_hdr_len = 0;

    *ip_hdr = NULL;
    *tcp_hdr = NULL;
    *udp_hdr = NULL;
    *payload = NULL;
    *payload_len = 0;

    if (unlikely(skb->len < sizeof(struct ull_ethhdr))) {
        return -EINVAL;
    }

    eth_hdr = (struct ull_ethhdr *)skb->data;
    if (unlikely(ntohs(eth_hdr->h_proto) != ETH_P_IP)) {
        return -EPROTONOSUPPORT;
    }

    if (unlikely(skb->len < sizeof(struct ull_ethhdr) + sizeof(struct ull_iphdr))) {
        return -EINVAL;
    }

    ip = (struct ull_iphdr *)(skb->data + sizeof(struct ull_ethhdr));
    *ip_hdr = ip;

    if (unlikely((ip->version_ihl >> 4) != 4)) {
        return -EPROTONOSUPPORT;
    }

    ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
    if (unlikely(ip_hdr_len < sizeof(struct ull_iphdr))) {
        return -EINVAL;
    }

    if (validate_checksum(ip, ip_hdr_len, ip->check) < 0) {
        return -EINVAL;
    }

    transport_hdr = (void *)ip + ip_hdr_len;

    switch (ip->protocol) {
        case IPPROTO_TCP:
            if (unlikely(skb->len < sizeof(struct ull_ethhdr) + ip_hdr_len + sizeof(struct ull_tcphdr))) {
                return -EINVAL;
            }
            *tcp_hdr = (struct ull_tcphdr *)transport_hdr;
            transport_hdr_len = (*tcp_hdr)->doff * 4;
            if (unlikely(transport_hdr_len < sizeof(struct ull_tcphdr))) {
                return -EINVAL;
            }
            break;

        case IPPROTO_UDP:
            if (unlikely(skb->len < sizeof(struct ull_ethhdr) + ip_hdr_len + sizeof(struct ull_udphdr))) {
                return -EINVAL;
            }
            *udp_hdr = (struct ull_udphdr *)transport_hdr;
            transport_hdr_len = sizeof(struct ull_udphdr);
            if (udp_hdr->check && validate_checksum(transport_hdr, ntohs(udp_hdr->len), udp_hdr->check) < 0) {
                return -EINVAL;
            }
            break;

        default:
            return -EPROTONOSUPPORT;
    }

    *payload = transport_hdr + transport_hdr_len;
    *payload_len = skb->len - sizeof(struct ull_ethhdr) - ip_hdr_len - transport_hdr_len;

    if (unlikely(*payload_len < 0)) {
        *payload_len = 0;
        *payload = NULL;
    }

    return 0;
}

__sum16 nanonet_compute_checksum(void *data, int len) {
    u32 sum = 0;
    u16 *ptr = (u16 *)data;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(u8 *)ptr << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (__sum16)~sum;
}