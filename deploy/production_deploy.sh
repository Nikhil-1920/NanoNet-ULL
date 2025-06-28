#!/bin/bash
#
# production_deploy.sh
# Deploys the NanoNet networking stack in a production environment.
# Loads the kernel module, sets permissions, configures the module, and ensures network readiness.

set -e

# Configuration
MODULE_NAME="nanonet"
DEVICE_PATH="/dev/nanonet"
CONFIG_FILE="../configs/nanonet.conf"
NETWORK_INTERFACE="eth0"
LOG_DIR="../logs"
STATS_LOG="$LOG_DIR/nanonet_stats.log"

# Check for root privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script must be run as root."
    exit 1
fi

# Create log directory if it doesn't exist
mkdir -p "$LOG_DIR"

# Check if network interface exists
if ! ip link show "$NETWORK_INTERFACE" > /dev/null 2>&1; then
    echo "Error: Network interface $NETWORK_INTERFACE not found."
    exit 1
fi

# Build the project
echo "Building the project..."
cd ../
./scripts/build.sh

# Load the kernel module
echo "Loading kernel module $MODULE_NAME..."
insmod "$MODULE_NAME.ko" || {
    echo "Error: Failed to load kernel module."
    exit 1
}
echo "Module loaded successfully."

# Set permissions for /dev/nanonet
echo "Setting permissions for $DEVICE_PATH..."
chmod 666 "$DEVICE_PATH" || {
    echo "Error: Failed to set permissions for $DEVICE_PATH."
    exit 1
}

# Configure the module using nanonet.conf
if [ -f "$CONFIG_FILE" ]; then
    echo "Applying configuration from $CONFIG_FILE..."
    TARGET_IP=$(grep 'target_ip' "$CONFIG_FILE" | cut -d'=' -f2 | tr -d ' ')
    TARGET_PORT=$(grep 'target_port' "$CONFIG_FILE" | cut -d'=' -f2 | tr -d ' ')
    PROTOCOL=$(grep 'protocol' "$CONFIG_FILE" | cut -d'=' -f2 | tr -d ' ')
    MULTICAST=$(grep 'multicast' "$CONFIG_FILE" | cut -d'=' -f2 | tr -d ' ')
    MULTICAST_GROUP=$(grep 'multicast_group' "$CONFIG_FILE" | cut -d'=' -f2 | tr -d ' ')

    CMD="./tools/nanonet_control config $TARGET_IP $TARGET_PORT $PROTOCOL"
    if [ "$MULTICAST" = "true" ]; then
        CMD="$CMD multicast $MULTICAST_GROUP"
    fi

    $CMD || {
        echo "Error: Failed to apply configuration."
        rmmod "$MODULE_NAME"
        exit 1
    }
else
    echo "Warning: $CONFIG_FILE not found. Using default configuration."
    ./tools/nanonet_control config 192.168.1.100 8080 udp
fi

# Enable the module
echo "Enabling the module..."
./tools/nanonet_control enable || {
    echo "Error: Failed to enable module."
    rmmod "$MODULE_NAME"
    exit 1
}

# Verify deployment
echo "Verifying deployment..."
./tools/nanonet_control status | tee -a "$STATS_LOG"

echo "Deployment completed successfully."
exit 0