#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include "../include/nanonet.h"

extern struct ull_config global_config;
extern struct ull_stats global_stats;

static dev_t nanonet_dev_number;
static struct cdev nanonet_cdev;

static struct class *nanonet_class = NULL;
static struct device *nanonet_device = NULL;

#define NANONET_IOC_MAGIC 'u'
#define NANONET_IOC_SET_CONFIG _IOW(NANONET_IOC_MAGIC, 1, struct ull_config)
#define NANONET_IOC_GET_CONFIG _IOR(NANONET_IOC_MAGIC, 2, struct ull_config)
#define NANONET_IOC_GET_STATS  _IOR(NANONET_IOC_MAGIC, 3, struct ull_stats)
#define NANONET_IOC_RESET_STATS _IO(NANONET_IOC_MAGIC, 4)
#define NANONET_IOC_CLEAR_CONNECTIONS _IO(NANONET_IOC_MAGIC, 5)

static int nanonet_open(struct inode *inode, struct file *file) {
    return nanonet_check_permissions();
}

static int nanonet_release(struct inode *inode, struct file *file) {
    return 0;
}

static long nanonet_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int ret = 0;

    switch (cmd) {
        case NANONET_IOC_SET_CONFIG:
            if (copy_from_user(&global_config, (void __user *)arg, sizeof(struct ull_config))) {
                ret = -EFAULT;
                nanonet_log_error("Failed to copy config from user");
                break;
            }
            ret = nanonet_validate_config(&global_config);
            if (ret < 0) {
                nanonet_log_error("Invalid configuration: %d", ret);
                break;
            }
            printk(KERN_INFO "NANONET: Configuration updated\n");
            break;

        case NANONET_IOC_GET_CONFIG:
            if (copy_to_user((void __user *)arg, &global_config, sizeof(struct ull_config))) {
                ret = -EFAULT;
                nanonet_log_error("Failed to copy config to user");
            }
            break;

        case NANONET_IOC_GET_STATS:
            if (copy_to_user((void __user *)arg, &global_stats, sizeof(struct ull_stats))) {
                ret = -EFAULT;
                nanonet_log_error("Failed to copy stats to user");
            }
            break;

        case NANONET_IOC_RESET_STATS:
            atomic64_set(&global_stats.packets_processed, 0);
            atomic64_set(&global_stats.packets_bypassed, 0);
            atomic64_set(&global_stats.responses_sent, 0);
            atomic64_set(&global_stats.errors, 0);
            atomic64_set(&global_stats.connections_active, 0);
            atomic64_set(&global_stats.connections_dropped, 0);
            global_stats.last_process_time_ns = 0;
            global_stats.min_process_time_ns = UINT64_MAX;
            global_stats.max_process_time_ns = 0;
            global_stats.avg_process_time_ns = 0;
            printk(KERN_INFO "NANONET: Statistics reset\n");
            break;

        case NANONET_IOC_CLEAR_CONNECTIONS:
            nanonet_clear_tcp_connections();
            printk(KERN_INFO "NANONET: TCP connections cleared\n");
            break;

        default:
            ret = -ENOTTY;
            nanonet_log_error("Invalid IOCTL command: %u", cmd);
            break;
    }

    return ret;
}

static const struct file_operations nanonet_fops = {
    .owner = THIS_MODULE,
    .open = nanonet_open,
    .release = nanonet_release,
    .unlocked_ioctl = nanonet_ioctl,
};

static int nanonet_proc_show(struct seq_file *m, void *v) {
    seq_printf(m, "NanoNet Module Status\n");
    seq_printf(m, "========================================\n");
    seq_printf(m, "Enabled: %s\n", global_config.enabled ? "Yes" : "No");
    seq_printf(m, "Target IP: %pI4\n", &global_config.target_ip);
    seq_printf(m, "Target Port: %u\n", ntohs(global_config.target_port));
    seq_printf(m, "Protocol: %s\n", global_config.protocol == IPPROTO_TCP ? "TCP" : "UDP");
    seq_printf(m, "Multicast: %s\n", global_config.multicast ? "Yes" : "No");
    if (global_config.multicast) {
        seq_printf(m, "Multicast Group: %pI4\n", &global_config.multicast_group);
    }
    seq_printf(m, "\nStatistics:\n");
    seq_printf(m, "Packets Processed: %llu\n", atomic64_read(&global_stats.packets_processed));
    seq_printf(m, "Packets Bypassed: %llu\n", atomic64_read(&global_stats.packets_bypassed));
    seq_printf(m, "Responses Sent: %llu\n", atomic64_read(&global_stats.responses_sent));
    seq_printf(m, "Errors: %llu\n", atomic64_read(&global_stats.errors));
    seq_printf(m, "Active Connections: %llu\n", atomic64_read(&global_stats.connections_active));
    seq_printf(m, "Dropped Connections: %llu\n", atomic64_read(&global_stats.connections_dropped));
    seq_printf(m, "Min Process Time: %llu ns\n", global_stats.min_process_time_ns);
    seq_printf(m, "Max Process Time: %llu ns\n", global_stats.max_process_time_ns);
    seq_printf(m, "Avg Process Time: %llu ns\n", global_stats.avg_process_time_ns);

    return 0;
}

static int nanonet_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, nanonet_proc_show, NULL);
}

static const struct proc_ops nanonet_proc_fops = {
    .proc_open = nanonet_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

int nanonet_control_init(void) {
    int ret;

    ret = alloc_chrdev_region(&nanonet_dev_number, 0, 1, "nanonet");
    if (ret < 0) {
        printk(KERN_ERR "NANONET: Failed to allocate device number\n");
        return ret;
    }

    cdev_init(&nanonet_cdev, &nanonet_fops);
    nanonet_cdev.owner = THIS_MODULE;

    ret = cdev_add(&nanonet_cdev, nanonet_dev_number, 1);
    if (ret < 0) {
        unregister_chrdev_region(nanonet_dev_number, 1);
        printk(KERN_ERR "NANONET: Failed to add character device\n");
        return ret;
    }

    nanonet_class = class_create(THIS_MODULE, "nanonet");
    if (IS_ERR(nanonet_class)) {
        cdev_del(&nanonet_cdev);
        unregister_chrdev_region(nanonet_dev_number, 1);
        return PTR_ERR(nanonet_class);
    }

    nanonet_device = device_create(nanonet_class, NULL, nanonet_dev_number, NULL, "nanonet");
    if (IS_ERR(nanonet_device)) {
        class_destroy(nanonet_class);
        cdev_del(&nanonet_cdev);
        unregister_chrdev_region(nanonet_dev_number, 1);
        return PTR_ERR(nanonet_device);
    }

    if (!proc_create("nanonet", 0600, NULL, &nanonet_proc_fops)) {
        device_destroy(nanonet_class, nanonet_dev_number);
        class_destroy(nanonet_class);
        cdev_del(&nanonet_cdev);
        unregister_chrdev_region(nanonet_dev_number, 1);
        printk(KERN_ERR "NANONET: Failed to create proc entry\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "NANONET: Control interface initialized\n");
    return 0;
}

void nanonet_control_cleanup(void) {
    remove_proc_entry("nanonet", NULL);
    device_destroy(nanonet_class, nanonet_dev_number);
    class_destroy(nanonet_class);
    cdev_del(&nanonet_cdev);
    unregister_chrdev_region(nanonet_dev_number, 1);
    printk(KERN_INFO "NANONET: Control interface cleaned up\n");
}