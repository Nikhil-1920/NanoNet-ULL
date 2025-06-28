#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/time.h>
#include <linux/netdevice.h>
#include <net/ip.h>
#include "../include/nanonet.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikhil Singh");
MODULE_DESCRIPTION("NanoNet: Ultra-Low Latency Networking Stack");
MODULE_VERSION("1.0");

// Global configuration and statistics
struct ull_config global_config = {
    .enabled = false,
    .target_ip = 0,
    .target_port = 0,
    .protocol = IPPROTO_UDP,
    .response_ip = 0,
    .response_port = 0,
    .application_logic_type = 0,
    .multicast = false,
    .multicast_group = 0,
};

struct ull_stats global_stats = {
    .packets_processed = ATOMIC64_INIT(0),
    .packets_bypassed = ATOMIC64_INIT(0),
    .responses_sent = ATOMIC64_INIT(0),
    .errors = ATOMIC64_INIT(0),
    .min_process_time_ns = UINT64_MAX,
    .max_process_time_ns = 0,
    .avg_process_time_ns = 0,
};

static struct nf_hook_ops nfho_in;
static struct net_device *target_dev = NULL;

static inline u64 get_timestamp_ns(void) {
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int init_multicast(void) {
    if (!global_config.multicast || !target_dev) {
        return 0;
    }

    struct ip_mreqn mreq = {0};
    mreq.imr_multiaddr.s_addr = global_config.multicast_group;
    mreq.imr_ifindex = target_dev->ifindex;

    return ip_mc_join_group(&init_net, &mreq);
}

static unsigned int nanonet_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {
    struct ull_iphdr *ip_hdr;
    struct ull_tcphdr *tcp_hdr = NULL;
    struct ull_udphdr *udp_hdr = NULL;
    void *payload;
    int payload_len;
    u64 start_time, end_time, process_time;
    int result;

    if (!global_config.enabled || !skb->dev) {
        atomic64_inc(&global_stats.packets_bypassed);
        return NF_ACCEPT;
    }

    start_time = get_timestamp_ns();

    result = ull_parse_packet(skb, &ip_hdr, &tcp_hdr, &udp_hdr, &payload, &payload_len);
    if (result < 0) {
        atomic64_inc(&global_stats.errors);
        nanonet_log_error("Packet parsing failed: %d", result);
        return NF_ACCEPT;
    }

    if (nanonet_validate_packet(skb, ip_hdr) < 0) {
        atomic64_inc(&global_stats.errors);
        return NF_ACCEPT;
    }

    if (ip_hdr->daddr != global_config.target_ip &&
        (!global_config.multicast || ip_hdr->daddr != global_config.multicast_group)) {
        atomic64_inc(&global_stats.packets_bypassed);
        return NF_ACCEPT;
    }

    if (global_config.protocol == IPPROTO_TCP && tcp_hdr) {
        if (tcp_hdr->dest != global_config.target_port) {
            atomic64_inc(&global_stats.packets_bypassed);
            return NF_ACCEPT;
        }
        result = nanonet_track_tcp_connection(ip_hdr, tcp_hdr);
        if (result < 0) {
            atomic64_inc(&global_stats.errors);
            return NF_ACCEPT;
        }
    } else if (global_config.protocol == IPPROTO_UDP && udp_hdr) {
        if (udp_hdr->dest != global_config.target_port) {
            atomic64_inc(&global_stats.packets_bypassed);
            return NF_ACCEPT;
        }
    }

    result = nanonet_process_application_logic(payload, payload_len, &global_config);
    if (result < 0) {
        atomic64_inc(&global_stats.errors);
        nanonet_log_error("Application logic failed: %d", result);
    } else if (result > 0) {
        atomic64_inc(&global_stats.responses_sent);
    }

    atomic64_inc(&global_stats.packets_processed);

    end_time = get_timestamp_ns();
    process_time = end_time - start_time;

    global_stats.last_process_time_ns = process_time;
    if (process_time < global_stats.min_process_time_ns) {
        global_stats.min_process_time_ns = process_time;
    }
    if (process_time > global_stats.max_process_time_ns) {
        global_stats.max_process_time_ns = process_time;
    }
    global_stats.avg_process_time_ns =
        (global_stats.avg_process_time_ns * (atomic64_read(&global_stats.packets_processed) - 1) + process_time) /
        atomic64_read(&global_stats.packets_processed);

    trace_nanonet_packet_processed(ip_hdr->saddr, tcp_hdr ? ntohs(tcp_hdr->source) : ntohs(udp_hdr->source),
                                    ip_hdr->daddr, tcp_hdr ? ntohs(tcp_hdr->dest) : ntohs(udp_hdr->dest),
                                    process_time, result );

    return NF_STOLEN;
}

extern int nanonet_control_init(void);
extern void nanonet_control_cleanup(void);
extern int nanonet_debug_init(void);
extern void nanonet_debug_cleanup(void);
extern int nanonet_init_response_pool(void);
extern void nanonet_cleanup_response_pool(void);

static int __init nanonet_init(void) {
    int result;

    printk(KERN_INFO "NANONET: Initializing ultra-low latency networking module\n");

    target_dev = dev_get_by_name(&init_net, "eth0");
    if (!target_dev) {
        printk(KERN_ERR "NANONET: Failed to find network device\n");
        return -ENODEV;
    }

    result = nanonet_init_response_pool();
    if (result < 0) {
        printk(KERN_ERR "NANONET: Failed to initialize response pool\n");
        dev_put(target_dev);
        return result;
    }

    result = nanonet_control_init();
    if (result < 0) {
        printk(KERN_ERR "NANONET: Failed to initialize control interface\n");
        nanonet_cleanup_response_pool();
        dev_put(target_dev);
        return result;
    }

    result = nanonet_debug_init();
    if (result < 0) {
        printk(KERN_ERR "NANONET: Failed to initialize debug interface\n");
        nanonet_control_cleanup();
        nanonet_cleanup_response_pool();
        dev_put(target_dev);
        return result;
    }

    result = init_multicast();
    if (result < 0) {
        printk(KERN_ERR "NANONET: Failed to join multicast group\n");
        nanonet_debug_cleanup();
        nanonet_control_cleanup();
        nanonet_cleanup_response_pool();
        dev_put(target_dev);
        return result;
    }

    nfho_in.hook = nanonet_hook;
    nfho_in.hooknum = NF_INET_PRE_ROUTING;
    nfho_in.pf = PF_INET;
    nfho_in.priority = NF_IP_PRI_FIRST;

    result = nf_register_net_hook(&init_net, &nfho_in);
    if (result < 0) {
        printk(KERN_ERR "NANONET: Failed to register netfilter hook\n");
        nanonet_debug_cleanup();
        nanonet_control_cleanup();
        nanonet_cleanup_response_pool();
        dev_put(target_dev);
        return result;
    }

    printk(KERN_INFO "NANONET: Module loaded successfully\n");
    printk(KERN_INFO "NANONET: Use /dev/nanonet for control or check /proc/nanonet for status\n");

    return 0;
}

static void __exit nanonet_exit(void) {
    printk(KERN_INFO "NANONET: Unloading module\n");

    nf_unregister_net_hook(&init_net, &nfho_in);
    nanonet_debug_cleanup();
    nanonet_control_cleanup();
    nanonet_cleanup_response_pool();
    if (target_dev) {
        dev_put(target_dev);
    }

    printk(KERN_INFO "NANONET: Module unloaded successfully\n");
}

module_init(nanonet_init);
module_exit(nanonet_exit);