# MDS Gateway Demo - Quick Start

**One-page reference for booth staff**

---

## Setup (One Time)

```bash
# macOS
cd demo && ./run-demo-macos.sh

# Linux
cd demo && sudo ./run-demo.sh
```

---

## Demo Flow (30 seconds)

| Step | Action | What Happens |
|------|--------|--------------|
| 1 | Point to terminal | "Gateway is running, waiting for data..." |
| 2 | **Press Button 1** on nRF5340-DK | Fault triggered |
| 3 | Watch terminal | Packets stream, chunks upload |
| 4 | Show Memfault dashboard | Crash appears with full stack trace |
| 5 | Device auto-resets | Gateway reconnects automatically |
| 6 | **Ready for next demo!** | No restart needed |

---

## Key Talking Points

- **"Zero-touch crash capture"** - fault triggers automatic upload
- **"Standard USB HID"** - no drivers, works everywhere
- **"Seconds to cloud"** - see crashes in Memfault immediately
- **"Continuous operation"** - gateway survives device resets

---

## If Something Goes Wrong

| Problem | Fix |
|---------|-----|
| "No device found" | Check USB cable, try different port |
| "Permission denied" | Run with `sudo` |
| Gateway disconnects | It will auto-reconnect, just wait |
| Need full restart | `Ctrl+C` then `./run-demo-macos.sh` |

---

## Device Info

- **VID:** `1915` (Nordic)
- **Board:** nRF5340-DK
- **Transport:** USB HID
- **Cloud:** Memfault

---

*Press the button. See the crash. Fix the bug.*
