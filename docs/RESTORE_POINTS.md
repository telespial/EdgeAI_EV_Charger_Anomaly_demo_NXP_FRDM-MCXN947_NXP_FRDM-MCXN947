# Restore Points

## 2026-02-15 Golden Baseline
- Golden tag: `GOLDEN_20260215_053739`
- Lock tag: `GOLDEN_LOCK_20260215_053739`
- Failsafe binary: `failsafe/edgeai_ev_charger_monitor_demo_cm33_core0_GOLDEN_20260215_053739.bin`
- Failsafe checksum: `failsafe/edgeai_ev_charger_monitor_demo_cm33_core0_GOLDEN_20260215_053739.sha256`
- Metadata: `failsafe/GOLDEN_20260215_053739_metadata.txt`

### Baseline Contents
- Current EV charger cockpit UI and telemetry loop at 20 Hz.
- Solid green `SYSTEM STABLE` status box behavior in normal AI state.
- Warning/fault status remains flashing-only behavior.
- Touchscreen AI on/off pill remains a known unresolved issue at this checkpoint.

### Restore Steps
1. Use the failsafe binary directly.
2. Flash:
   `LinkServer flash --probe '#1' --update-mode none MCXN947:FRDM-MCXN947 load --addr 0x0 failsafe/edgeai_ev_charger_monitor_demo_cm33_core0_GOLDEN_20260215_053739.bin`
3. Verify LCD and UART startup, then continue touch-debug work from this baseline.

## 2026-02-14 Golden Baseline
- Golden tag: `GOLDEN_20260214_203015`
- Lock tag: `GOLDEN_LOCK_20260214_203015_22b87ce`
- Failsafe binary: `failsafe/edgeai_ev_charger_monitor_demo_cm33_core0_GOLDEN_20260214_203015.bin`
- Failsafe checksum: `failsafe/edgeai_ev_charger_monitor_demo_cm33_core0_GOLDEN_20260214_203015.sha256`
- Metadata: `failsafe/GOLDEN_20260214_203015_metadata.txt`

### Baseline Contents
- 12-minute replay dataset at 20 Hz (`1 minute = 1 hour simulated charge time`).
- Profiles generated: normal, wear, fault (`data/ev_charge_12min_*_20hz.csv`).
- Default firmware replay sourced from wear profile (`data/replay_trace.csv` -> `src/replay_trace_generated.h`).
- AI connector-wear anomaly scoring and terminal status readouts.
- Elapsed charge clock rendered inside center gauge.
- Oscilloscope lines include random noise jitter for realistic trace behavior.

### Restore Steps
1. Build from current baseline or use the failsafe binary directly.
2. Flash:
   `LinkServer flash --probe '#1' --update-mode none MCXN947:FRDM-MCXN947 load --addr 0x0 failsafe/edgeai_ev_charger_monitor_demo_cm33_core0_GOLDEN_20260214_203015.bin`
3. Verify UART samples and LCD dashboard behavior.
