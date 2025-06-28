#!/bin/bash

# test.sh
# Runs latency and functional tests for the NanoNet networking stack.

set -e

# Configuration
TARGET_IP="192.168.1.100"
TARGET_PORT="8080"
MULTICAST_GROUP="239.1.1.1"
PROTOCOLS=("udp" "tcp")

# Check for Python 3
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is required."
    exit 1
fi

# Check for control tool
if [ ! -x "./tools/nanonet_control" ]; then
    echo "Error: nanonet_control tool not found or not executable."
    exit 1
fi

# Check for test scripts
for script in tests/test_latency.py tests/test_functional.py; do
    if [ ! -f "$script" ]; then
        echo "Error: Test script $script not found."
        exit 1
    fi
done

# Ensure module is loaded
if ! lsmod | grep -q nanonet; then
    echo "Error: Kernel module nanonet is not loaded."
    exit 1
fi

# Run tests for each protocol
for PROTOCOL in "${PROTOCOLS[@]}"; do
    echo "Configuring module for $PROTOCOL..."
    if ! ./tools/nanonet_control config "$TARGET_IP" "$TARGET_PORT" "$PROTOCOL" multicast "$MULTICAST_GROUP"; then
        echo "Error: Failed to configure module for $PROTOCOL."
        exit 1
    fi
    if ! ./tools/nanonet_control enable; then
        echo "Error: Failed to enable module for $PROTOCOL."
        exit 1
    fi

    echo "Running latency test for $PROTOCOL..."
    if ! python3 tests/test_latency.py --ip "$TARGET_IP" --port "$TARGET_PORT" --protocol "$PROTOCOL" --multicast "$MULTICAST_GROUP"; then
        echo "Warning: Latency test for $PROTOCOL failed."
    fi

    echo "Running functional test for $PROTOCOL..."
    if ! python3 tests/test_functional.py --ip "$TARGET_IP" --port "$TARGET_PORT" --protocol "$PROTOCOL" --multicast "$MULTICAST_GROUP"; then
        echo "Warning: Functional test for $PROTOCOL failed."
    fi

    echo "Checking module statistics for $PROTOCOL..."
    if [ -f /proc/nanonet ]; then
        cat /proc/nanonet
    else
        echo "Warning: /proc/nanonet not found."
    fi
done

echo "Tests completed."