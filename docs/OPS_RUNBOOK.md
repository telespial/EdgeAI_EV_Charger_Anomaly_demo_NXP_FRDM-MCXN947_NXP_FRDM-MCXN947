# Ops Runbook

## Setup
```bash
./tools/bootstrap_ubuntu_user.sh
./tools/setup_mcuxsdk_ws.sh
```

## Build + Flash
```bash
BUILD_DIR=mcuxsdk_ws/build_anomaly ./tools/build_frdmmcxn947.sh debug
BUILD_DIR=mcuxsdk_ws/build_anomaly ./tools/flash_frdmmcxn947.sh
```

## Required Docs Update After Changes
Update these when behavior/build/runtime changes:
- `README.md`
- `docs/PROJECT_STATE.md`
- `STATUS.md`
- `docs/COMMAND_LOG.md`
- `docs/RESTORE_POINTS.md` (if baseline/restore point changes)
- `docs/failsafe.md` (if failsafe artifact changes)

## Operational Truth
- AI decisions are real runtime firmware decisions.
- Default telemetry source is replay/simulated data.
