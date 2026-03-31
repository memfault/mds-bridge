# MDS Gateway Demo - Video & Presentation Content

**For CES 2026 Nordic Booth - 60-Second Looping Video**

---

## Slide Content (4 Key Points)

### Memfault Diagnostic Service (MDS) Gateway Demo
**Streaming Device Diagnostics to the Cloud in Real-Time**

**1. Zero-Touch Crash Capture**
When a fault occurs on the nRF5340, the coredump is automatically captured and streamed to Memfault's cloud — no manual intervention, no JTAG, no log pulling.

**2. USB HID Transport**
Diagnostic data flows over standard USB HID — no custom drivers, no serial port configuration. Works out-of-the-box on any host OS.

**3. Continuous Operation**
The gateway automatically reconnects after device resets. Trigger faults repeatedly without restarting anything — ideal for field deployment and automated testing.

**4. Instant Cloud Analysis**
Within seconds of a crash, full stack traces, register dumps, and system metrics appear in the Memfault dashboard — ready for root cause analysis.

---

## 60-Second Video Script

### Opening (0:00 - 0:08)
**[Visual: Nordic nRF5340-DK on desk, USB cable connected to laptop, terminal window visible]**

**Text on screen:** "What happens when your device crashes in the field?"

**Narration/Text:** "Every embedded developer knows the pain: a device crashes, and the evidence is gone."

---

### The Problem (0:08 - 0:15)
**[Visual: Quick cuts - frustrated developer, blinking error LED, question marks]**

**Text on screen:** "No JTAG. No logs. No answers."

**Narration/Text:** "No debugger attached. No way to retrieve the crash data. Until now."

---

### The Solution - Introduction (0:15 - 0:22)
**[Visual: Zoom to nRF5340-DK with Memfault + Nordic logos appearing]**

**Text on screen:** "Memfault Diagnostic Service on Nordic nRF5340"

**Narration/Text:** "Introducing the Memfault Diagnostic Service — crash diagnostics that stream directly to the cloud."

---

### Demo - The Crash (0:22 - 0:32)
**[Visual: Finger pressing button on DK, LED changes color, terminal shows activity]**

**Text on screen:** "One button. One crash. Full visibility."

**Narration/Text:** "Press a button to trigger a fault. The device captures a full coredump and streams it over USB HID to the gateway."

**[Visual: Terminal showing colorful output - "Packet received", "UPLOADING CHUNK", "UPLOAD SUCCESSFUL"]**

---

### Demo - The Cloud (0:32 - 0:42)
**[Visual: Memfault dashboard appearing, new issue populating, stack trace expanding]**

**Text on screen:** "Crash to cloud in seconds"

**Narration/Text:** "Within seconds, the crash appears in Memfault's dashboard — complete with stack trace, registers, and system state."

**[Visual: Highlight the stack trace, variable values, device info]**

---

### The Magic - Auto-Reconnect (0:42 - 0:50)
**[Visual: Device resets (LED blinks), terminal shows "RECONNECTING...", then "CONNECTED"]**

**Text on screen:** "Device resets. Gateway reconnects. Automatically."

**Narration/Text:** "When the device resets, the gateway reconnects automatically. No manual intervention. Ready for the next crash."

---

### Closing - Call to Action (0:50 - 0:60)
**[Visual: Split screen - nRF5340-DK on left, Memfault dashboard on right, both active]**

**Text on screen:**
- "Nordic nRF5340 + Memfault"
- "Try the live demo at our booth"

**Narration/Text:** "Nordic hardware. Memfault diagnostics. From crash to insight in seconds."

**[Visual: Nordic and Memfault logos side by side]**

**Final text:** "Ask for a demo"

---

## Key Visual Elements

### Terminal Output to Capture
```
╔═══════════════════════════════════════════════════════════╗
║           MEMFAULT DIAGNOSTIC SERVICE GATEWAY             ║
║                    CES 2026 Demo                          ║
╚═══════════════════════════════════════════════════════════╝

✓ Connected to device
✓ Diagnostic streaming enabled

⚡ GATEWAY ACTIVE

[12:34:56] 📦 Packet #1 received
[12:34:56] 📤 UPLOADING CHUNK...
[12:34:57] ✓ UPLOAD SUCCESSFUL
  Total uploaded: 47 chunks, 2,891 bytes

📴 DEVICE DISCONNECTED
🔄 RECONNECTING...
✓ DEVICE CONNECTED
```

### Memfault Dashboard Elements to Show
- New issue appearing in issue list
- Stack trace with function names and line numbers
- Register dump
- Device timeline
- "Coredump received" indicator

### Hardware Shot List
- nRF5340-DK hero shot (clean, well-lit)
- Close-up of button being pressed
- USB cable connection
- LED state changes (ready → fault → streaming → complete)

---

## Taglines (Pick Your Favorite)

1. **"Press the button. See the crash. Fix the bug."**
2. **"From crash to cloud in seconds."**
3. **"Your device crashed. Now you'll know why."**
4. **"Crash diagnostics without the guesswork."**
5. **"Every crash tells a story. Now you can read it."**

---

## Video Production Notes

### Tone
- Professional but approachable
- Confident, not salesy
- Focus on solving a real pain point

### Music
- Modern, tech-forward
- Subtle, not distracting
- Build slightly toward the demo moment

### Pacing
- Quick cuts for problem/intro (keeps attention)
- Slower, focused shots for demo (let it breathe)
- Energetic close with clear CTA

### Text Style
- Clean sans-serif font
- High contrast (white on dark, or dark on light)
- Animate text in/out (subtle fade or slide)

---

## Social Media / Short-Form Variants

### 15-Second Version
- 0:00-0:03: "What if every device crash went straight to the cloud?"
- 0:03-0:10: Button press → terminal activity → dashboard
- 0:10-0:15: "Nordic + Memfault. Live demo at CES."

### 30-Second Version
- 0:00-0:05: Problem statement
- 0:05-0:20: Demo flow (crash → upload → dashboard)
- 0:20-0:30: Auto-reconnect + CTA

---

**Last Updated:** 2025-12-15
**Version:** 1.0
**Contact:** [Your contact info]
