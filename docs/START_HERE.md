# Start Here

Read in order:
1. `AGENTS.md`
2. `docs/PROJECT_STATE.md`
3. `docs/OPS_RUNBOOK.md`
4. `docs/HARDWARE_SETUP.md`
5. `docs/BUILD_FLASH.md`
6. `docs/RESTORE_POINTS.md`
7. `docs/COMMAND_LOG.md`
8. `docs/TODO.md`

Current baseline:
- Golden tag: `GOLDEN_20260215_053739`
- Lock tag: `GOLDEN_LOCK_20260215_053739`
- Failsafe: `failsafe/edgeai_ev_charger_monitor_demo_cm33_core0_GOLDEN_20260215_053739.bin`

Primary objective:
- implement stable EV charger monitor sensing, state model, and UI loop on FRDM-MCXN947.

Known blocker at this baseline:
- Touchscreen AI on/off pill is still unreliable on target hardware and requires follow-up calibration/debug.
