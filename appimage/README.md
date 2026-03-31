# AppImage Packaging for MDS Bridge

This directory contains files for building a self-contained AppImage for Linux.

## Quick Start

```bash
# On Ubuntu/Debian, install dependencies first:
sudo apt-get install libhidapi-dev libcurl4-openssl-dev cmake build-essential wget

# Build the AppImage
./appimage/build-appimage.sh
```

This creates `MDS_Bridge-<version>-<arch>.AppImage` in the project root.

## Usage

```bash
# Show help
./MDS_Bridge-2.0.0-x86_64.AppImage --help

# Run gateway (default)
./MDS_Bridge-2.0.0-x86_64.AppImage 2fe3 0007
./MDS_Bridge-2.0.0-x86_64.AppImage gateway 2fe3 0007

# Run monitor
./MDS_Bridge-2.0.0-x86_64.AppImage monitor 2fe3 0007

# Run demo version
./MDS_Bridge-2.0.0-x86_64.AppImage demo 2fe3 0007

# Dry-run mode
./MDS_Bridge-2.0.0-x86_64.AppImage gateway 2fe3 0007 --dry-run
```

## HID Device Permissions

On Linux, non-root users typically can't access HID devices. Create a udev rule:

```bash
# Create /etc/udev/rules.d/99-mds-bridge.rules
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="2fe3", MODE="0666"

# Or for specific device:
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="2fe3", ATTRS{idProduct}=="0007", MODE="0666"

# Reload rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Files

- `build-appimage.sh` - Main build script
- `mds-bridge.desktop` - Desktop entry file
- `mds-bridge.svg` - Application icon

## How It Works

1. Downloads `linuxdeploy` tool (if not cached)
2. Builds MDS Bridge with static libraries
3. Bundles executables and shared library dependencies
4. Creates self-contained AppImage

## Customizing

Edit `mds-bridge.desktop` to change:
- Application name and description
- Default command (currently `mds_gateway`)
- Categories

## Troubleshooting

**AppImage won't run:**
```bash
# Check if FUSE is available
fusermount --version

# If not, extract and run directly
./MDS_Bridge-*.AppImage --appimage-extract
./squashfs-root/usr/bin/mds_gateway 2fe3 0007
```

**Missing libraries:**
```bash
# Check what's bundled
./MDS_Bridge-*.AppImage --appimage-extract
ldd ./squashfs-root/usr/bin/mds_gateway
```

**Build fails with missing deps:**
```bash
# Ubuntu/Debian
sudo apt-get install libhidapi-dev libcurl4-openssl-dev

# Fedora
sudo dnf install hidapi-devel libcurl-devel

# Arch
sudo pacman -S hidapi curl
```
