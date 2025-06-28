# Usage Guide

This guide explains how to use the NanoNet ultra-low-latency networking stack for packet processing, testing, and monitoring. All commands requiring `/dev/nanonet` access must be run as root due to its permissions (`600`).

## Configuring the Module
Use the `nanonet_control` tool to configure the module:
```bash
sudo ./tools/nanonet_control config <ip> <port> <protocol> [multicast <group>]
```

Example (UDP with multicast):
```bash
sudo ./tools/nanonet_control config 192.168.1.100 8080 udp multicast 239.1.1.1
```

Example (TCP):
```bash
sudo ./tools/nanonet_control config 192.168.1.100 8080 tcp
```

Enable packet processing:
```bash
sudo ./tools/nanonet_control enable
```

View current configuration and statistics:
```bash
sudo ./tools/nanonet_control status
```

## Generating Test Packets
Use the `packet_generator` tool to simulate market data:
```bash
sudo ./tools/packet_generator 192.168.1.100 8080 udp multicast 239.1.1.1
```

For TCP:
```bash
sudo ./tools/packet_generator 192.168.1.100 8080 tcp
```

This sends 1000 packets with a price below the threshold (`9999` cents), triggering buy orders.

## Running Tests
Run the test suite for both protocols:
```bash
sudo ./scripts/test.sh
```

### Latency Test
Measure packet send latency:
```bash
python3 tests/test_latency.py --ip 192.168.1.100 --port 8080 --protocol udp --multicast 239.1.1.1
```
For TCP:
```bash
python3 tests/test_latency.py --ip 192.168.1.100 --port 8080 --protocol tcp
```

Expected output includes min, max, average, and percentile latencies.

### Functional Test
Verify order generation:
```bash
python3 tests/test_functional.py --ip 192.168.1.100 --port 8080 --protocol udp --multicast 239.1.1.1
```
For TCP:
```bash
python3 tests/test_functional.py --ip 192.168.1.100 --port 8080 --protocol tcp
```

Expected output confirms receipt of a buy order (e.g., `symbol=AAPL, price=10000, quantity=100, side=buy`).

## Monitoring
### Status and Statistics
```bash
sudo ./tools/nanonet_control status
```

View `/proc/nanonet` for detailed statistics:
```bash
cat /proc/nanonet
```

### Debug Statistics
```bash
cat /sys/kernel/debug/nanonet/stats
```

### Trace Events
Enable tracing:
```bash
echo 1 > /sys/kernel/debug/tracing/events/nanonet/nanonet_packet_processed/enable
cat /sys/kernel/debug/tracing/trace
```

### Logs
Check kernel logs:
```bash
dmesg | grep NANONET
```

## Managing Connections
Clear TCP connections (if using TCP):
```bash
sudo ./tools/nanonet_control clear-connections
```

Reset statistics:
```bash
sudo ./tools/nanonet_control reset
```

## Production Deployment
1. Install and configure the module:
   ```bash
   sudo ./scripts/install.sh
   sudo ./tools/nanonet_control config 192.168.1.100 8080 udp multicast 239.1.1.1
   sudo ./tools/nanonet_control enable
   ```
2. Send test packets:
   ```bash
   sudo ./tools/packet_generator 192.168.1.100 8080 udp multicast 239.1.1.1
   ```
3. Run functional test:
   ```bash
   python3 tests/test_functional.py --ip 192.168.1.100 --port 8080 --protocol udp --multicast 239.1.1.1
   ```
4. Monitor performance:
   ```bash
   sudo ./tools/nanonet_control status
   cat /sys/kernel/debug/nanonet/stats
   ```

## Cleanup
Unload the module and clean build artifacts:
```bash
sudo ./scripts/clean.sh
```

## Example Workflow
1. Install and configure:
   ```bash
   sudo ./scripts/install.sh
   sudo ./tools/nanonet_control config 192.168.1.100 8080 tcp
   sudo ./tools/nanonet_control enable
   ```
2. Send test packets:
   ```bash
   sudo ./tools/packet_generator 192.168.1.100 8080 tcp
   ```
3. Run functional test:
   ```bash
   python3 tests/test_functional.py --ip 192.168.1.100 --port 8080 --protocol tcp
   ```
4. Monitor and clear connections:
   ```bash
   sudo ./tools/nanonet_control status
   cat /sys/kernel/debug/nanonet/stats
   sudo ./tools/nanonet_control clear-connections
   ```

This workflow ensures the module is correctly processing packets and generating responses for HFT or other applications.