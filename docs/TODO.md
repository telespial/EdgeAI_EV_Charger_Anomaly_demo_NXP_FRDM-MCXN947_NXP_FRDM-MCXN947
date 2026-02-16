# TODO

## Priority Next
- Add runtime source selector (`REPLAY` / `LIVE`) in UI.
- Add per-profile SoC presets (start/end/taper) configurable at runtime.
- Tune `WIRED` and `OUTLET` constants with captured real traces.
- Add concise operator guidance text tied to AI decisions.

## AI Hardening
- Add deterministic unit tests for:
  - status transitions and hold timers
  - fault flag generation
  - decision thresholds and derate actions
- Add startup false-positive suppression regression tests.

## Productization Path
- Integrate real sensing as primary input path.
- Add failsafe control outputs and watchdog strategy.
- Add compliance and long-run reliability validation workflow.

## Documentation Discipline
After any behavior change, update:
- `README.md`
- `docs/PROJECT_STATE.md`
- `STATUS.md`
- `docs/COMMAND_LOG.md`
- `docs/RESTORE_POINTS.md`
