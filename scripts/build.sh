#!/bin/bash

# build.sh
# Builds the NanoNet kernel module and user-space tools.

set -e

# Check for build dependencies
if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
    echo "Error: Kernel headers for $(uname -r) not found."
    exit 1
fi
if ! command -v make &> /dev/null; then
    echo "Error: make is required."
    exit 1
fi

echo "Building NanoNet ultra-low-latency networking stack..."

# Build the kernel module and tools
if ! make -C /lib/modules/$(uname -r)/build M=$(pwd) modules; then
    echo "Error: Failed to build kernel module."
    exit 1
fi
if ! make -f Makefile tools; then
    echo "Error: Failed to build user-space tools."
    exit 1
fi

echo "Build completed successfully."
echo "Artifacts:"
echo "- nanonet.ko (kernel module)"
echo "- tools/nanonet_control (control tool)"
echo "- tools/packet_generator (packet generator)"