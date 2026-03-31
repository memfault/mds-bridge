#!/bin/bash
#
# CES 2026 MDS Demo - Permissions Setup
#
# This script installs a udev rule to allow non-root access to Nordic USB devices.
# Only needs to be run once per Linux PC.
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

UDEV_RULE_FILE="/etc/udev/rules.d/99-nordic.rules"
UDEV_RULE_CONTENT='# Nordic Semiconductor devices - allow non-root access for MDS demo
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1915", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="1915", MODE="0666"'

echo -e "${CYAN}"
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║                                                           ║"
echo "║           MDS Demo - Permissions Setup                   ║"
echo "║                                                           ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo -e "${RESET}"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}✗ This script must be run with sudo${RESET}"
    echo ""
    echo "Please run:"
    echo -e "  ${WHITE}sudo $0${RESET}"
    echo ""
    exit 1
fi

echo -e "${BLUE}▸ Installing udev rule for Nordic devices...${RESET}"
echo ""

# Check if rule already exists
if [ -f "$UDEV_RULE_FILE" ]; then
    echo -e "${YELLOW}⚠  udev rule already exists${RESET}"
    echo ""
    echo "Current rule:"
    cat "$UDEV_RULE_FILE" | sed 's/^/  /'
    echo ""
    read -p "Overwrite? [y/N] " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Cancelled."
        exit 0
    fi
fi

# Install the udev rule
echo "$UDEV_RULE_CONTENT" > "$UDEV_RULE_FILE"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ udev rule installed${RESET}"
    echo ""
    echo "Rule location: $UDEV_RULE_FILE"
    echo "Rule content:"
    cat "$UDEV_RULE_FILE" | sed 's/^/  /'
    echo ""
else
    echo -e "${RED}✗ Failed to install udev rule${RESET}"
    exit 1
fi

# Reload udev rules
echo -e "${BLUE}▸ Reloading udev rules...${RESET}"
udevadm control --reload-rules

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ udev rules reloaded${RESET}"
    echo ""
else
    echo -e "${RED}✗ Failed to reload udev rules${RESET}"
    exit 1
fi

# Trigger udev for existing devices
echo -e "${BLUE}▸ Applying rules to existing devices...${RESET}"
udevadm trigger

echo -e "${GREEN}✓ Setup complete!${RESET}"
echo ""
echo -e "${CYAN}Next steps:${RESET}"
echo "  1. Unplug your Nordic device"
echo "  2. Plug it back in"
echo "  3. Run the demo without sudo:"
echo -e "     ${WHITE}./run-demo.sh${RESET}"
echo ""
echo -e "${GREEN}You will never need sudo again for this demo!${RESET}"
echo ""
