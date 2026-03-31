# MDS Gateway Demo - CES 2026

**Quick Demo Guide for Nordic FAEs**

This demo showcases the Memfault Diagnostic Service (MDS) protocol running on Nordic hardware, streaming diagnostic data to the Memfault cloud.

---

## 🎯 Demo Overview

**What visitors will see:**

1. **Button Press** → Fault triggered on nRF5340-DK
2. **Visual Feedback** → LEDs indicate status (ready, fault, uploading, complete)
3. **Gateway Display** → Real-time progress on Linux PC terminal
4. **Cloud Dashboard** → Memfault web interface shows received diagnostics
5. **Auto-Reconnect** → Gateway automatically reconnects after device resets

**Duration:** 30-60 seconds per run

**Key Feature:** The gateway automatically reconnects after device faults! No need to restart the gateway between demos - just let the device reset and press the button again.

---

## 📋 Prerequisites

### Hardware
- **nRF5340-DK** (or nRF52840 Dongle for testing)
- **Linux PC** (Ubuntu 20.04+ recommended)
- **USB cable** (must support data, not charge-only)

### Software
- **Git**
- **CMake** 3.15+
- **GCC** or Clang
- **libhidapi-dev**
- **libcurl4-openssl-dev**

### Memfault Account
- Active Memfault project
- Project key configured in firmware
- Symbols uploaded for your build

---

## 🚀 First-Time Setup

### Step 1: Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y git cmake build-essential libhidapi-dev libcurl4-openssl-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install -y git cmake gcc hidapi-devel libcurl-devel
```

### Step 2: Clone and Build

```bash
# Clone the repository
git clone https://github.com/memfault/mds-bridge.git
cd mds-bridge

# Build the demo
mkdir -p build
cd build
cmake ..
make

# Verify demo executable was built
ls -la examples/mds_gateway_demo
```

### Step 3: Set Up Permissions

You have **two options** for USB permissions:

**Option A: Run with sudo (easiest)**
- Just run `sudo ./demo/run-demo.sh` every time
- Requires password each run
- Good for quick testing

**Option B: One-time udev setup (recommended for booth)**
```bash
cd demo
sudo ./setup-permissions.sh
# Unplug and replug your Nordic device
# Never need sudo again!
```

### Step 4: Test the Demo

```bash
# From the mds-bridge directory
cd demo
./run-demo.sh
```

You should see:
- Device auto-detection
- Colorized status messages
- "Waiting for diagnostic data..." prompt

Press `Ctrl+C` to stop.

---

## 🎪 Running the Demo at CES

### Before the Show

1. **Power on Linux PC** - demo scripts ready in `~/mds-bridge/demo/`
2. **Plug in nRF5340-DK** - verify LED shows "ready" state
3. **Open terminal** - maximize for visibility
4. **Launch gateway:**
   ```bash
   cd ~/mds-bridge/demo
   ./run-demo.sh
   ```
5. **Open browser** - Memfault dashboard on separate monitor (optional)

### Demo Flow

1. **Invite visitor:** "Let me show you Memfault's diagnostic streaming..."

2. **Point to terminal:** Gateway is running and waiting for data

3. **Press Button 1** on nRF5340-DK

4. **Visual feedback:**
   - DK LED turns red (fault injected)
   - Terminal shows "📦 Packet received"
   - Terminal shows "📤 UPLOADING CHUNK..."
   - Terminal shows "✓ UPLOAD SUCCESSFUL"
   - DK LED turns green (complete)

5. **Show dashboard:** Refresh Memfault browser tab
   - New issue appears
   - Stack trace visible
   - Metrics/coredump available

6. **Auto-Reconnect:** After the device resets from the fault:
   - Terminal shows "📴 DEVICE DISCONNECTED"
   - Terminal shows "🔄 RECONNECTING..."
   - Once device reboots, terminal shows "✓ DEVICE CONNECTED" again
   - Ready for next demo!

**Repeat for next visitor!** No need to restart the gateway.

### Quick Reset

The gateway automatically reconnects - **no restart needed!**

If demo gets stuck:
```bash
Ctrl+C          # Stop gateway
./run-demo.sh   # Restart
```

Or use Button 2 on DK to reset device state.

---

## 🔧 Troubleshooting

### Problem: "No Nordic devices found"

**Check:**
- Is device plugged in?
- Is USB cable data-capable? (Try a different cable)
- Does `lsusb | grep 1915` show the device?

**Solution:**
```bash
# Verify device is visible
lsusb | grep -i nordic

# If not visible, try different USB port
# If still not visible, check device power/firmware
```

### Problem: "Permission denied accessing USB device"

**Check:**
- Did you run `sudo ./setup-permissions.sh`?
- Did you unplug/replug after setup?
- Is udev rule installed? `ls /etc/udev/rules.d/99-nordic.rules`

**Solution:**
```bash
# Quick fix: use sudo
sudo ./run-demo.sh

# Permanent fix: install udev rule
sudo ./setup-permissions.sh
# Then unplug and replug device
```

### Problem: "Connection failed"

**Check:**
- Is correct firmware loaded on device?
- Does firmware have MDS support enabled?
- Are VID/PID correct for your device?

**Solution:**
```bash
# Check what VID/PID the device reports
lsusb | grep -i nordic

# Manually specify if auto-detect fails
../build/examples/mds_gateway_demo 1915 <pid>
```

### Problem: "Upload failed" or "HTTP error"

**Check:**
- Is Linux PC connected to internet?
- Is project key correct in firmware?
- Is Memfault service operational? (status.memfault.com)

**Solution:**
```bash
# Test internet connectivity
curl -I https://chunks.memfault.com/

# Verify project key in firmware matches Memfault project
# Check Memfault dashboard for API quota/limits
```

### Problem: Device sends data but nothing uploads

**Check:**
- Is chunk data valid?
- Are symbols uploaded for this firmware build?

**Solution:**
```bash
# Run in dry-run mode to see raw chunk data
../build/examples/mds_gateway 1915 <pid> --dry-run

# Verify symbols are uploaded in Memfault dashboard
```

---

## 📱 Device States & LED Indicators

**Expected LED behavior on nRF5340-DK:**

| State | LED | Meaning |
|-------|-----|---------|
| Ready | Slow green pulse | Waiting for button press |
| Fault | Solid red | Fault injected, capturing data |
| Streaming | Blinking amber | Sending data to gateway |
| Uploading | Fast amber blink | Gateway uploading to cloud |
| Complete | Solid green | Upload successful |
| Error | Blinking red | Error occurred |

*Note: Actual LED behavior depends on firmware implementation.*

---

## 🖥️ Terminal Output Guide

**Normal operation looks like:**

```
╔═══════════════════════════════════════════════════════════╗
║           MEMFAULT DIAGNOSTIC SERVICE GATEWAY             ║
║                    CES 2026 Demo                          ║
╚═══════════════════════════════════════════════════════════╝

━━━ DEVICE CONNECTION ━━━
✓ Connected to device

━━━ DEVICE CONFIGURATION ━━━
  Device ID:     nRF5340-DK-12345678
  Data URI:      https://chunks.memfault.com/api/v0/chunks/ACME
  Features:      0x0000001F
✓ Configuration read successfully

━━━ STREAM CONTROL ━━━
✓ Diagnostic streaming enabled

╔════════════════════════════════════════════════════════════╗
║                     ⚡ GATEWAY ACTIVE                       ║
╚════════════════════════════════════════════════════════════╝

┌────────────────────────────────────────────────────────────┐
│  Waiting for diagnostic data from device...               │
│  Press button on nRF device to trigger fault              │
│  Press Ctrl+C to stop                                     │
└────────────────────────────────────────────────────────────┘

[12:34:56] 💓 Waiting...
[12:35:12] 📦 Packet #1 received (session: 1, total: 1)
[12:35:12] 📤 UPLOADING CHUNK...
[12:35:13] ✓ UPLOAD SUCCESSFUL
  Total uploaded: 1 chunks, 512 bytes

--- After device fault/reset ---

╔════════════════════════════════════════════════════════════╗
║                     📴 DEVICE DISCONNECTED                  ║
╚════════════════════════════════════════════════════════════╝

▸ Device may have reset after fault - waiting to reconnect...
▸ Waiting for device to reboot...

╔════════════════════════════════════════════════════════════╗
║                     🔄 RECONNECTING...                      ║
╚════════════════════════════════════════════════════════════╝

━━━ DEVICE CONNECTION ━━━
  Target Device: 1915:CAFE
  Connecting...
✓ Connected to device

╔════════════════════════════════════════════════════════════╗
║                     ✓ DEVICE CONNECTED                      ║
╚════════════════════════════════════════════════════════════╝
```

**Color coding:**
- 🟢 **Green** = Success, ready
- 🟡 **Yellow** = In progress, waiting
- 🔴 **Red** = Error, fault
- 🔵 **Blue** = Info, status

---

## 🎬 Demo Script (30 seconds)

**Suggested talking points:**

> "This is Memfault's Diagnostic Service running on a Nordic nRF5340. Watch what happens when I trigger a fault..."
>
> *[Press Button 1]*
>
> "The device just captured a coredump and is streaming it over USB to this gateway. The gateway parses the MDS protocol and uploads to Memfault's cloud..."
>
> *[Point to terminal showing upload progress]*
>
> "And there's the confirmation - uploaded successfully. Now if we look at the Memfault dashboard..."
>
> *[Switch to browser tab]*
>
> "...we can see the new issue with full stack trace, variables, and system metrics. All of this happened automatically - no manual intervention needed."

---

## 🔍 Advanced: Manual Operation

If you need more control, you can run the gateway directly:

```bash
# Auto-detect and run
cd build
./examples/mds_gateway_demo 1915 cafe

# Dry-run mode (don't upload)
./examples/mds_gateway 1915 cafe --dry-run

# Original version (less visual)
./examples/mds_gateway 1915 cafe

# Monitor only (no upload)
./examples/mds_monitor 1915 cafe
```

---

## 📞 Support

**Before CES:**
- Test the full flow end-to-end
- Verify Memfault project is configured
- Upload symbols for your firmware build
- Run through demo script 10+ times
- Have backup device ready

**During CES:**
- Keep this README accessible
- Have backup USB cables
- Monitor internet connectivity
- Log issues for post-show review

**Emergency contacts:**
- [Your support contact info here]

---

## ✅ Pre-Show Checklist

- [ ] Linux PC configured and tested
- [ ] Dependencies installed (`libhidapi-dev`, `libcurl4-openssl-dev`)
- [ ] MDS Bridge built successfully
- [ ] Permissions configured (udev rule installed)
- [ ] nRF5340-DK with MDS firmware programmed
- [ ] Device detected by `lsusb`
- [ ] Demo runs successfully (end-to-end test)
- [ ] Memfault project configured (project key, symbols uploaded)
- [ ] Browser bookmark to Memfault dashboard
- [ ] Backup nRF5340-DK available
- [ ] Backup USB cables available
- [ ] This README printed/accessible
- [ ] Practice run completed successfully

---

## 📚 Additional Resources

- **MDS Bridge Documentation:** `../README.md`
- **Memfault Docs:** https://docs.memfault.com/
- **Troubleshooting:** See "Troubleshooting" section above
- **Source Code:** `../examples/mds_gateway_demo.c`

---

**Last Updated:** 2025-12-15
**Version:** 1.1 (Added auto-reconnect support)
**Contact:** [Your contact info]
