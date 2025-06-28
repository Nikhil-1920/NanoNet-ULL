#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

struct market_data {
    char symbol[8];
    uint32_t price;
    uint32_t quantity;
    uint64_t timestamp;
} __packed;

void print_usage(const char *program_name) {
    printf("Usage: %s <ip> <port> <protocol> [multicast <group>]\n", program_name);
    printf("Example: %s 192.168.1.100 8080 udp multicast 239.1.1.1\n", program_name);
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in dest_addr;
    struct market_data packet;
    int multicast = 0;
    struct in_addr multicast_group;

    if (argc < 4 || (strcmp(argv[3], "multicast") == 0 && argc != 5)) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[3], "tcp") == 0) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
    } else if (strcmp(argv[3], "udp") == 0) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    } else {
        printf("Invalid protocol: %s\n", argv[3]);
        return 1;
    }
    if (sock < 0) {
        perror("Failed to create socket");
        return 1;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(atoi(argv[2]));
    if (inet_aton(argv[1], &dest_addr.sin_addr) == 0) {
        printf("Invalid IP address: %s\n", argv[1]);
        close(sock);
        return 1;
    }

    if (argc == 5 && strcmp(argv[3], "udp") == 0 && strcmp(argv[4], "multicast") == 0) {
        multicast = 1;
        if (inet_aton(argv[5], &multicast_group) == 0) {
            printf("Invalid multicast group: %s\n", argv[5]);
            close(sock);
            return 1;
        }
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multicast_group;
        mreq.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            perror("Failed to join multicast group");
            close(sock);
            return 1;
        }
        dest_addr.sin_addr = multicast_group;
    }

    if (strcmp(argv[3], "tcp") == 0) {
        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("Failed to connect");
            close(sock);
            return 1;
        }
    }

    strncpy(packet.symbol, "AAPL    ", 8);
    packet.price = htonl(9999); // Below threshold to trigger order
    packet.quantity = htonl(1000);
    packet.timestamp = htobe64(time(NULL) * 1000000000ULL);

    for (int i = 0; i < 1000; i++) {
        packet.timestamp = htobe64(time(NULL) * 1000000000ULL + i * 1000000);
        if (strcmp(argv[3], "udp") == 0) {
            if (sendto(sock, &packet, sizeof(packet), 0,
                       (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
                perror("Failed to send packet");
                break;
            }
        } else {
            if (send(sock, &packet, sizeof(packet), 0) < 0) {
                perror("Failed to send packet");
                break;
            }
        }
        usleep(1000); // 1ms interval
    }

    close(sock);
    return 0;
}