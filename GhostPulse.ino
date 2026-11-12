/*
 * MIT License
 *
 * Copyright (c) 2026 Frank Jennings
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * ===================================================================================
 * PROJECT: GhostPulse - Cross-Platform USB HID Keep-Awake Controller (Arduino Uno R4)
 * AUTHOR:  Frank Jennings
 * ===================================================================================
 *
 * KEY FEATURES:
 * 1. Cross-Platform OS Support (Windows, macOS, Linux X11/Wayland):
 *    - Uses native Renesas RA4M1 USB HID hardware emulation (Mouse & Keyboard).
 *    - Alternates between three non-intrusive, zero-net-displacement actions:
 *      a) 2-step curved micro-mouse jiggle with exact coordinate return.
 *      b) Harmless Left Shift modifier tap (resets idle timers without typing).
 *      c) Micro scroll-wheel up/down nudge (0 net scroll change).
 * 2. Human-Like Randomized Intervals:
 *    - Triggers randomly between 30s and 110s to avoid robotic detection patterns.
 * 3. Smooth Non-Blocking Quadratic LED Breathing:
 *    - Uses PWM on Pin 13 (LED_BUILTIN) to smoothly fade in and out over 3s
 *      without blocking button presses or serial telemetry.
 * 4. Multi-Function Tactile Push Button (Pin 2 to GND):
 *    - Short Tap   (< 0.8s)      : Toggle Pause / Resume (dims LED when paused).
 *    - Medium Hold (0.8s - 2.0s) : Trigger immediate manual keep-awake pulse.
 *    - Long Hold   (> 2.0s)      : Toggle Passive Buzzer Mute On / Off.
 * 5. Passive Buzzer Acoustic Feedback (Pin 8 to GND):
 *    - Distinct melodic chimes for Startup, Pulse, Pause, Resume, and Mute.
 * 6. Serial Monitor Telemetry & Control (115200 baud):
 *    - Displays live pulse counts and countdown timers.
 *    - Accepts 'p' (manual pulse), 'm' (toggle mute), and 's' (print stats).
 *
 * HARDWARE WIRING:
 * - Tactile Button : Connect diagonally opposite legs to Pin 2 and GND.
 * - Passive Buzzer : Connect (+) leg to Pin 8 and (-) leg to GND.
 * - Built-in LED   : Uses onboard Pin 13 (no external wiring needed).
 * ============================================================================
 */

#include <Mouse.h>     // Native USB HID Mouse library for Arduino Uno R4
#include <Keyboard.h>  // Native USB HID Keyboard library for Arduino Uno R4

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
const int LED_PIN    = LED_BUILTIN; // Pin 13 (Supports hardware PWM on Uno R4)
const int BUTTON_PIN = 2;           // 4-leg tactile push button (Pin 2 <-> GND)
const int BUZZER_PIN = 8;           // Passive piezo buzzer (+ on Pin 8, - on GND)

// ============================================================================
// TIMING & THRESHOLD CONSTANTS
// ============================================================================
const unsigned long MIN_INTERVAL     = 30000;  // Minimum wait between pulses (30s)
const unsigned long MAX_INTERVAL     = 110000; // Maximum wait between pulses (110s)
const unsigned long BREATHE_DUR_MS   = 3000;   // Duration of LED breathing cycle (3s)
const unsigned long DEBOUNCE_MS      = 30;     // Minimum press time to filter noise
const unsigned long MED_HOLD_MS      = 800;    // Hold time threshold for manual pulse
const unsigned long LONG_HOLD_MS     = 2000;   // Hold time threshold for mute toggle

// ============================================================================
// GLOBAL STATE VARIABLES
// ============================================================================
unsigned long previousMillis  = 0;     // Timestamp of the last keep-awake pulse
unsigned long currentInterval = 15000; // Initial delay after boot (15 seconds)
unsigned long totalPulses     = 0;     // Counter for total pulses sent since boot

bool isPaused  = false;                // Tracks whether keep-awake is paused
bool soundMute = false;                // Tracks whether buzzer audio is muted

// --- Non-Blocking LED Breathing State ---
bool isBreathing = false;              // True while LED fade animation is active
unsigned long breatheStartMillis = 0;  // Timestamp when current breath began

// --- Button State & Press Duration Tracking ---
int lastButtonState = HIGH;            // Previous reading of BUTTON_PIN (pull-up)
unsigned long pressStartTime = 0;      // Timestamp when button was pressed down
bool buttonHandled = false;            // Prevents re-triggering during a single hold

// ============================================================================
// SETUP: Runs once at power-on or reset
// ============================================================================
void setup() {
  // Configure hardware pin modes
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // Enable internal 20k-50k pull-up resistor
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize USB Serial for debugging and live telemetry
  Serial.begin(115200);

  // Seed pseudo-random generator with floating analog noise from pin A0
  randomSeed(analogRead(A0));

  // Initialize USB HID endpoints for Mouse and Keyboard
  Mouse.begin();
  Keyboard.begin();

  // Play startup arpeggio and trigger an initial 3-second LED breath
  playChimeStartup();
  startBreathing();
}

// ============================================================================
// MAIN LOOP: Runs continuously at full processor speed (non-blocking)
// ============================================================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. Poll tactile push button for short tap, medium hold, or long hold
  handleButton(currentMillis);

  // 2. Step through non-blocking PWM brightness curve for the built-in LED
  updateLEDBreathing(currentMillis);

  // 3. Check if user sent any interactive commands via Serial Monitor
  handleSerialCommands();

  // 4. If system is paused, hold LED at a dim 8% standby glow and halt pulses
  if (isPaused) {
    analogWrite(LED_PIN, 20); // 20 out of 255 PWM duty cycle (~8% brightness)
    return;
  }

  // 5. Check if the randomized countdown interval has elapsed
  if (currentMillis - previousMillis >= currentInterval) {
    previousMillis = currentMillis; // Reset timer reference
    totalPulses++;                  // Increment lifetime pulse counter

    // Execute cross-platform HID activity, audio cue, and LED pulsation
    performKeepAwakeAction();
    playChimePulse();
    startBreathing();

    // Pick a new randomized wait interval for the next cycle
    currentInterval = random(MIN_INTERVAL, MAX_INTERVAL);

    // Log status to Serial Monitor
    Serial.print("Pulse #");
    Serial.print(totalPulses);
    Serial.print(" sent | Next pulse in ");
    Serial.print(currentInterval / 1000);
    Serial.println("s");
  }
}

// ============================================================================
// BUTTON HANDLER: Evaluates press duration for 3 distinct functions
// ============================================================================
void handleButton(unsigned long now) {
  int reading = digitalRead(BUTTON_PIN);

  // Detect falling edge (button just pressed down; LOW due to INPUT_PULLUP)
  if (reading == LOW && lastButtonState == HIGH) {
    pressStartTime = now;
    buttonHandled = false;
    delay(20); // Brief hardware debounce settle time
  }

  // Evaluate Long Hold (> 2.0s) in real time while button is still held down
  if (reading == LOW && !buttonHandled) {
    unsigned long holdTime = now - pressStartTime;
    if (holdTime >= LONG_HOLD_MS) {
      buttonHandled = true;     // Mark event handled so release won't re-trigger
      soundMute = !soundMute;   // Flip mute state

      // Provide immediate audio (low tone = muted, high tone = unmuted) & LED cue
      tone(BUZZER_PIN, soundMute ? 400 : 1200, 120);
      startBreathing();

      Serial.println(soundMute ? "[AUDIO] Buzzer Muted" : "[AUDIO] Buzzer Unmuted");
    }
  }

  // Detect rising edge (button released before reaching Long Hold threshold)
  if (reading == HIGH && lastButtonState == LOW && !buttonHandled) {
    unsigned long pressDuration = now - pressStartTime;
    buttonHandled = true;

    if (pressDuration >= MED_HOLD_MS) {
      // --- MEDIUM HOLD (0.8s - 2.0s): Force Immediate Keep-Awake Pulse ---
      Serial.println("[MANUAL] Forced keep-awake pulse triggered!");
      performKeepAwakeAction();
      playChimePulse();
      startBreathing();
      previousMillis = now; // Reset automatic countdown timer
    } 
    else if (pressDuration >= DEBOUNCE_MS) {
      // --- SHORT TAP (< 0.8s): Toggle Pause / Resume ---
      isPaused = !isPaused;
      if (isPaused) {
        isBreathing = false;    // Cancel any active breathing animation
        playChimePause();
        Serial.println("[STATE] System Paused");
      } else {
        analogWrite(LED_PIN, 0); // Clear dim standby glow
        previousMillis = now;    // Reset countdown timer upon resuming
        playChimeResume();
        startBreathing();
        Serial.println("[STATE] System Resumed");
      }
    }
  }

  // Save current state for edge detection on next loop iteration
  lastButtonState = reading;
}

// ============================================================================
// NON-BLOCKING LED BREATHING ENGINE
// ============================================================================
void startBreathing() {
  isBreathing = true;
  breatheStartMillis = millis();
}

void updateLEDBreathing(unsigned long now) {
  if (!isBreathing) return;

  unsigned long elapsed = now - breatheStartMillis;

  // Stop animation and turn off LED once full duration has elapsed
  if (elapsed >= BREATHE_DUR_MS) {
    isBreathing = false;
    analogWrite(LED_PIN, 0);
    return;
  }

  // Normalize elapsed time into a 0.0 -> 1.0 progress ratio
  float progress = (float)elapsed / (float)BREATHE_DUR_MS;

  // Convert linear progress into a symmetric triangle wave (0.0 -> 1.0 -> 0.0)
  float triangle = (progress < 0.5f) ? (progress * 2.0f) : ((1.0f - progress) * 2.0f);

  // Apply quadratic curve (triangle^2) to match human logarithmic eye perception
  int brightness = (int)(triangle * triangle * 255.0f);

  analogWrite(LED_PIN, brightness);
}

// ============================================================================
// CROSS-PLATFORM USB HID ROUTINES (Windows, macOS, Linux)
// ============================================================================
void performKeepAwakeAction() {
  // Randomly select one of three non-intrusive HID routines
  int actionType = random(0, 3);

  switch (actionType) {
    case 0: {
      // ROUTINE A: 2-Step Curved Micro-Jiggle with Exact Coordinate Return
      // Moves cursor in two tiny random vectors, then subtracts the exact sum
      // so the pointer returns to its original pixel without drift.
      int dx1 = random(-2, 3);
      int dy1 = random(-2, 3);
      if (dx1 == 0 && dy1 == 0) dx1 = 1; // Guarantee at least 1px movement

      int dx2 = random(-1, 2);
      int dy2 = random(-1, 2);

      Mouse.move(dx1, dy1, 0);
      delay(random(35, 65));             // Natural human-like micro-pause
      Mouse.move(dx2, dy2, 0);
      delay(random(35, 65));
      Mouse.move(-(dx1 + dx2), -(dy1 + dy2), 0); // Return to origin
      break;
    }

    case 1: {
      // ROUTINE B: Harmless Left Shift Modifier Tap
      // Resets OS IOKit/Win32/libinput idle timers without typing characters,
      // triggering shortcuts, or toggling Caps/Num Lock indicators.
      Keyboard.press(KEY_LEFT_SHIFT);
      delay(random(35, 75));
      Keyboard.release(KEY_LEFT_SHIFT);
      break;
    }

    case 2: {
      // ROUTINE C: Zero-Net Scroll Wheel Nudge
      // Scrolls 1 notch up and immediately 1 notch down. Reliably keeps browser
      // sessions and active desktop windows awake with zero net scroll shift.
      Mouse.move(0, 0, 1);
      delay(random(40, 80));
      Mouse.move(0, 0, -1);
      break;
    }
  }
}

// ============================================================================
// PASSIVE BUZZER ACOUSTIC FEEDBACK ROUTINES
// ============================================================================
void playChimeStartup() {
  if (soundMute) return;
  tone(BUZZER_PIN, 523, 90);  delay(100); // C5
  tone(BUZZER_PIN, 659, 90);  delay(100); // E5
  tone(BUZZER_PIN, 784, 140); delay(150); // G5 (Major triad arpeggio)
  noTone(BUZZER_PIN);
}

void playChimePulse() {
  if (soundMute) return;
  // Subtle, high-pitched double-tick confirmation
  tone(BUZZER_PIN, 1400, 25); delay(45);
  tone(BUZZER_PIN, 1800, 25); delay(30);
  noTone(BUZZER_PIN);
}

void playChimePause() {
  if (soundMute) return;
  // Descending two-tone chime (E5 -> A4) indicating standby
  tone(BUZZER_PIN, 659, 90);  delay(100);
  tone(BUZZER_PIN, 440, 140); delay(150);
  noTone(BUZZER_PIN);
}

void playChimeResume() {
  if (soundMute) return;
  // Ascending two-tone chime (A4 -> E5) indicating active state
  tone(BUZZER_PIN, 440, 90);  delay(100);
  tone(BUZZER_PIN, 659, 140); delay(150);
  noTone(BUZZER_PIN);
}

// ============================================================================
// SERIAL MONITOR INTERACTIVE COMMANDS (115200 BAUD)
// ============================================================================
void handleSerialCommands() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    if (cmd == 'p' || cmd == 'P') {
      // Force an immediate pulse via Serial Monitor
      Serial.println("[SERIAL] Manual pulse triggered");
      performKeepAwakeAction();
      playChimePulse();
      startBreathing();
    } 
    else if (cmd == 'm' || cmd == 'M') {
      // Toggle buzzer mute via Serial Monitor
      soundMute = !soundMute;
      Serial.println(soundMute ? "[SERIAL] Buzzer Muted" : "[SERIAL] Buzzer Unmuted");
    } 
    else if (cmd == 's' || cmd == 'S') {
      // Print uptime and pulse telemetry
      Serial.print("[STATS] Uptime: ");
      Serial.print(millis() / 60000);
      Serial.print(" mins | Total Pulses: ");
      Serial.print(totalPulses);
      Serial.print(" | State: ");
      Serial.println(isPaused ? "PAUSED" : "ACTIVE");
    }
  }
}
    