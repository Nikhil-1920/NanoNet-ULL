# NanoNet-ULL: Ultra-Low Latency Networking Stack

## Overview

**NanoNet** is a high-performance Linux kernel module designed for ultra-low latency network packet processing, optimized for applications like High-Frequency Trading (HFT), real-time analytics, and other latency-sensitive systems. By bypassing parts of the standard Linux networking stack, NanoNet achieves nanosecond-level packet processing and response generation, making it ideal for environments where microseconds determine success. It supports TCP and UDP protocols, with specific optimizations for UDP multicast, and includes user-space tools and test scripts for configuration, packet generation, and performance evaluation.

NanoNet processes incoming market data packets, applies custom application logic (e.g., generating trading orders based on price thresholds), and sends responses with minimal overhead. It features advanced optimizations like pre-allocated response pools, CPU affinity, and packet prefetching, along with robust monitoring via debugfs, procfs, and trace events.

## Use Cases

NanoNet is designed for scenarios requiring rapid packet processing and response generation. Key use cases include:

1. **High-Frequency Trading (HFT)**:
  - Processes real-time market data feeds (e.g., stock prices) via UDP multicast and generates buy/sell orders based on predefined logic.
  - Minimizes latency to execute trades before market conditions change, critical for profitability in HFT.
  - Supports multicast for efficient data dissemination from exchanges.

2. **Real-Time Applications**:
  - Suitable for gaming servers, IoT systems, or real-time analytics where low-latency packet processing is essential.
  - Extensible for custom application logic beyond trading.

3. **Network Performance Testing**:
  - Includes tools (`packet_generator`) and test scripts (`test_latency.py`, `test_functional.py`) to benchmark latency and verify functionality.

4. **Research and Development**:
  - Provides a framework for experimenting with kernel-level networking optimizations, such as custom packet processing or buffer management.

## Project Structure

```
NanoNet-ULL/
├── src/                        # Kernel module source files
│   ├── nanonet.c               # Main kernel module logic
│   ├── micro_stack.c           # Packet parsing and checksum computation
│   ├── packet_processor.c      # Application logic (e.g., trading order generation)
│   ├── response_sender.c       # Response packet creation and transmission
│   ├── control_interface.c     # User-space control interface via /dev/nanonet
│   ├── optimizations.c         # Performance optimizations (e.g., response pool)
│   ├── security.c              # Packet validation and TCP connection tracking
│   └── debug.c                 # Debugfs interface and error logging
├── include/                    # Header files
│   └── nanonet.h               # Common structures and prototypes
├── tools/                      # User-space utilities
│   ├── nanonet_control.c       # Control program for configuring module
│   └── packet_generator.c      # Tool to generate test packets
├── tests/                      # Test scripts
│   ├── test_latency.py         # Latency measurement script
│   ├── test_functional.py      # Functional test script
├── deploy/                     # Deployment scripts
│   ├── production_deploy.sh    # Production deployment script
│   └── undeploy.sh             # Cleanup script
├── docs/                       # Documentation
│   ├── performance_tuning.md   # Performance optimization guide
│   ├── setup_guide.md          # Setup and installation guide
│   ├── usage_guide.md          # Usage instructions
├── scripts/                    # Build and test scripts
│   ├── build.sh                # Build script
│   ├── install.sh              # Installation script
│   ├── test.sh                 # Test script
│   ├── clean.sh                # Cleanup script
├── configs/                    # Configuration files
│   └── nanonet.conf            # Default configuration
├── logs/                       # Log directory (created at runtime)
│   └── stats.log               # Statistics log
├── Makefile                    # Build configuration
└── README.md                   # Project overview
```

## Prerequisites

- **Operating System**: Linux (e.g., Ubuntu 20.04+, CentOS 8+).
- **Kernel Headers**: Matching the running kernel (`uname -r`).
- **Tools**: `build-essential`, Python 3, `ethtool`.
- **Network Interface**: A high-performance NIC (e.g., `eth0`) supporting multicast.
- **Privileges**: Root access for module loading and configuration.

Install dependencies (Debian/Ubuntu):
```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) python3 python3-pip ethtool
```

Install dependencies (RHEL/CentOS):
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y kernel-devel-$(uname -r) python3 ethtool
```

## Building the Project

1. **Navigate to the Project Directory**:
   ```bash
   cd nano-net
   ```

2. **Build the Kernel Module and Tools**:
   ```bash
   ./scripts/build.sh
   ```

   **Expected Build Output**:
   ```
   Building nano-net...
   make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
     CC [M]  src/nanonet.o
     CC [M]  src/micro_stack.o
     CC [M]  src/packet_processor.o
     CC [M]  src/response_sender.o
     CC [M]  src/control_interface.o
     CC [M]  src/optimizations.o
     CC [M]  src/security.o
     CC [M]  src/debug.o
     LD [M]  nanonet.ko
   make -f Makefile tools
   Build completed successfully.
   Artifacts:
   - nanonet.ko (kernel module)
   - tools/nanonet_control (control tool)
   - tools/packet_generator (packet generator)
   ```

## Installing the Kernel Module

```bash
./scripts/install.sh
```

This loads `nanonet.ko` and sets permissions for `/dev/nanonet`.

**Expected Install Output** (in kernel logs, view via `dmesg`):
```
[ 1234.567890] NANONET: Initializing ultra-low latency networking module
[ 1234.567900] NANONET: Control interface initialized
[ 1234.567910] NANONET: Module loaded successfully
[ 1234.567920] NANONET: Use /dev/nanonet for control or check /proc/nanonet for status
```

**Artifacts Created**:
- `/dev/nanonet`: Character device for `ioctl` control.
- `/proc/nanonet`: Procfs entry for status and statistics.
- `/sys/kernel/debug/nanonet/stats`: Debugfs entry for debug statistics.

## Running the Project

### Step 1: Configure the Module
Edit `configs/nanonet.conf`:
```plaintext
target_ip=192.168.1.100
target_port=8080
protocol=udp
multicast=true
multicast_group=239.1.1.1
```

Apply the configuration:
```bash
./tools/nanonet_control config 192.168.1.100 8080 udp multicast 239.1.1.1
```

**Expected Output**:
```
Configuration updated
```

Enable the module:
```bash
./tools/nanonet_control enable
```

**Expected Output**:
```
Module enabled
```

### Step 2: Generate Test Packets
Simulate market data:
```bash
./tools/packet_generator 192.168.1.100 8080 udp multicast 239.1.1.1
```

**Expected Behavior**:
- Sends 1000 UDP packets with:
  - Symbol: `AAPL    `
  - Price: `9999` cents (triggers buy order)
  - Quantity: `1000`
  - Timestamp: Current time
- The module generates a `trading_order` for each packet (price < `$100.00`):
  - Symbol: `AAPL    `
  - Price: `10000` cents
  - Quantity: `100`
  - Side: Buy (`0`)
  - Sent to `192.168.1.100:9999`

### Step 3: Test Latency
```bash
python3 tests/test_latency.py --ip 192.168.1.100 --port 8080 --protocol udp --multicast 239.1.1.1
```

**Expected Output** (example):
```
Running latency test: 1000 packets to 192.168.1.100:8080 (udp)
Multicast group: 239.1.1.1
Sent 0 packets...
Sent 100 packets...
...
Sent 900 packets...

Latency Results:
Packets sent: 1000
Min latency: 1,234 ns (1.23 μs)
Max latency: 5,678 ns (5.68 μs)
Avg latency: 2,500 ns (2.50 μs)
Median latency: 2,400 ns (2.40 μs)
Std deviation: 500 ns (0.50 μs)
Jitter: 4,444 ns (4.44 μs)
95th percentile: 3,800 ns (3.80 μs)
99th percentile: 4,500 ns (4.50 μs)
```

### Step 4: Test Functionality
```bash
python3 tests/test_functional.py --ip 192.168.1.100 --port 8080 --protocol udp --multicast 239.1.1.1
```

**Expected Output**:
```
Running functional test: 192.168.1.100:8080 (udp)
Multicast group: 239.1.1.1
Received order: symbol=AAPL    , price=10000, quantity=100, side=buy
```

### Step 5: Monitor Statistics
View status:
```bash
./tools/nanonet_control status
```

**Expected Output** (example):
```
NanoNet Status:
Enabled: Yes
Target IP: 192.168.1.100
Target Port: 8080
Protocol: UDP
Multicast: Yes
Multicast Group: 239.1.1.1

Statistics:
Packets Processed: 1000
Packets Bypassed: 0
Responses Sent: 1000
Errors: 0
Active Connections: 0
Dropped Connections: 0
Min Process Time: 1500 ns
Max Process Time: 3000 ns
Avg Process Time: 2200 ns
```

View debug statistics:
```bash
cat /sys/kernel/debug/nanonet/stats
```

**Expected Output**:
```
NanoNet Debug Statistics
========================
Total Interrupts: 1000
Cache Misses: 50
Memory Allocations: 1000
Queue Full Events: 0
Checksum Errors: 0
Last Error: [1234567890 ns] None
```

View trace events:
```bash
echo 1 > /sys/kernel/debug/tracing/events/nanonet/ull_packet_processed/enable
cat /sys/kernel/debug/tracing/trace
```

**Expected Output** (example):
```
# tracer: nop
# entries-in-buffer/entries-written: 1000/1000   #P:4
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
           <...>-1234 [000] .... 1234.567890: ull_packet_processed: src=192.168.1.1:12345 dst=239.1.1.1:8080 time=2500 ns result=1
```

View logs:
```bash
cat logs/stats.log
```

**Expected Output** (example):
```
NanoNet Status:
Enabled: Yes
Target IP: 192.168.1.100
Target Port: 8080
Protocol: UDP
Multicast: Yes
Multicast Group: 239.1.1.1
...
```

### Step 6: Deploy in Production
```bash
./deploy/production_deploy.sh
```

### Step 7: Clean Up
```bash
./deploy/undeploy.sh
./scripts/clean.sh
```

**Expected Undeploy Output** (in kernel logs):
```
[ 1234.678900] NANONET: Unloading module
[ 1234.678910] NANONET: Control interface cleaned up
[ 1234.678920] NANONET: Module unloaded successfully
```

## Output Artifacts

- **Kernel Module (`nanonet.ko`)**:
  - Processes packets, generates trading orders, and logs metrics.
- **Control Tool (`nanonet_control`)**:
  - Configures the module and displays status/statistics.
- **Packet Generator (`packet_generator`)**:
  - Simulates market data to trigger responses.
- **Test Scripts**:
  - `test_latency.py`: Reports latency statistics.
  - `test_functional.py`: Verifies order generation.
- **System Interfaces**:
  - `/dev/nanonet`: For `ioctl` control.
  - `/proc/nanonet`: For status and statistics.
  - `/sys/kernel/debug/nanonet/stats`: For debug statistics.
  - `/sys/kernel/debug/tracing/trace`: For packet traces.
- **Logs**: `logs/stats.log` for runtime status snapshots.

## Use in High-Frequency Trading (HFT)

NanoNet is transformative in HFT due to its ability to process market data and generate orders in nanoseconds:
- **Market Data Processing**: Handles high-speed UDP multicast feeds from exchanges (e.g., NASDAQ), generating buy orders for stocks below a threshold (e.g., `$100.00` for AAPL).
- **Latency Advantage**: Achieves sub-microsecond processing, enabling trades before competitors, critical for arbitrage and market-making strategies.
- **Scalability**: Manages high packet volumes with pre-allocated buffers and CPU optimizations.
- **Monitoring**: Provides real-time performance metrics via debugfs and trace events, essential for optimizing trading strategies.

**Example**: A firm uses NanoNet to monitor AAPL prices. When the price drops below `$100.00`, NanoNet generates a buy order in ~2 μs, securing a favorable trade.

## Influence in Modern Technology

NanoNet’s impact extends beyond HFT:
1. **Financial Markets**: Enhances market efficiency by enabling rapid trading, contributing to over 50% of equity market volume driven by HFT.
2. **Technology Innovation**: Pushes the boundaries of kernel-level networking, inspiring advancements in telecommunications, gaming, and edge computing.
3. **Open-Source**: GPL-licensed, fostering collaboration and innovation in low-latency systems.
4. **Versatility**: Adaptable for non-HFT applications like real-time analytics or IoT.

## Troubleshooting

- **Module Load Failure**: Check kernel headers (`linux-headers-$(uname -r)`) and `dmesg` for errors.
- **No Responses**: Verify configuration (`nanonet_control status`), firewall settings, and response port (`9999`).
- **Test Failures**: Ensure Python 3 is installed and the response port is open.
- **Performance Issues**: Refer to `docs/performance_tuning.md` for optimization tips.

## Future Improvements

- Support additional protocols (e.g., SCTP).
- Enhance trading logic in `packet_processor.c`.
- Implement multi-core packet processing.
- Add a web-based monitoring dashboard.

## License

GNU General Public License (GPL) v2. See `src/nanonet.c` for details.

## Author

Nikhil Singh
