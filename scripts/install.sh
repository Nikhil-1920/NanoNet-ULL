#!/bin/bash

# install.sh
# Installs the NanoNet kernel module and sets permissions.

set -e

# Check for root privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script must be run as root."
    exit 1
fi

# Check for build dependencies
if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
    echo "Error: Kernel headers for $(uname -r) not found."
    exit 1
fi
if ! command -v make &> /dev/null; then
    echo "Error: make is required."
    exit 1
fi

# Build the project
echo "Building the project..."
if ! ./build.sh; then
    echo "Error: Build failed."
    exit 1
fi

# Load the kernel module
echo "Loading kernel module nanonet..."
if ! insmod nanonet.ko; then
    echo "Error: Failed to load kernel module."
    exit 1
fi

# Set permissions for /dev/nanonet
echo "Setting permissions for /dev/nanonet..."
if [ -c /dev/nanonet ]; then
    if ! chmod 600 /dev/nanonet; then
        echo "Error: Failed to set permissions."
        rmmod nanonet 2>/dev/null || true
        exit 1
    fi
else
    echo "Error: /dev/nanonet not found."
    rmmod nanonet 2>/dev/null || true
    exit 1
fi

echo "Installation completed successfully."