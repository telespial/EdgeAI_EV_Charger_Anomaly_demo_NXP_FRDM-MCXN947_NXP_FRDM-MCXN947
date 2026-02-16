# Project State

Last updated: 2026-02-15
Project: `EdgeAI_EV_Charger_Anomaly_demo_NXP_FRDM-MCXN947`

## Current Baseline
- Final golden/failsafe ID: `GOLDEN_20260215_170204`
- Failsafe binary: `failsafe/edgeai_ev_charger_anomaly_demo_cm33_core0_GOLDEN_20260215_170204.bin`
- Runtime: 20 Hz replay + on-device AI anomaly/decision loop
- UI refresh: 500 ms dashboard cadence with change-driven redraw
- Profiles: `WIRED` and `OUTLET` (12-hour behavior model)
- Timeline: hour-select playback (`1H`..`12H`) with per-hour looping

## AI Pipeline Summary
- Source file: `src/power_data_source.c`
- Core function: `UpdateAiModel`
- Inputs: V/I/P/T + derived EMA/slope/residual/drift features
- Outputs:
  - `ai_status` (`NORMAL/WARNING/FAULT`)
  - `ai_fault_flags` (sag/spike/unstable)
  - decisions (`WATCH`, `DERATE_15`, `DERATE_30`, `SHED_LOAD`)
  - risk metrics (connector, wire, thermal damage)
  - overtemp prediction horizons

## UI Rendering Contract (Current)
- No unnecessary gauge text redraws.
- Warning bar redraw only on warning visual state change.
- Scope legend redraw only when scope advances.
- Elapsed clock redraw when changed or potentially overdrawn.

## Name Migration
- Local project folder has been renamed from `...Monitor...` to `...Anomaly...`.
- Build uses `BUILD_DIR=mcuxsdk_ws/build_anomaly` to avoid old path cache conflicts.
