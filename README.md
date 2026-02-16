# EdgeAI EV Charger Anomaly Demo (FRDM-MCXN947)

Edge AI anomaly-detection and decision demo for EV charging telemetry on **FRDM-MCXN947 + LCD-PAR-S035**.

![EV_charger_monitor](https://github.com/user-attachments/assets/a411539e-43e5-44ff-acb8-0f39ce5a6ef2)


## What This Project Does
- Runs a real-time on-device AI decision pipeline over charging telemetry.
- Visualizes charger state with analog gauges, timeline graph, warning bar, and touch controls.
- Demonstrates two realistic 12-hour charging profiles (`WIRED` and `OUTLET`) at 20 Hz.
- Supports hourly playback navigation (`1H`..`12H`) and profile switching in UI.

## What This Project Is Not
- Not a certified EVSE controller.
- Not a production safety product.
- Default data source is replay/simulated telemetry (live integration path is scaffolded but not default).

## How The AI Works (Specific)
AI logic lives in `src/power_data_source.c` (`UpdateAiModel`).

### 1) Input Features (20 Hz)
Per sample:
- Voltage (`voltage_mV`)
- Current (`current_mA`)
- Power (`power_mW`)
- Temperature (`temp_c`)

Derived online features:
- EMA voltage/current/power/temp
- Current step (`di`), power step (`dp`)
- Power residual vs expected (`P_measured - V*I`)
- Thermal slope and estimated thermal risk horizon
- Connector/wire thermal accumulation and wear trend
- Drift metric (actual thermal response vs expected thermal model)

### 2) Fault/Anomaly Signals
The model evaluates:
- `AI_FAULT_VOLTAGE_SAG`
- `AI_FAULT_CURRENT_SPIKE`
- `AI_FAULT_POWER_UNSTABLE`

It computes:
- `anomaly_score_pct`
- `thermal_risk_s`
- `predicted_overtemp_s`
- `connector_risk_pct`
- `wire_risk_pct`
- `thermal_damage_risk_pct`

### 3) Status Classification
AI status transitions:
- `NORMAL`
- `WARNING`
- `FAULT`

Using threshold logic + hold/latch behavior:
- warning/fault hold timers reduce flicker and status bounce
- startup/low-current guardrails suppress false instability at idle

### 4) Decision Layer
When AI assist is enabled:
- `WATCH`
- `DERATE_15`
- `DERATE_30`
- `SHED_LOAD`

Decision is based on anomaly severity, thermal trend/risk horizon, and fault combinations.
Decision output adjusts modeled current/power/thermal trajectory and records preventive events.

### 5) UI Rendering Contract
- Gauge text updates only when each gauge value changes.
- Warning bar redraws only when rendered warning state changes.
- Timeline/scope updates are change-driven to reduce raster flash.

## Replay Profiles
- `WIRED`: hardwired wall-connector style higher current, lower sag.
- `OUTLET`: outlet-style lower current with increased sag/thermal stress.

`1H` startup behavior:
- `0:00-2:00` handshake/idle (`0 A`, room temp)
- `2:00-12:00` staged startup tests with visible current pulses
- then ramp to full charge behavior

## Build And Flash
```bash
./tools/bootstrap_ubuntu_user.sh
./tools/setup_mcuxsdk_ws.sh
BUILD_DIR=mcuxsdk_ws/build_anomaly ./tools/build_frdmmcxn947.sh debug
BUILD_DIR=mcuxsdk_ws/build_anomaly ./tools/flash_frdmmcxn947.sh
```

## Restore / Failsafe
Latest golden/failsafe restore info is tracked in:
- `docs/RESTORE_POINTS.md`
- `docs/failsafe.md`

## Repository Layout
- `src/` firmware source
- `docs/` runbooks/state/restore docs
- `tools/` setup/build/flash utilities
- `data/` replay CSV assets
- `failsafe/` golden restore binaries + checksum + metadata
