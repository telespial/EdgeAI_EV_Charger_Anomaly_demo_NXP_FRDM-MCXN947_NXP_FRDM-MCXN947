# Test Data Pipeline

## Runtime Model
- Sample rate: 20 Hz
- Session span: 12-hour cycle
- Profiles:
  - `WIRED` (hardwired-style)
  - `OUTLET` (outlet-style)

## Source Implementation
Procedural replay generation in:
- `src/power_data_source.c`
  - `ReplayProfileCfg`
  - `SampleFromReplay`

Legacy CSV/header flow is still available:
- `data/replay_trace.csv`
- `tools/trace_csv_to_header.py`
- `src/replay_trace_generated.h`

## Startup Behavior in `1H`
- 0-30 sec: idle/handshake at room temp, 0 A
- 0:30-10:30 min: staged startup testing with visible current pulses
- then transition to charge ramp and bulk profile behavior

## Verify Workflow
1. Edit profile logic in `src/power_data_source.c`
2. Build and flash
3. Validate gauge, warning, and AI decision behavior on board
