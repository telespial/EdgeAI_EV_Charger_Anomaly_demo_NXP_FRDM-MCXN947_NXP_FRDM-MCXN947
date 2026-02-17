# EdgeAI EV Charger Anomaly Demo (FRDM-MCXN947)

Edge AI EV charging anomaly and mitigation demo for **FRDM-MCXN947 + LCD-PAR-S035**.

![EV_charger_monitor](https://github.com/user-attachments/assets/a411539e-43e5-44ff-acb8-0f39ce5a6ef2)

## Scope
- Runs a real-time on-device anomaly engine at `20 Hz` (`50 ms` sample period).
- Shows charger state on gauges, timeline plot, status terminal, and warning banner.
- Replays realistic `12-hour` charging sessions with touch-selectable `WIRED` and `OUTLET` profiles.
- Supports timeline hour stepping (`1H`..`12H`) with looped playback inside the selected hour.

## Safety And Product Status
- This is a demo/reference implementation.
- This is not a certified EVSE controller and not a production safety system.
- Default input is replay telemetry. Live sensor override hooks exist but are not the default runtime path.

## AI Engine (Exact Behavior)
Core logic is in `src/power_data_source.c`, function `UpdateAiModel(...)`.

### 1) Input And Timing
- Tick rate: `20 Hz`.
- Base signals per sample:
- `voltage_mV`
- `current_mA`
- `power_mW`
- `temp_c`

### 2) Derived Features
- EMA baselines for voltage/current/power/temperature.
- Instant deltas: `dv`, `di`, `dp`.
- Residual power error: measured power vs expected `V*I`.
- Thermal slope and time-to-threshold estimates.
- Connector drift/wear trend and thermal stress accumulation.

### 3) Fault Flags
The engine raises bit-flags when conditions are met:
- `AI_FAULT_VOLTAGE_SAG`
- `AI_FAULT_CURRENT_SPIKE`
- `AI_FAULT_POWER_UNSTABLE`

It also computes:
- `anomaly_score_pct`
- `thermal_risk_s`
- `predicted_overtemp_s`
- `connector_risk_pct`
- `wire_risk_pct`
- `thermal_damage_risk_pct`

### 4) Status Classification
Status is classified as:
- `NORMAL`
- `WARNING`
- `FAULT`

Classification uses threshold combinations plus hold/latch timers to avoid flicker and immediate bounce back to green after transient events.

### 5) AI Decisions (AI ON)
With AI assist enabled (`PowerData_SetAiAssistEnabled(true)`), the engine chooses:
- `WATCH`
- `DERATE_15`
- `DERATE_30`
- `SHED_LOAD`

These decisions actively modify modeled output (current/power/thermal trajectory), apply mitigation, and increment preventive-event counters when a severe pre-mitigation fault is avoided.

### 6) Rule Mode (AI OFF)
With AI assist disabled, decision output is forced to `NONE` and UI uses rule-based status/warnings from deterministic thresholds (temperature/power limits). No active mitigation derating is applied by the AI layer.

## Warning Text Contract
Warning banner headline:
- AI ON: `EDGEAI WARNING`
- AI OFF: `ALGO WARNING`

Second-line detail text set:
- AI ON:
- `VOLTAGE SAG`
- `CURRENT SPIKE`
- `POWER UNSTABLE`
- `THERMAL RISK %us`
- `CONNECTOR DRIFT` (OUTLET mode only)
- AI OFF:
- `RULE SHUTDOWN TEMP`
- `RULE OVERCURRENT`
- `RULE NEAR OVERTEMP`
- `RULE OVERCURRENT WARN`
- `RULE Analyzing....` (fallback)
- `RULE Fault Check` (rare fallback)

## Replay Profiles
- `WIRED`: hardwired wall-connector behavior, higher sustained current, lower sag/drift sensitivity.
- `OUTLET`: lower sustained current with greater sag/thermal drift sensitivity and connector drift handling.

`1H` startup sequence:
- `0:00-0:30`: handshake/idle at room temperature (`0 A`).
- `0:30-10:30`: staged startup validation pulses and current tests.
- Then ramp to bulk charging, later taper, then idle/cool.

## Build And Flash
```bash
./tools/bootstrap_ubuntu_user.sh
./tools/setup_mcuxsdk_ws.sh
BUILD_DIR=mcuxsdk_ws/build_anomaly ./tools/build_frdmmcxn947.sh debug
BUILD_DIR=mcuxsdk_ws/build_anomaly ./tools/flash_frdmmcxn947.sh
```

## Restore / Failsafe
- `docs/RESTORE_POINTS.md`
- `docs/failsafe.md`

## Repository Layout
- `src/` firmware source
- `docs/` runbooks, restore/failsafe docs
- `tools/` setup/build/flash scripts
- `data/` replay assets
- `failsafe/` restore binaries + checksums + metadata
