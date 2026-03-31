#!/bin/bash
#
# CES 2026 MDS Demo Runner
#
# This script auto-detects Nordic devices and runs the demo gateway.
# It handles permission issues gracefully and provides clear guidance.
#

set -e

# Colors
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
CYAN='\033[1;36m'
WHITE='\033[1;37m'
RESET='\033[0m'

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
DEMO_EXECUTABLE="${BUILD_DIR}/examples/mds_gateway_demo"

# Nordic Semiconductor VID
NORDIC_VID="1915"

# Banner
echo -e "${CYAN}"
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║                                                           ║"
echo "║           MEMFAULT DIAGNOSTIC SERVICE - CES 2026          ║"
echo "║                                                           ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo -e "${RESET}"
echo ""

# Check if demo executable exists
if [ ! -f "$DEMO_EXECUTABLE" ]; then
    echo -e "${RED}✗ Demo executable not found${RESET}"
    echo ""
    echo "Please build the project first:"
    echo -e "  ${WHITE}cd ${SCRIPT_DIR}/..${RESET}"
    echo -e "  ${WHITE}mkdir -p build && cd build${RESET}"
    echo -e "  ${WHITE}cmake ..${RESET}"
    echo -e "  ${WHITE}make${RESET}"
    echo ""
    exit 1
fi

echo -e "${BLUE}▸ Searching for Nordic devices...${RESET}"
echo ""

# Function to find Nordic USB devices using lsusb
find_nordic_devices() {
    if ! command -v lsusb &> /dev/null; then
        echo -e "${YELLOW}⚠  lsusb not found, cannot auto-detect devices${RESET}"
        return 1
    fi

    # Find all Nordic devices
    lsusb -d ${NORDIC_VID}: 2>/dev/null || true
}

# Get list of Nordic devices
DEVICES=$(find_nordic_devices)

if [ -z "$DEVICES" ]; then
    echo -e "${YELLOW}⚠  No Nordic devices found${RESET}"
    echo ""
    echo "Please ensure:"
    echo "  1. nRF5340-DK is plugged into USB"
    echo "  2. Device is powered on"
    echo "  3. USB cable supports data (not charge-only)"
    echo ""
    echo "You can also manually run:"
    echo -e "  ${WHITE}$DEMO_EXECUTABLE <vid> <pid>${RESET}"
    echo ""
    exit 1
fi

echo -e "${GREEN}✓ Found Nordic device(s):${RESET}"
echo "$DEVICES" | while read -r line; do
    echo "  $line"
done
echo ""

# Extract PID from first device
FIRST_PID=$(echo "$DEVICES" | head -1 | sed -n 's/.*ID [0-9a-f]*:\([0-9a-f]*\).*/\1/p')

if [ -z "$FIRST_PID" ]; then
    echo -e "${RED}✗ Failed to extract PID from lsusb output${RESET}"
    echo ""
    echo "Please manually run:"
    echo -e "  ${WHITE}$DEMO_EXECUTABLE <vid> <pid>${RESET}"
    echo ""
    exit 1
fi

echo -e "${BLUE}▸ Using device: ${WHITE}${NORDIC_VID}:${FIRST_PID}${RESET}"
echo ""

# Function to test HID access
test_hid_access() {
    # Try to list HID devices
    if [ -d /dev/hidraw0 ]; then
        # Check if we can read attributes
        if [ -r /sys/class/hidraw/hidraw0/device/uevent ]; then
            return 0
        fi
    fi
    return 1
}

# Check if we're running as root
if [ "$EUID" -eq 0 ]; then
    echo -e "${GREEN}✓ Running as root - HID access available${RESET}"
    echo ""
else
    # Check if we have access to HID devices
    if ! test_hid_access; then
        # Check if udev rule exists
        if [ -f /etc/udev/rules.d/99-nordic.rules ]; then
            echo -e "${YELLOW}⚠  udev rule exists but may need device replug${RESET}"
            echo ""
            echo "Please unplug and replug your Nordic device, then try again."
            echo ""
        else
            echo -e "${YELLOW}⚠  Permission denied accessing USB device${RESET}"
            echo ""
            echo -e "${CYAN}Option 1:${RESET} Run with sudo (easiest for demo)"
            echo -e "  ${WHITE}sudo $0${RESET}"
            echo ""
            echo -e "${CYAN}Option 2:${RESET} One-time setup (never need sudo again)"
            echo -e "  ${WHITE}${SCRIPT_DIR}/setup-permissions.sh${RESET}"
            echo ""
            exit 1
        fi
    else
        echo -e "${GREEN}✓ HID device access available${RESET}"
        echo ""
    fi
fi

# Launch the demo
echo -e "${CYAN}━━━ Launching Demo Gateway ━━━${RESET}"
echo ""

# Run the demo with the detected device
exec "$DEMO_EXECUTABLE" "$NORDIC_VID" "$FIRST_PID"
