# Project State

Last updated: 2026-02-14
Project: `EdgeAI_EV_Charger_Monitor_demo_NXP_FRDM-MCXN947`

## Current Baseline
- Release baseline: initial scaffold published
- Golden tag: none yet
- Lock tag: none yet
- Failsafe image: none yet
- Lifecycle: active setup
- Initial commit: `ee3119e`

## Firmware Behavior (Current)
- Baseline app initializes board pins/clocks/debug console.
- UART startup marker prints on boot.
- Gauge style preset `cockpit_dark_v1` is defined (`src/gauge_style.*`) and logged at boot.
- EV charger monitor sensing/state/UI logic is pending implementation.

## Next Milestones
- Add EV charger signal acquisition path.
- Implement charger-state classifier and threshold model.
- Add display UI and warning states.
- Validate on hardware and publish first golden restore point.

## Update 2026-02-14
- Created standalone repository scaffold with docs, scripts, source tree, and MCUX overlay.
- Connected remote `origin` to GitHub and pushed `main`.

## Update 2026-02-14
- Added a sports-cockpit gauge style baseline based on the requested visual direction.
- Added `src/gauge_style.h` and `src/gauge_style.c` and wired the module into startup logging.
- Added style documentation in `docs/GAUGE_STYLE.md` and updated `docs/TODO.md`.
