# EdgeAI EV Charger Monitor Demo (FRDM-MCXN947)

This repository is a standalone firmware project scaffold for an EV charger monitor demo on NXP FRDM-MCXN947.

Target hardware:
- FRDM-MCXN947
- LCD-PAR-S035 display (optional UI)
- EV charger input sensing hardware (project-specific integration pending)

## Project Status
- Repository initialized with self-contained docs, scripts, and MCUX SDK overlay wiring.
- Firmware app is currently a bring-up baseline that prints a startup banner.
- Gauge style baseline `cockpit_dark_v1` has been defined for a sports-cockpit look.
- Replay-based test data pipeline is in place (`data/replay_trace.csv` -> `src/replay_trace_generated.h`).
- First cockpit-style LCD dashboard render is implemented and flash-validated.
- EV charger runtime logic is pending implementation.

## Quickstart
```bash
./tools/bootstrap_ubuntu_user.sh
./tools/setup_mcuxsdk_ws.sh
./tools/build_frdmmcxn947.sh debug
./tools/flash_frdmmcxn947.sh
```

## Repo Layout
- `src/`: project firmware sources
- `sdk_example/`: MCUX SDK examples overlay
- `tools/`: setup/build/flash scripts
- `docs/`: project state, runbook, restore points, and logs

Style references:
- `docs/GAUGE_STYLE.md`
- `src/gauge_style.h`

Test-data references:
- `docs/TEST_DATA_PIPELINE.md`
- `src/power_data_source.h`
