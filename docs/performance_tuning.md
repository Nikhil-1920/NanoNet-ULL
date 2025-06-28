# Performance Tuning Guide

This guide provides recommendations for optimizing the performance of the NanoNet ultra-low-latency networking stack for high-frequency trading (HFT) and other low-latency applications.

## 1. Hardware Optimizations

### CPU Selection
- Use high-frequency CPUs with large cache sizes (e.g., Intel Xeon Scalable or AMD EPYC processors).
- Enable Turbo Boost for maximum clock speed.
- Disable hyper-threading to reduce context-switching overhead:
  ```bash
  echo 0 > /sys/devices/system/cpu/smt/control
  ```

### NUMA Configuration
- Ensure the network interface card (NIC) and CPU are on the same NUMA node to minimize memory access latency.
- Use `numactl` to pin the module to a specific NUMA node:
  ```bash
  numactl --membind=0 --cpunodebind=0 ./tools/nanonet_control status
  ```

### NIC Configuration
- Use a low-latency NIC (e.g., Solarflare or Mellanox ConnectX-5).
- Enable Receive Side Scaling (RSS) and set multiple receive queues:
  ```bash
  ethtool -L eth0 combined 4
  ```
- Disable interrupt coalescing for minimal latency:
  ```bash
  ethtool -C eth0 rx-usecs 0
  ```

## 2. Kernel and System Tuning

### Kernel Parameters
- Disable power-saving features:
  ```bash
  echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
  ```
- Increase socket buffer sizes:
  ```bash
  sysctl -w net.core.rmem_max=8388608
  sysctl -w net.core.wmem_max=8388608
  ```

### IRQ Affinity
- Pin NIC interrupts to specific CPU cores to reduce latency:
  ```bash
  set_irq_affinity.sh eth0 0-3
  ```
  Alternatively, use `irqbalance` or a custom script to distribute IRQs.

### Real-Time Scheduling
- Run user-space tools with real-time priority (requires root):
  ```bash
  chrt -r -p 99 $(pidof nanonet_control)
  ```

## 3. Module-Specific Optimizations

### Response Pool Size
- The default `RESPONSE_POOL_SIZE` in `optimizations.c` is 256. For high packet rates (>10,000 packets/sec), consider increasing to 512 or 1024:
  ```c
  #define RESPONSE_POOL_SIZE 512
  ```
- Ensure sufficient memory for larger buffers if packet sizes exceed 1500 bytes. Pre-allocate larger `sk_buff` sizes in `nanonet_init_response_pool`.

### CPU Affinity
- By default, `nanonet_set_cpu_affinity` in `optimizations.c` pins to CPU core 0. For parallel processing, modify to use multiple cores:
  ```c
  cpumask_clear(&mask);
  cpumask_set_cpu(smp_processor_id(), &mask);
  set_cpus_allowed_ptr(current, &mask);
  ```

### Multicast Configuration
- Ensure the NIC supports hardware multicast filtering.
- Verify the multicast group is correctly joined:
  ```bash
  ip maddr show dev eth0
  ```
- Clean up multicast group membership during module unload to free resources:
  ```bash
  ./tools/nanonet_control disable
  ```

### TCP Connection Tracking
- For TCP-based applications, optimize `nanonet_track_tcp_connection` in `security.c`:
  - Increase `CONN_HASH_SIZE` (default 1024) for high connection volumes:
    ```c
    #define CONN_HASH_SIZE 2048
    ```
  - Periodically clear stale connections using:
    ```bash
    ./tools/nanonet_control clear-connections
    ```

## 4. Application Logic Tuning
- Optimize `nanonet_process_application_logic` in `packet_processor.c` for specific trading strategies.
- Reduce memory allocations by reusing buffers for `trading_order` structures:
  ```c
  static struct trading_order order_cache[16]; // Pre-allocate a small pool
  ```
- Adjust the price threshold (`10000` cents) in `process_market_data` based on market conditions.

## 5. Monitoring and Profiling
- Use `/sys/kernel/debug/tracing` to analyze packet processing times:
  ```bash
  echo 1 > /sys/kernel/debug/tracing/events/nanonet/nanonet_packet_processed/enable
  cat /sys/kernel/debug/tracing/trace
  ```
- Monitor cache misses and memory allocations via `/sys/kernel/debug/nanonet/stats`:
  ```bash
  cat /sys/kernel/debug/nanonet/stats
  ```
- Use `perf` to profile kernel module performance:
  ```bash
  perf record -e cycles -k mono insmod nanonet.ko
  perf report
  ```

## 6. Network Tuning
- Enable jumbo frames if supported by the network to reduce packet overhead:
  ```bash
  ip link set eth0 mtu 9000
  ```
- Disable TCP offloading to ensure the module handles checksums:
  ```bash
  ethtool -K eth0 tso off gso off ufo off
  ```

## 7. Testing and Benchmarking
- Use `test_latency.py` to measure baseline performance:
  ```bash
  python3 tests/test_latency.py --ip 192.168.1.100 --port 8080 --protocol udp --multicast 239.1.1.1
  ```
- Test TCP performance as well:
  ```bash
  python3 tests/test_latency.py --ip 192.168.1.100 --port 8080 --protocol tcp
  ```
- Increase packet rate in `packet_generator.c` to stress-test the system:
  ```c
  for (int i = 0; i < 10000; i++)   // Send 10,000 packets
  ```

## 8. Troubleshooting Performance Issues
- **High Latency**: Check for interrupt conflicts (`cat /proc/interrupts`) or high system load (`top`, `htop`).
- **Packet Drops**: Increase `RESPONSE_POOL_SIZE` or check NIC buffer overflows (`ethtool -S eth0`).
- **Checksum Errors**: Verify NIC offloading settings and network integrity (`ethtool -k eth0`).
- **Permission Errors**: Ensure `nanonet_control` is run as root due to `/dev/nanonet` permissions (`600`).

By applying these optimizations, you can achieve sub-microsecond packet processing times, critical for HFT and other real-time applications.