#!/usr/bin/env python3

import socket
import time
import threading
import struct
import argparse
import statistics

class LatencyTester:
    def __init__(self, target_ip, target_port, protocol='udp', multicast_group=None):
        self.target_ip = target_ip
        self.target_port = target_port
        self.protocol = protocol.lower()
        self.multicast_group = multicast_group
        self.results = []

    def create_test_packet(self, sequence_num):
        symbol = b'AAPL    '
        price = struct.pack('<I', 9999)
        quantity = struct.pack('<I', 1000)
        timestamp = struct.pack('<Q', int(time.time_ns()))
        return symbol + price + quantity + timestamp

    def send_packet(self, data):
        if self.protocol == 'udp':
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        else:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((self.target_ip, self.target_port))

        try:
            start_time = time.time_ns()
            if self.protocol == 'udp':
                addr = (self.multicast_group if self.multicast_group else self.target_ip, self.target_port)
                if self.multicast_group:
                    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                                    socket.inet_aton(self.multicast_group) + socket.inet_aton('0.0.0.0'))
                sock.sendto(data, addr)
            else:
                sock.send(data)
            end_time = time.time_ns()
            return end_time - start_time
        finally:
            sock.close()

    def run_test(self, num_packets=1000, interval_us=1000):
        print(f"Running latency test for NanoNet: {num_packets} packets to {self.target_ip}:{self.target_port} ({self.protocol})")
        if self.multicast_group:
            print(f"Multicast group: {self.multicast_group}")

        latencies = []

        for i in range(num_packets):
            packet = self.create_test_packet(i)
            try:
                latency = self.send_packet(packet)
                latencies.append(latency)
                if i % 100 == 0:
                    print(f"Sent {i} packets...")
            except Exception as e:
                print(f"Error sending packet {i}: {e}")
                continue
            if interval_us > 0:
                time.sleep(interval_us / 1000000.0)

        if latencies:
            min_latency = min(latencies)
            max_latency = max(latencies)
            avg_latency = statistics.mean(latencies)
            median_latency = statistics.median(latencies)
            std_dev = statistics.stdev(latencies) if len(latencies) > 1 else 0
            jitter = max(latencies) - min(latencies)

            print(f"\nLatency Results:")
            print(f"Packets sent: {len(latencies)}")
            print(f"Min latency: {min_latency:,} ns ({min_latency/1000:.2f} μs)")
            print(f"Max latency: {max_latency:,} ns ({max_latency/1000:.2f} μs)")
            print(f"Avg latency: {avg_latency:,.0f} ns ({avg_latency/1000:.2f} μs)")
            print(f"Median latency: {median_latency:,.0f} ns ({median_latency/1000:.2f} μs)")
            print(f"Std deviation: {std_dev:,.0f} ns ({std_dev/1000:.2f} μs)")
            print(f"Jitter: {jitter:,} ns ({jitter/1000:.2f} μs)")
            p95 = sorted(latencies)[int(len(latencies) * 0.95)]
            p99 = sorted(latencies)[int(len(latencies) * 0.99)]
            print(f"95th percentile: {p95:,} ns ({p95/1000:.2f} μs)")
            print(f"99th percentile: {p99:,} ns ({p99/1000:.2f} μs)")

def main():
    parser = argparse.ArgumentParser(description='Test NanoNet networking module')
    parser.add_argument('--ip', default='127.0.0.1', help='Target IP address')
    parser.add_argument('--port', type=int, default=8080, help='Target port')
    parser.add_argument('--protocol', choices=['tcp', 'udp'], default='udp', help='Protocol')
    parser.add_argument('--multicast', help='Multicast group IP (UDP only)')
    parser.add_argument('--packets', type=int, default=1000, help='Number of packets to send')
    parser.add_argument('--interval', type=int, default=1000, help='Interval between packets (microseconds)')

    args = parser.parse_args()

    tester = LatencyTester(args.ip, args.port, args.protocol, args.multicast)
    tester.run_test(args.packets, args.interval)

if __name__ == '__main__':
    main()