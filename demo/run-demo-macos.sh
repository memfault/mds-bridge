#!/bin/bash
#
# CES 2026 MDS Demo Runner - macOS Version
#
# This script auto-detects Nordic devices and runs the demo gateway on macOS.
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
echo "║                       (macOS Version)                     ║"
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

# Function to find Nordic USB devices using system_profiler
find_nordic_devices() {
    # Use system_profiler to find USB devices with Nordic VID (0x1915)
    # Extract PID and product name
    system_profiler SPUSBDataType 2>/dev/null | grep -B 3 -A 6 "Vendor ID: 0x1915" | while read line; do
        # Look for product name (first line of device section, before Vendor ID)
        if [[ "$line" =~ ^[[:space:]]+([A-Za-z0-9][^:]+):$ ]]; then
            PRODUCT_NAME="${BASH_REMATCH[1]}"
        fi
        # Look for Product ID
        if [[ "$line" =~ Product[[:space:]]ID:[[:space:]]0x([0-9a-fA-F]+) ]]; then
            PID=$(echo "${BASH_REMATCH[1]}" | tr '[:upper:]' '[:lower:]')
            if [ -n "$PRODUCT_NAME" ] && [ -n "$PID" ]; then
                echo "${NORDIC_VID}:${PID} ${PRODUCT_NAME}"
                PRODUCT_NAME=""
                PID=""
            fi
        fi
    done
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
    echo "To see all USB devices, run:"
    echo -e "  ${WHITE}ioreg -p IOUSB -l -w 0 | grep -A 10 'idVendor'${RESET}"
    echo ""
    exit 1
fi

echo -e "${GREEN}✓ Found Nordic device(s):${RESET}"
echo "$DEVICES" | while read -r line; do
    vid_pid=$(echo "$line" | cut -d' ' -f1)
    name=$(echo "$line" | cut -d' ' -f2-)
    echo "  $vid_pid - $name"
done
echo ""

# Extract VID:PID from first device
FIRST_VIDPID=$(echo "$DEVICES" | head -1 | cut -d' ' -f1)
VID=$(echo "$FIRST_VIDPID" | cut -d':' -f1)
PID=$(echo "$FIRST_VIDPID" | cut -d':' -f2)

if [ -z "$VID" ] || [ -z "$PID" ]; then
    echo -e "${RED}✗ Failed to parse VID/PID${RESET}"
    echo ""
    echo "Please manually run:"
    echo -e "  ${WHITE}$DEMO_EXECUTABLE <vid> <pid>${RESET}"
    echo ""
    exit 1
fi

echo -e "${BLUE}▸ Using device: ${WHITE}${VID}:${PID}${RESET}"
echo ""

# macOS doesn't require special permissions for HID access
echo -e "${GREEN}✓ macOS HID access available (no setup needed)${RESET}"
echo ""

# Launch the demo
echo -e "${CYAN}━━━ Launching Demo Gateway ━━━${RESET}"
echo ""

# Run the demo with the detected device
exec "$DEMO_EXECUTABLE" "$VID" "$PID"
