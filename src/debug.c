#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/trace_events.h>
#include <linux/jiffies.h>
#include <linux/ratelimit.h>
#include "../include/nanonet.h"

static struct dentry *nanonet_debug_dir;
static struct dentry *nanonet_debug_stats;

struct ull_debug_stats {
    u64 total_interrupts;
    u64 cache_misses;
    u64 memory_allocations;
    u64 queue_full_events;
    u64 checksum_errors;
    char last_error[256];
};

static struct ull_debug_stats debug_stats;
static DEFINE_RATELIMIT_STATE(nanonet_error_ratelimit, 5 * HZ, 20);

TRACE_EVENT(nanonet_packet_processed,
    TP_PROTO(u32 src_ip, u16 src_port, u32 dst_ip, u16 dst_port, u64 process_time_ns, int result),
    TP_ARGS(src_ip, src_port, dst_ip, dst_port, process_time_ns, result),
    TP_STRUCT__entry(
        __field(u32, src_ip)
        __field(u16, src_port)
        __field(u32, dst_ip)
        __field(u16, dst_port)
        __field(u64, process_time_ns)
        __field(int, result)
    ),
    TP_fast_assign(
        __entry->src_ip = src_ip;
        __entry->src_port = src_port;
        __entry->dst_ip = dst_ip;
        __entry->dst_port = dst_port;
        __entry->process_time_ns = process_time_ns;
        __entry->result = result;
    ),
    TP_printk("src=%pI4:%u dst=%pI4:%u time=%llu ns result=%d", &__entry->src_ip, __entry->src_port,
              &__entry->dst_ip, __entry->dst_port,
              __entry->process_time_ns, __entry->result)
);

static int nanonet_debug_stats_show(struct seq_file *m, void *v) {
    seq_printf(m, "NanoNet Debug Statistics\n");
    seq_printf(m, "============================\n");
    seq_printf(m, "Total Interrupts: %llu\n", debug_stats.total_interrupts);
    seq_printf(m, "Cache Misses: %llu\n", debug_stats.cache_misses);
    seq_printf(m, "Memory Allocations: %llu\n", debug_stats.memory_allocations);
    seq_printf(m, "Queue Full Events: %llu\n", debug_stats.queue_full_events);
    seq_printf(m, "Checksum Errors: %llu\n", debug_stats.checksum_errors);
    seq_printf(m, "Last Error: %s\n", debug_stats.last_error);

    return 0;
}

static int nanonet_debug_stats_open(struct inode *inode, struct file *file) {
    return single_open(file, nanonet_debug_stats_show, NULL);
}

static const struct file_operations nanonet_debug_stats_fops = {
    .open = nanonet_debug_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

int nanonet_debug_init(void) {
    nanonet_debug_dir = debugfs_create_dir("nanonet", NULL);
    if (!nanonet_debug_dir) {
        printk(KERN_ERR "NANONET: Failed to create debugfs directory\n");
        return -ENOMEM;
    }

    nanonet_debug_stats = debugfs_create_file("stats", 0444, nanonet_debug_dir, NULL, &nanonet_debug_stats_fops);
    
    if (!nanonet_debug_stats) {
        debugfs_remove_recursive(nanonet_debug_dir);
        printk(KERN_ERR "NANONET: Failed to create debugfs stats file\n");
        return -ENOMEM;
    }

    return 0;
}

void nanonet_debug_cleanup(void) {
    debugfs_remove_recursive(nanonet_debug_dir);
}

void nanonet_log_error(const char *fmt, ...) {
    va_list args;
    char buffer[256];
    u64 ts = get_timestamp_ns();

    if (!__ratelimit(&nanonet_error_ratelimit)) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    snprintf(debug_stats.last_error, sizeof(debug_stats.last_error), "[%llu ns] %s", ts, buffer);
    printk(KERN_ERR "NANONET: %s\n", debug_stats.last_error);
}