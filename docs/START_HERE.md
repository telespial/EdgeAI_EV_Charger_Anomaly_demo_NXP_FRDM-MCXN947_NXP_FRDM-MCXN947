# Start Here

Read in order:
1. `AGENTS.md`
2. `README.md`
3. `docs/PROJECT_STATE.md`
4. `docs/OPS_RUNBOOK.md`
5. `docs/HARDWARE_SETUP.md`
6. `docs/BUILD_FLASH.md`
7. `docs/TEST_DATA_PIPELINE.md`
8. `docs/CAPTURE_MODE.md`
9. `docs/RESTORE_POINTS.md`
10. `docs/COMMAND_LOG.md`
11. `docs/TODO.md`

## Current Baseline
- Project: `EdgeAI_EV_Charger_Anomaly_demo_NXP_FRDM-MCXN947`
- Hardware: FRDM-MCXN947 + LCD-PAR-S035
- Runtime: replay telemetry @20 Hz + on-device AI decision logic
- UI: touch AI toggle, hour timeline buttons, profile switch (`WIRED|OUTLET`)

## Clarification
- AI decisions are real runtime firmware behavior.
- Default input stream is replay/simulated telemetry.
- This is a high-quality demo/prototype baseline, not a certified safety product.
