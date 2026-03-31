#!/bin/bash
#
# Build AppImage for MDS Bridge
#
# Prerequisites (Ubuntu/Debian):
#   sudo apt-get install libhidapi-dev libcurl4-openssl-dev cmake build-essential
#
# Usage:
#   ./appimage/build-appimage.sh
#
# Output:
#   MDS_Bridge-x86_64.AppImage (or MDS_Bridge-aarch64.AppImage on ARM)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build-appimage"
APPDIR="${BUILD_DIR}/AppDir"
ARCH=$(uname -m)

# Version from CMakeLists.txt
VERSION=$(grep "project(mds_bridge VERSION" "${PROJECT_DIR}/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\).*/\1/')

echo "=== MDS Bridge AppImage Builder ==="
echo "Project:  ${PROJECT_DIR}"
echo "Build:    ${BUILD_DIR}"
echo "Version:  ${VERSION}"
echo "Arch:     ${ARCH}"
echo ""

# Download linuxdeploy if not present
LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-${ARCH}.AppImage"
if [ ! -f "$LINUXDEPLOY" ]; then
    echo ">>> Downloading linuxdeploy..."
    mkdir -p "$BUILD_DIR"
    wget -q --show-progress -O "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

# Clean previous AppDir
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

# Build the project
echo ""
echo ">>> Building MDS Bridge..."
mkdir -p "${BUILD_DIR}/cmake"
cd "${BUILD_DIR}/cmake"
cmake "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_TESTS=OFF \
    -DENABLE_NODEJS=OFF

make -j$(nproc)

# Install to AppDir
echo ""
echo ">>> Installing to AppDir..."
DESTDIR="$APPDIR" make install

# Also install the example binaries to usr/bin (they're the main apps)
mkdir -p "${APPDIR}/usr/bin"
cp -v "${BUILD_DIR}/cmake/examples/mds_gateway" "${APPDIR}/usr/bin/"
cp -v "${BUILD_DIR}/cmake/examples/mds_gateway_demo" "${APPDIR}/usr/bin/"
cp -v "${BUILD_DIR}/cmake/examples/mds_monitor" "${APPDIR}/usr/bin/"

# Copy desktop file and icon
echo ""
echo ">>> Setting up desktop integration..."
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"

cp "${SCRIPT_DIR}/mds-bridge.desktop" "${APPDIR}/usr/share/applications/"
cp "${SCRIPT_DIR}/mds-bridge.svg" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/"

# Create wrapper script that provides CLI access to all tools
cat > "${APPDIR}/usr/bin/mds-bridge-wrapper" << 'WRAPPER_EOF'
#!/bin/bash
#
# MDS Bridge AppImage wrapper
# Provides access to all MDS Bridge tools
#

# Get the bin directory where this script and the binaries live
BINDIR="$(dirname "$(readlink -f "$0")")"
TOOL="${1:-demo}"

case "$TOOL" in
    gateway|mds_gateway)
        shift 2>/dev/null || true
        exec "${BINDIR}/mds_gateway" "$@"
        ;;
    demo|mds_gateway_demo)
        shift 2>/dev/null || true
        exec "${BINDIR}/mds_gateway_demo" "$@"
        ;;
    monitor|mds_monitor)
        shift 2>/dev/null || true
        exec "${BINDIR}/mds_monitor" "$@"
        ;;
    --help|-h)
        echo "MDS Bridge - Memfault Diagnostic Service Tools"
        echo ""
        echo "Usage: $0 <command> [args...]"
        echo ""
        echo "Commands:"
        echo "  demo, mds_gateway_demo    CES booth demo with auto-reconnect (default)"
        echo "  gateway, mds_gateway      Upload diagnostic chunks"
        echo "  monitor, mds_monitor      Display diagnostic stream data"
        echo ""
        echo "Examples:"
        echo "  $0 2fe3 0007"
        echo "  $0 demo 2fe3 0007"
        echo "  $0 gateway 2fe3 0007 --dry-run"
        exit 0
        ;;
    *)
        # Assume it's arguments to mds_gateway_demo (the default tool)
        exec "${BINDIR}/mds_gateway_demo" "$@"
        ;;
esac
WRAPPER_EOF
chmod +x "${APPDIR}/usr/bin/mds-bridge-wrapper"

# Update desktop file to use wrapper
sed -i 's|Exec=mds_gateway|Exec=mds-bridge-wrapper|' "${APPDIR}/usr/share/applications/mds-bridge.desktop"

# Create custom AppRun that uses our wrapper (outside AppDir for linuxdeploy)
cat > "${BUILD_DIR}/AppRun" << 'APPRUN_EOF'
#!/bin/bash
APPDIR="$(dirname "$(readlink -f "$0")")"
exec "${APPDIR}/usr/bin/mds-bridge-wrapper" "$@"
APPRUN_EOF
chmod +x "${BUILD_DIR}/AppRun"

# Create AppImage
echo ""
echo ">>> Creating AppImage..."
cd "$BUILD_DIR"

# Set output name
export OUTPUT="MDS_Bridge-${VERSION}-${ARCH}.AppImage"

# Deploy the actual ELF binaries (not the shell wrapper)
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "${APPDIR}/usr/share/applications/mds-bridge.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/scalable/apps/mds-bridge.svg" \
    --executable "${APPDIR}/usr/bin/mds_gateway" \
    --executable "${APPDIR}/usr/bin/mds_gateway_demo" \
    --executable "${APPDIR}/usr/bin/mds_monitor" \
    --custom-apprun "${BUILD_DIR}/AppRun" \
    --output appimage

# Move to project root
mv "$OUTPUT" "$PROJECT_DIR/"

echo ""
echo "=== Build Complete ==="
echo "AppImage: ${PROJECT_DIR}/${OUTPUT}"
echo ""
echo "Usage:"
echo "  ./${OUTPUT} --help"
echo "  ./${OUTPUT} 2fe3 0007              # runs demo (default)"
echo "  ./${OUTPUT} gateway 2fe3 0007      # runs gateway"
echo "  ./${OUTPUT} monitor 2fe3 0007      # runs monitor"
echo ""
echo "Or extract and run directly:"
echo "  ./${OUTPUT} --appimage-extract"
echo "  ./squashfs-root/usr/bin/mds_gateway_demo 2fe3 0007"
