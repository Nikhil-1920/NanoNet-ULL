# Setup Guide

This guide explains how to set up the NanoNet ultra-low-latency networking stack on a Linux system.

## Prerequisites

- **Operating System**: Linux (e.g., Ubuntu 20.04 or later, CentOS 8).
- **Kernel Headers**: Matching the running kernel (`uname -r`).
- **Tools**:
  - `build-essential` (GCC, Make)
  - Python 3 (`python3`, `python3-pip`)
  - `ethtool` for network configuration
- **Network Interface**: A high-performance NIC (e.g., `eth0`) supporting multicast.
- **Privileges**: Root access for module loading and configuration (due to `/dev/nanonet` permissions).

### Install Dependencies (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) python3 python3-pip ethtool
```

### Install Dependencies (RHEL/CentOS)
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y kernel-devel-$(uname -r) python3 ethtool
```

## Obtain the Project
If the project is hosted in a Git repository:
```bash
git clone <repository_url>
cd nanonet
```

Alternatively, extract the project tarball or copy the `nanonet` directory to your system.

## Build the Project
```bash
cd nanonet
./scripts/build.sh
```

This compiles:
- The kernel module (`nanonet.ko`).
- User-space tools (`tools/nanonet_control`, `tools/packet_generator`).

## Install the Kernel Module
```bash
sudo ./scripts/install.sh
```

This:
- Loads the module (`insmod nanonet.ko`).
- Sets permissions for `/dev/nanonet` (`chmod 600`, root-only access).

## Configure the Network Interface
Ensure the network interface (`eth0`) is up and configured:
```bash
sudo ip link set eth0 up
```

For multicast, verify the NIC supports it:
```bash
ip link show eth0 | grep MULTICAST
```

## Configure the Module
Optionally, create `configs/nanonet.conf` with your settings, e.g.:
```plaintext
target_ip=192.168.1.100
target_port=8080
protocol=udp
multicast=true
multicast_group=239.1.1.1
```

Apply the configuration (UDP example):
```bash
sudo ./tools/nanonet_control config 192.168.1.100 8080 udp multicast 239.1.1.1
```

For TCP:
```bash
sudo ./tools/nanonet_control config 192.168.1.100 8080 tcp
```

Enable the module:
```bash
sudo ./tools/nanonet_control enable
```

## Verify Setup
Check the module status:
```bash
sudo ./tools/nanonet_control status
```

Verify the module is loaded:
```bash
lsmod | grep nanonet
```

Check kernel logs:
```bash
dmesg | grep NANONET
```

## Cleanup
To unload the module and clean build artifacts:
```bash
sudo ./scripts/clean.sh
```

## Troubleshooting
- **Kernel Headers Mismatch**: Ensure `linux-headers-$(uname -r)` matches the running kernel.
- **Module Load Failure**: Check `dmesg` for errors (e.g., missing `eth0`).
- **Permission Denied**: Run commands with `sudo` due to `/dev/nanonet` permissions (`600`).
- **No Multicast Support**: Verify NIC capabilities with `ethtool -i eth0`.

You’re now ready to run tests or deploy the module in production!