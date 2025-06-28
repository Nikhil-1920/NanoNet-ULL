#!/usr/bin/env python3

import socket
import struct
import time
import argparse

class FunctionalTester:
    def __init__(self, target_ip, target_port, protocol='udp', multicast_group=None):
        self.target_ip = target_ip
        self.target_port = target_port
        self.protocol = protocol.lower()
        self.multicast_group = multicast_group

    def create_test_packet(self, price):
        symbol = b'AAPL    '
        price = struct.pack('<I', price)
        quantity = struct.pack('<I', 1000)
        timestamp = struct.pack('<Q', int(time.time_ns()))
        return symbol + price + quantity + timestamp

    def run_test(self):
        print(f"Running functional test for NanoNet: {self.target_ip}:{self.target_port} ({self.protocol})")
        if self.multicast_group:
            print(f"Multicast group: {self.multicast_group}")

        if self.protocol == 'udp':
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(('0.0.0.0', 9999))  # Bind to response port
        else:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.bind(('0.0.0.0', 9999))
            sock.listen(1)

        try:
            # Send packet below threshold (should trigger order)
            packet = self.create_test_packet(9999)
            addr = (self.multicast_group if self.multicast_group else self.target_ip, self.target_port)
            if self.protocol == 'udp':
                if self.multicast_group:
                    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                                    socket.inet_aton(self.multicast_group) + socket.inet_aton('0.0.0.0'))
                sock.sendto(packet, addr)
                data, _ = sock.recvfrom(1024)
            else:
                sock.sendto(packet, addr)
                conn, _ = sock.accept()
                data = conn.recv(1024)
                conn.close()

            if len(data) >= 28:  # sizeof(trading_order)
                symbol = data[:8].decode()
                price = struct.unpack('<I', data[8:12])[0]
                quantity = struct.unpack('<I', data[12:16])[0]
                side = data[16]
                print(f"Received order: symbol={symbol}, price={price}, quantity={quantity}, side={'buy' if side == 0 else 'sell'}")
                return 0
            else:
                print("Invalid order received")
                return 1

        except Exception as e:
            print(f"Test failed: {e}")
            return 1
        finally:
            sock.close()

def main():
    parser = argparse.ArgumentParser(description='Functional test for NanoNet networking module')
    parser.add_argument('--ip', default='127.0.0.1', help='Target IP address')
    parser.add_argument('--port', type=int, default=8080, help='Target port')
    parser.add_argument('--protocol', choices=['tcp', 'udp'], default='udp', help='Protocol')
    parser.add_argument('--multicast', help='Multicast group IP (UDP only)')

    args = parser.parse_args()

    tester = FunctionalTester(args.ip, args.port, args.protocol, args.multicast)
    result = tester.run_test()
    exit(result)

if __name__ == '__main__':
    main()