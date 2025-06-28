#!/bin/bash

# undeploy.sh
# Unloads the NanoNet networking stack and cleans up resources.

set -e

# Configuration
MODULE_NAME="nanonet"
DEVICE_PATH="/dev/nanonet"
LOG_DIR="../logs"
STATS_LOG="$LOG_DIR/nanonet_stats.log"

# Check for root privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script must be run as root."
    exit 1
fi

# Check if module is loaded
if lsmod | grep -q "$MODULE_NAME"; then
    echo "Unloading kernel module $MODULE_NAME..."
    rmmod "$MODULE_NAME" || {
        echo "Error: Failed to unload kernel module."
        exit 1
    }
    echo "Module unloaded successfully."
else
    echo "Module $MODULE_NAME is not loaded."
fi

# Remove device file if it exists
if [ -e "$DEVICE_PATH" ]; then
    echo "Removing device file $DEVICE_PATH..."
    rm -f "$DEVICE_PATH" || {
        echo "Warning: Failed to remove $DEVICE_PATH."
    }
fi

# Clear logs
if [ -d "$LOG_DIR" ]; then
    echo "Clearing logs in $LOG_DIR..."
    rm -f "$STATS_LOG" || {
        echo "Warning: Failed to clear $STATS_LOG."
    }
fi

# Clear procfs and debugfs entries (handled by module unload, but verify)
if [ -d "/sys/kernel/debug/nanonet" ]; then
    echo "Warning: Debugfs directory /sys/kernel/debug/nanonet still exists."
fi
if [ -e "/proc/nanonet" ]; then
    echo "Warning: Procfs entry /proc/nanonet still exists."
fi

echo "Undeployment completed successfully."
exit 0