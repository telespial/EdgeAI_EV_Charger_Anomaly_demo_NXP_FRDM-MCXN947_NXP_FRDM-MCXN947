# TODO

- Implement EV charger telemetry acquisition module.
- Add state model (idle/charging/fault/complete).
- Tune cockpit gauge geometry/colors and optimize draw performance.
- Add UI rendering and alarms.
- Add UART/debug command path to update `PowerData_SetLiveOverride()` at runtime.
- Capture baseline validation run and create first golden restore point.
This is now a professional-grade demo architecture, not a hack.

System Overview

You are building a self-contained AI Power Conditioning Monitor demo with two operating modes:

Capture Mode (PC-connected)
Automated CLI-based energy measurement records real MCU current/power while running scripted workloads.

Replay Mode (Standalone)
The board replays recorded traces on its LCD and runs a local AI classifier to label power conditions in real time.

The demo can operate entirely standalone at a trade show while still being based on real measured data.

High-Level Architecture
              CAPTURE MODE
 ┌────────────────────────────────────┐
 │                                    │
 │   PC + MCUXpresso Energy CLI       │
 │        (live measurement)          │
 │                                    │
 │    CLI Capture Script              │
 │        ↓                           │
 │    CSV Energy Logs                 │
 └────────────────────────────────────┘
                ↓
         Trace Conversion Tool
                ↓
          Embedded Trace Pack
                ↓
              REPLAY MODE
 ┌────────────────────────────────────┐
 │ FRDM-MCXN947                       │
 │                                    │
 │  Replay Engine                     │
 │  Feature Extraction                │
 │  AI Classifier                     │
 │  LCD Dashboard                     │
 │                                    │
 └────────────────────────────────────┘

Capture Mode (Automated Energy Logging)
Objective

Generate high-quality, labeled power traces using real MCU workloads.

Step 1 — Workload Runner Firmware

Create a firmware called:

power_workload_runner


This firmware:

Cycles through defined workload scenes

Emits structured UART markers

Keeps each scene duration fixed

Scene Example Table
Scene ID	Name	Duration	Description
0	IDLE	10s	Low activity
1	UI_ANIM	15s	Heavy LCD refresh
2	AI_ONLY	15s	Continuous inference
3	AI_PLUS_UI	20s	Max load
4	BURST_CPU	15s	Periodic spikes
5	FFT_DSP	15s	Repeated FFT computation

Each scene prints:

SCENE_START,<id>,<name>,<timestamp>
SCENE_END,<id>,<timestamp>

Step 2 — CLI Energy Capture Script

Assume CLI command:

mcux_energy_capture \
   --board FRDM-MCXN947 \
   --duration 120 \
   --output capture.csv


Automation script:

#!/bin/bash

echo "Starting capture..."
mcux_energy_capture --duration 120 --output capture.csv &

sleep 2
python wait_for_uart_marker.py "RUNNING"

echo "Capture complete."


This gives you:

timestamp

current (mA)

power (mW)

voltage (optional)

Step 3 — Trace Processing Pipeline

Python tool:

trace_convert.py


Tasks:

Parse energy CSV

Parse UART markers

Segment by scene

Downsample to fixed timestep (e.g., 20 ms)

Convert to fixed-point integers

Emit trace_pack.h

Data Representation (Efficient)
typedef struct {
    uint16_t current_mA;
    uint16_t power_mW;
} trace_point_t;


Each scene:

const trace_point_t trace_idle[];
const trace_point_t trace_ai_only[];
...


Scene metadata:

typedef struct {
    const char* name;
    const trace_point_t* data;
    uint32_t length;
    uint16_t dt_ms;
} trace_scene_t;

Replay Mode (Standalone LCD Demo)
Core Modules
1. Replay Engine

Timer-driven (e.g., 20 ms tick)

Advances sample index

Outputs:

current_mA
power_mW


Supports:

loop scene

autoplay next scene

manual selection

pause/play

speed scaling

2. Feature Extraction Engine (10 Hz)

Compute sliding window features:

mean_current

stddev_current

peak_to_peak

spike_count

slope

energy_integral

These form the AI feature vector.

3. AI Classifier

Two phases:

Phase A (Deterministic Model)

Rules-based classifier:

Condition	Output Label
Low mean + low var	NORMAL_IDLE
High mean + low var	HIGH_LOAD_STABLE
High variance	BURSTY_LOAD
Frequent spikes	SPIKY_LOAD
Downward trend	POWER_RECOVERING

Confidence = distance from threshold.

Phase B (Optional ML Upgrade)

Replace rules with:

small dense NN

decision tree

or logistic regression

Inference cost extremely small.

4. LCD Dashboard
Layout

Top:

System Title

Mode: REPLAY

Left:

Current (mA)

Power (mW)

Energy (mJ cumulative)

Condition Label + Confidence

Right:

CPU Load (simulated per scene)

AI Inference Rate (per scene)

Graphics FPS (per scene)

Bottom:

10-second rolling strip chart

Event markers for spikes

Autoplay Demo Script

Cycle:

Idle (calm, green)

UI load (moderate)

AI only (stable high)

Combined load (peak)

Burst mode (spikes → red)

FFT mode (patterned spikes)

Return to idle

Transitions include:

fade effect

reset chart buffer

short overlay: “Scene: AI + Graphics”

Memory and Performance Considerations

If:

20 Hz replay

20 seconds per scene

6 scenes

Samples per scene:

20 Hz × 20 s = 400 samples


Total:

400 × 6 = 2400 samples


Each sample = 4 bytes
Total ≈ 9.6 KB

Very safe.

Demo Modes
Mode 1 — Standalone (Trade Show Default)

Replay traces

LCD only

No laptop required

Mode 2 — Live Capture Mode (Engineering Demo)

CLI capture running

Firmware streams live measurements to LCD

Toggle between Replay and Live

Optional Enhancements

Add “Anomaly Log” panel

Add Power Quality Score (0–100)

Add animated gauge needle

Add simulated thermal ramp during high load scenes

Add UART command interface for remote control

Why This Works

Uses real measured data

Zero board hardware modifications

Fully standalone display

AI component is real

Clean automation pipeline

Professional architecture

Final Deliverables

You will end up with:

power_workload_runner firmware

trace_convert.py

trace_pack.h

power_demo_dashboard firmware

CLI capture script

Documented demo script for presentation

If you’d like next, I can:

Write the exact trace_convert.py structure

Design the replay engine state machine

Or produce a file-by-file project architecture ready for implementation
