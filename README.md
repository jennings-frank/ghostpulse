# GhostPulse

**A smart, cross-platform hardware keep-awake controller built for the Arduino Uno R4.**

GhostPulse prevents host computers (Windows, macOS, and Linux) from entering idle sleep or triggering the lockscreen by emulating genuine, non-intrusive USB Human Interface Device (HID) activity. Equipped with a smooth quadratic breathing LED, acoustic buzzer chimes, and a multi-function tactile push button, it sits discreetly on your desk without interfering with your workflow.

---

## Understanding USB HID Support (Hardware Requirement)

To simulate a physical mouse or keyboard at the hardware level, a microcontroller must support **native USB HID (Human Interface Device)** communication. 

* **Why standard Uno boards won't work:** Classic boards like the **Arduino Uno R3** rely on a secondary USB-to-Serial bridge chip (such as the ATmega16U2 or CH340) that only presents the board to your computer as a virtual COM/Serial port—meaning it cannot natively act as a keyboard or mouse without complex firmware reflashing.
* **Why the Arduino Uno R4 works out of the box:** Both the **Arduino Uno R4 Minima** and **Arduino Uno R4 WiFi** are powered by the 32-bit **Renesas RA4M1** microcontroller, which features an integrated native USB controller. Using the standard `Mouse.h` and `Keyboard.h` libraries, the Uno R4 enumerates directly as a plug-and-play composite USB HID peripheral across all major operating systems—requiring **zero drivers or host-side software**.

---

## Key Features

* **Cross-Platform OS Compatibility (Windows, macOS, Linux X11/Wayland):**
  * **Curved 2-Step Micro-Jiggle:** Moves the cursor in two tiny randomized vectors and subtracts the exact delta to return to the starting pixel—eliminating cursor drift.
  * **Harmless Modifier Tap:** Sends a brief `KEY_LEFT_SHIFT` press/release to reset OS idle timers (`Win32`, `IOKit`, `libinput`) without typing characters or toggling Caps/Num Lock.
  * **Zero-Net Scroll Nudge:** Scrolls `1` tick up and immediately `1` tick down to keep active browser tabs and desktop windows awake with zero net scroll displacement.
* **Human-Like Randomized Intervals:** Fires at randomized intervals between **30 and 110 seconds** to avoid predictable, robotic timing patterns.
* **Non-Blocking Quadratic LED Breathing:** Uses PWM on the onboard LED (`Pin 13`) to produce a smooth, eye-friendly 3-second fade-in and fade-out during every pulse without blocking button inputs.
* **Multi-Function Tactile Push Button:** Single-button control supporting short taps, medium holds, and long holds.
* **Acoustic Feedback (Passive Buzzer):** Distinct melodic chimes for startup, keep-awake pulses, pause/resume states, and mute toggles.
* **Serial Monitor Telemetry & Control:** Live countdowns, uptime statistics, and interactive keyboard shortcuts over USB Serial (`115200` baud).

---

## Hardware Requirements & Wiring

### Bill of Materials
* **1x** Arduino Uno R4 (Minima or WiFi)
* **1x** USB-C Data Cable (ensure it supports data transfer, not charge-only)
* **1x** 4-Leg Tactile Push Button
* **1x** Passive Piezo Buzzer *(must be a passive buzzer to generate variable frequencies via `tone()`)*
* **3x** Jumper Wires (and a small breadboard, optional)

### Pinout & Wiring Table

| Component | Arduino Pin | Ground / Power | Notes |
| :--- | :--- | :--- | :--- |
| **Tactile Push Button** | **Pin 2** | **GND** | Connect diagonally opposite legs. Uses internal `INPUT_PULLUP` (no external resistor needed). |
| **Passive Buzzer** | **Pin 8** (`+`) | **GND** (`-`) | Connect positive leg to Pin 8 and negative leg to GND. |
| **Built-in LED** | **Pin 13** | — | Uses the onboard `LED_BUILTIN` directly. |

---

## Getting Started

### 1. Install the Uno R4 Board Package
1. Open the **Arduino IDE** (v2.0 or newer recommended).
2. Navigate to **Tools > Board > Boards Manager...** (or click the Boards Manager icon in the left sidebar).
3. Search for **`Arduino UNO R4 Boards`** and click **Install**.

### 2. Upload the Sketch
1. Connect your Arduino Uno R4 to your computer via a USB-C data cable.
2. Select your board under **Tools > Board > Arduino UNO R4 Boards** (`Arduino UNO R4 Minima` or `Arduino UNO R4 WiFi`) and choose the corresponding **Port**.
3. Open `GhostPulse.ino`, click **Upload**, and wait for the startup triad chime and initial 3-second LED breath.

> **Tip:** If you ever modify the sketch with very short delays and have trouble uploading new code, **double-tap the RESET button** on the Uno R4 to force it into bootloader mode.

---

## Controls & Usage

### Tactile Button Gestures
| Gesture | Duration | Action | Feedback |
| :--- | :--- | :--- | :--- |
| **Short Tap** | `< 0.8s` | **Pause / Resume** | Descending chime + steady 8% dim LED glow when paused; ascending chime when resumed. |
| **Medium Hold** | `0.8s – 2.0s` | **Manual Pulse** | Immediately triggers a HID pulse, plays double-tick chime, breathes LED, and resets timer. |
| **Long Hold** | `> 2.0s` | **Mute / Unmute** | Single confirmation beep (low pitch = muted, high pitch = unmuted) + LED breath. |

### Serial Monitor Commands (`115200` Baud)
Open the Serial Monitor in the Arduino IDE at **`115200` baud** to view live pulse logs or send single-character commands:
* **`p`** — Trigger an immediate manual keep-awake pulse.
* **`m`** — Toggle passive buzzer mute on or off.
* **`s`** — Display total uptime (in minutes), total pulses sent, and current active/paused state.

---

## Customization

You can easily adjust timing and button thresholds at the top of the sketch:

```cpp
const unsigned long MIN_INTERVAL   = 30000;  // Minimum wait between pulses (ms)
const unsigned long MAX_INTERVAL   = 110000; // Maximum wait between pulses (ms)
const unsigned long BREATHE_DUR_MS = 3000;   // Duration of LED breathing cycle (ms)
