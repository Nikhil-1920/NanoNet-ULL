#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <stdint.h>

struct ull_config {
    int enabled;
    uint32_t target_ip;
    uint16_t target_port;
    uint8_t protocol;
    uint32_t response_ip;
    uint16_t response_port;
    uint8_t application_logic_type;
    int multicast;
    uint32_t multicast_group;
};

struct ull_stats {
    long long packets_processed;
    long long packets_bypassed;
    long long responses_sent;
    long long errors;
    uint64_t last_process_time_ns;
    uint64_t min_process_time_ns;
    uint64_t max_process_time_ns;
    uint64_t avg_process_time_ns;
    long long connections_active;
    long long connections_dropped;
};

#define NANONET_IOC_MAGIC 'u'
#define NANONET_IOC_SET_CONFIG _IOW(NANONET_IOC_MAGIC, 1, struct ull_config)
#define NANONET_IOC_GET_CONFIG _IOR(NANONET_IOC_MAGIC, 2, struct ull_config)
#define NANONET_IOC_GET_STATS _IOR(NANONET_IOC_MAGIC, 3, struct ull_stats)
#define NANONET_IOC_RESET_STATS _IO(NANONET_IOC_MAGIC, 4)
#define NANONET_IOC_CLEAR_CONNECTIONS _IO(NANONET_IOC_MAGIC, 5)

#define DEVICE_PATH "/dev/nanonet"

void print_usage(const char *program_name) {
    printf("Usage: %s <command> [options]\n", program_name);
    printf("Commands:\n");
    printf("  status                    - Show current status\n");
    printf("  enable                    - Enable packet processing\n");
    printf("  disable                   - Disable packet processing\n");
    printf("  config <ip> <port> <proto> [multicast <group>]\n");
    printf("                            - Set target configuration\n");
    printf("  stats                     - Show statistics\n");
    printf("  reset                     - Reset statistics\n");
    printf("  clear-connections         - Clear TCP connections\n");
    printf("\nExample:\n");
    printf("  %s config 192.168.1.100 8080 udp multicast 239.1.1.1\n", program_name);
}

int main(int argc, char *argv[]) {
    int fd;
    struct ull_config config;
    struct ull_stats stats;
    int ret;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    if (strcmp(argv[1], "status") == 0) {
        ret = ioctl(fd, NANONET_IOC_GET_CONFIG, &config);
        if (ret < 0) {
            perror("Failed to get configuration");
            close(fd);
            return 1;
        }
        ret = ioctl(fd, NANONET_IOC_GET_STATS, &stats);
        if (ret < 0) {
            perror("Failed to get statistics");
            close(fd);
            return 1;
        }

        printf("NanoNet Module Status:\n");
        printf("Enabled: %s\n", config.enabled ? "Yes" : "No");
        printf("Target IP: %s\n", inet_ntoa(*(struct in_addr*)&config.target_ip));
        printf("Target Port: %u\n", ntohs(config.target_port));
        printf("Protocol: %s\n", config.protocol == 6 ? "TCP" : "UDP");
        printf("Multicast: %s\n", config.multicast ? "Yes" : "No");
        if (config.multicast) {
            printf("Multicast Group: %s\n", inet_ntoa(*(struct in_addr*)&config.multicast_group));
        }
        printf("\nStatistics:\n");
        printf("Packets Processed: %lld\n", stats.packets_processed);
        printf("Packets Bypassed: %lld\n", stats.packets_bypassed);
        printf("Responses Sent: %lld\n", stats.responses_sent);
        printf("Errors: %lld\n", stats.errors);
        printf("Active Connections: %lld\n", stats.connections_active);
        printf("Dropped Connections: %lld\n", stats.connections_dropped);
        printf("Min Process Time: %llu ns\n", stats.min_process_time_ns);
        printf("Max Process Time: %llu ns\n", stats.max_process_time_ns);
        printf("Avg Process Time: %llu ns\n", stats.avg_process_time_ns);

    } else if (strcmp(argv[1], "enable") == 0) {
        ret = ioctl(fd, NANONET_IOC_GET_CONFIG, &config);
        if (ret < 0) {
            perror("Failed to get configuration");
            close(fd);
            return 1;
        }
        config.enabled = 1;
        ret = ioctl(fd, NANONET_IOC_SET_CONFIG, &config);
        if (ret < 0) {
            perror("Failed to enable module");
            close(fd);
            return 1;
        }
        printf("Module enabled\n");

    } else if (strcmp(argv[1], "disable") == 0) {
        ret = ioctl(fd, NANONET_IOC_GET_CONFIG, &config);
        if (ret < 0) {
            perror("Failed to get configuration");
            close(fd);
            return 1;
        }
        config.enabled = 0;
        ret = ioctl(fd, NANONET_IOC_SET_CONFIG, &config);
        if (ret < 0) {
            perror("Failed to disable module");
            close(fd);
            return 1;
        }
        printf("Module disabled\n");

    } else if (strcmp(argv[1], "config") == 0) {
        if (argc < 5 || (strcmp(argv[4], "multicast") == 0 && argc != 6)) {
            printf("Usage: %s config <ip> <port> <proto> [multicast <group>]\n", argv[0]);
            close(fd);
            return 1;
        }

        memset(&config, 0, sizeof(config));
        if (inet_aton(argv[2], (struct in_addr*)&config.target_ip) == 0) {
            printf("Invalid IP address: %s\n", argv[2]);
            close(fd);
            return 1;
        }
        config.target_port = htons(atoi(argv[3]));
        if (strcmp(argv[4], "tcp") == 0) {
            config.protocol = 6;
        } else if (strcmp(argv[4], "udp") == 0) {
            config.protocol = 17;
        } else {
            printf("Invalid protocol: %s (use 'tcp' or 'udp')\n", argv[4]);
            close(fd);
            return 1;
        }
        config.response_ip = config.target_ip;
        config.response_port = htons(9999);
        config.application_logic_type = 0;

        if (argc == 6 && strcmp(argv[4], "udp") == 0 && strcmp(argv[5], "multicast") == 0) {
            config.multicast = 1;
            if (inet_aton(argv[6], (struct in_addr*)&config.multicast_group) == 0) {
                printf("Invalid multicast group: %s\n", argv[6]);
                close(fd);
                return 1;
            }
        }

        ret = ioctl(fd, NANONET_IOC_SET_CONFIG, &config);
        if (ret < 0) {
            perror("Failed to set configuration");
            close(fd);
            return 1;
        }
        printf("Configuration updated\n");

    } else if (strcmp(argv[1], "stats") == 0) {
        ret = ioctl(fd, NANONET_IOC_GET_STATS, &stats);
        if (ret < 0) {
            perror("Failed to get statistics");
            close(fd);
            return 1;
        }
        printf("Statistics:\n");
        printf("Packets Processed: %lld\n", stats.packets_processed);
        printf("Packets Bypassed: %lld\n", stats.packets_bypassed);
        printf("Responses Sent: %lld\n", stats.responses_sent);
        printf("Errors: %lld\n", stats.errors);
        printf("Active Connections: %lld\n", stats.connections_active);
        printf("Dropped Connections: %lld\n", stats.connections_dropped);
        printf("Min Process Time: %llu ns\n", stats.min_process_time_ns);
        printf("Max Process Time: %llu ns\n", stats.max_process_time_ns);
        printf("Avg Process Time: %llu ns\n", stats.avg_process_time_ns);

    } else if (strcmp(argv[1], "reset") == 0) {
        ret = ioctl(fd, NANONET_IOC_RESET_STATS, 0);
        if (ret < 0) {
            perror("Failed to reset statistics");
            close(fd);
            return 1;
        }
        printf("Statistics reset\n");

    } else if (strcmp(argv[1], "clear-connections") == 0) {
        ret = ioctl(fd, NANONET_IOC_CLEAR_CONNECTIONS, 0);
        if (ret < 0) {
            perror("Failed to clear connections");
            close(fd);
            return 1;
        }
        printf("TCP connections cleared\n");

    } else {
        printf("Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}