#!/bin/bash

# clean.sh
# Cleans up the NanoNet kernel module and build artifacts.

set -e

# Check for root privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script must be run as root."
    exit 1
fi

# Check for build dependencies
if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
    echo "Warning: Kernel headers for $(uname -r) not found, but proceeding with cleanup."
fi
if ! command -v make &> /dev/null; then
    echo "Warning: make is not installed, but proceeding with cleanup."
fi

# Unload the kernel module
if lsmod | grep -q nanonet; then
    echo "Unloading kernel module nanonet..."
    if ! rmmod nanonet; then
        echo "Error: Failed to unload kernel module."
        exit 1
    fi
else
    echo "Kernel module nanonet is not loaded."
fi

# Clean build artifacts
echo "Cleaning build artifacts..."
if [ -f Makefile ]; then
    if ! make -C /lib/modules/$(uname -r)/build M=$(pwd) clean; then
        echo "Warning: Failed to clean kernel module artifacts."
    fi
    if ! make -f Makefile clean; then
        echo "Warning: Failed to clean user-space tool artifacts."
    fi
else
    echo "Warning: Makefile not found."
fi

# Remove temporary files
echo "Removing temporary files..."
find . -name "*.o" -delete
find . -name "*.ko" -delete
find . -name "*.mod.c" -delete
find . -name ".*.cmd" -delete
find . -name "Module.symvers" -delete
find . -name "modules.order" -delete

echo "Cleanup completed successfully."