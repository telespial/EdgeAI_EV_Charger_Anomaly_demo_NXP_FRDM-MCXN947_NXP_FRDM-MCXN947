# Failsafe Baseline

Active failsafe baseline:
- `GOLDEN_20260215_170204`
- `failsafe/edgeai_ev_charger_anomaly_demo_cm33_core0_GOLDEN_20260215_170204.bin`

Associated files:
- checksum: `failsafe/edgeai_ev_charger_anomaly_demo_cm33_core0_GOLDEN_20260215_170204.sha256`
- metadata: `failsafe/GOLDEN_20260215_170204_metadata.txt`

## Flash
```bash
LinkServer flash --probe '#1' --update-mode none MCXN947:FRDM-MCXN947 \
  load --addr 0x0 failsafe/edgeai_ev_charger_anomaly_demo_cm33_core0_GOLDEN_20260215_170204.bin
```

## Intent
This is the final golden demo checkpoint and the fast recovery fallback image.
