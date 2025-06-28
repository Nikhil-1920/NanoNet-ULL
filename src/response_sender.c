#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/route.h>
#include <net/ip.h>
#include <net/route.h>
#include "../include/nanonet.h"

static struct sk_buff *nanonet_create_response_packet(struct sk_buff *orig_skb,
                                                    void *response_data,
                                                    int response_len,
                                                    struct ull_config *config) {
    struct sk_buff *new_skb;
    struct ull_ethhdr *orig_eth = NULL, *new_eth;
    struct ull_iphdr *orig_ip = NULL, *new_ip;
    struct ull_tcphdr *orig_tcp = NULL, *new_tcp;
    struct ull_udphdr *orig_udp = NULL, *new_udp;
    int total_len;
    int ip_hdr_len = sizeof(struct ull_iphdr);
    int transport_hdr_len;
    void *payload_ptr;
    struct net_device *dev = NULL;

    if (config->response_ip == 0 || config->response_port == 0) {
        nanonet_log_error("Invalid response IP or port");
        return NULL;
    }

    if (config->protocol == IPPROTO_TCP) {
        transport_hdr_len = sizeof(struct ull_tcphdr);
    } else if (config->protocol == IPPROTO_UDP) {
        transport_hdr_len = sizeof(struct ull_udphdr);
    } else {
        nanonet_log_error("Unsupported protocol: %d", config->protocol);
        return NULL;
    }

    total_len = sizeof(struct ull_ethhdr) + ip_hdr_len + transport_hdr_len + response_len;

    new_skb = nanonet_get_response_skb();
    if (!new_skb) {
        new_skb = alloc_skb(total_len + NET_IP_ALIGN, GFP_ATOMIC);
        if (!new_skb) {
            nanonet_log_error("Failed to allocate response skb");
            return NULL;
        }
    }

    skb_reserve(new_skb, NET_IP_ALIGN);

    if (orig_skb) {
        orig_eth = (struct ull_ethhdr *)orig_skb->data;
        orig_ip = (struct ull_iphdr *)(orig_skb->data + sizeof(struct ull_ethhdr));
    }

    new_eth = (struct ull_ethhdr *)skb_put(new_skb, sizeof(struct ull_ethhdr));
    if (orig_eth) {
        memcpy(new_eth->h_dest, orig_eth->h_source, ETH_ALEN);
        memcpy(new_eth->h_source, orig_eth->h_dest, ETH_ALEN);
    } else {
        memset(new_eth->h_dest, 0, ETH_ALEN);
        memset(new_eth->h_source, 0, ETH_ALEN);
    }
    new_eth->h_proto = htons(ETH_P_IP);

    new_ip = (struct ull_iphdr *)skb_put(new_skb, ip_hdr_len);
    memset(new_ip, 0, ip_hdr_len);
    new_ip->version_ihl = 0x45;
    new_ip->tos = 0;
    new_ip->tot_len = htons(ip_hdr_len + transport_hdr_len + response_len);
    new_ip->id = 0;
    new_ip->frag_off = htons(IP_DF);
    new_ip->ttl = 64;
    new_ip->protocol = config->protocol;
    new_ip->saddr = config->response_ip;
    new_ip->daddr = orig_ip ? orig_ip->saddr : config->target_ip;
    new_ip->check = 0;
    new_ip->check = nanonet_compute_checksum(new_ip, ip_hdr_len);

    if (config->protocol == IPPROTO_TCP) {
        if (orig_skb) {
            orig_tcp = (struct ull_tcphdr *)((void *)orig_ip + ((orig_ip->version_ihl & 0x0F) * 4));
        }

        new_tcp = (struct ull_tcphdr *)skb_put(new_skb, transport_hdr_len);
        memset(new_tcp, 0, transport_hdr_len);
        new_tcp->source = config->response_port;
        new_tcp->dest = orig_tcp ? orig_tcp->source : config->target_port;
        new_tcp->seq = htonl(config->seq_num);
        new_tcp->ack_seq = orig_tcp ? htonl(ntohl(orig_tcp->seq) + 1) : 0;
        new_tcp->doff = sizeof(struct ull_tcphdr) / 4;
        new_tcp->psh = 1;
        new_tcp->ack = orig_tcp ? 1 : 0;
        new_tcp->window = htons(65535);
        new_tcp->check = 0;
    } else if (config->protocol == IPPROTO_UDP) {
        if (orig_skb) {
            orig_udp = (struct ull_udphdr *)((void *)orig_ip + ((orig_ip->version_ihl & 0x0F) * 4));
        }
        new_udp = (struct ull_udphdr *)skb_put(new_skb, transport_hdr_len);
        new_udp->source = config->response_port;
        new_udp->dest = orig_udp ? orig_udp->source : config->target_port;
        new_udp->len = htons(transport_hdr_len + response_len);
        new_udp->check = 0;
    }

    payload_ptr = skb_put(new_skb, response_len);
    memcpy(payload_ptr, response_data, response_len);

    dev = orig_skb ? orig_skb->dev : dev_get_by_name(&init_net, "eth0");
    if (!dev) {
        kfree_skb(new_skb);
        nanonet_log_error("Failed to get network device");
        return NULL;
    }

    new_skb->dev = dev;
    new_skb->protocol = htons(ETH_P_IP);

    return new_skb;
}

int nanonet_send_response(struct sk_buff *orig_skb, void *response_data, int response_len, struct ull_config *config) {
    struct sk_buff *response_skb;
    int result;

    if (!response_data || response_len <= 0) {
        nanonet_log_error("Invalid response data or length");
        return -EINVAL;
    }

    response_skb = nanonet_create_response_packet(orig_skb, response_data, response_len, config);
    if (!response_skb) {
        return -ENOMEM;
    }

    result = nanonet_raw_send(response_skb, response_skb->dev);
    if (result != NET_XMIT_SUCCESS) {
        kfree_skb(response_skb);
        nanonet_log_error("Failed to send response: %d", result);
        return -EIO;
    }

    config->seq_num += response_len;
    return 0;
}