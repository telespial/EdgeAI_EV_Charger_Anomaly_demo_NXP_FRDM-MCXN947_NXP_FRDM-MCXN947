# Restore Points

## Active Restore Point (Golden + Failsafe)
- Project: `EdgeAI_EV_Charger_Anomaly_demo_NXP_FRDM-MCXN947`
- Status: **FINAL GOLDEN** and **ACTIVE FAILSAFE**
- Golden ID: `GOLDEN_20260217_085851`
- Binary: `failsafe/edgeai_ev_charger_anomaly_demo_cm33_core0_GOLDEN_20260217_085851.bin`
- Checksum: `failsafe/edgeai_ev_charger_anomaly_demo_cm33_core0_GOLDEN_20260217_085851.sha256`
- Metadata: `failsafe/GOLDEN_20260217_085851_metadata.txt`

## Restore Command
```bash
LinkServer flash --probe '#1' --update-mode none MCXN947:FRDM-MCXN947 \
  load --addr 0x0 failsafe/edgeai_ev_charger_anomaly_demo_cm33_core0_GOLDEN_20260217_085851.bin
```

## Post-Restore Verification
- Dashboard boots and renders correctly
- AI pill touch toggle works
- Timeline L/R hour stepping works
- Profile switch (`WIRED|OUTLET`) works
- Gauge text and warning bar redraw only on state/value changes

## Policy
- This checkpoint is both golden and failsafe.
- Future restore points must update:
  - `docs/RESTORE_POINTS.md`
  - `docs/failsafe.md`
  - `docs/COMMAND_LOG.md`
