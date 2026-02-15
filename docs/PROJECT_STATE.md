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
- Replay test samples feed the runtime data path via `src/power_data_source.*`.
- Cockpit dashboard frame now renders on LCD using `src/gauge_render.c`.
- EV charger monitor sensing/state classifier and alarm logic are still pending.

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

## Update 2026-02-14
- Added editable replay trace input at `data/replay_trace.csv`.
- Added converter `tools/trace_csv_to_header.py` generating `src/replay_trace_generated.h`.
- Added runtime data abstraction in `src/power_data_source.h` and `src/power_data_source.c`.
- Wired sample output logging in `src/edgeai_ev_charger_monitor_demo.c`.
- Added pipeline documentation in `docs/TEST_DATA_PIPELINE.md`.

## Update 2026-02-15
- Added first cockpit-style LCD renderer (`src/gauge_render.c`) with rings, needles, and numeric fields.
- Imported display/text primitives (`src/par_lcd_s035.*`, `src/text5x7.*`) and expanded glyph coverage.
- Updated demo startup loop to render live replay samples to LCD.
- Build succeeded and flash completed via:
  - workspace: `.../EdgeAI_sphere_demo.../mcuxsdk_ws`
  - build dir: `build_ev_charger_try3`

## Update 2026-02-15
- Fixed a render cadence bug that could appear as a blank screen.
- `src/edgeai_ev_charger_monitor_demo.c` now draws the gauge frame every loop iteration.
- Rebuilt and reflashed `build_ev_charger_try3` after the fix.

## Update 2026-02-15
- Fixed LCD bring-up initialization path:
  - switched to `BOARD_InitHardware()` in `src/edgeai_ev_charger_monitor_demo.c`
  - included `app.h` for board init declaration
- Fixed UART formatting in `src/gauge_style.c` (`\r\n` handling).
- Rebuilt and reflashed `build_ev_charger_try3`.
- Verified UART boot markers include:
  - `Gauge render: ready`
  - `Power data source: replay`

## Update 2026-02-15
- Reduced visible raster flashing in gauge UI:
  - `src/gauge_render.c` now keeps static dashboard background persistent
  - only dynamic gauge/value regions are cleared and redrawn per update
  - full-screen clear per frame removed
- `src/edgeai_ev_charger_monitor_demo.c` now redraws only when sample values change.
- Rebuilt and reflashed `build_ev_charger_try3` with anti-flicker update.

## Update 2026-02-15
- Fixed gauge/value text overlap by moving numeric readouts away from ring geometry.
- Switched dynamic rendering to incremental updates:
  - erase/redraw needle only when index changes
  - clear/redraw value bands only when value changes
- Slowed replay stepping (`POWER_REPLAY_TICK_DIVIDER`) to reduce gauge thrash.
- Rebuilt and reflashed `build_ev_charger_try3`.

## Update 2026-02-15
- Added capture-mode tooling for real board energy traces:
  - `tools/capture_energy_trace.sh` (energy CSV + UART markers)
  - `tools/trace_convert.py` (energy CSV -> `data/replay_trace.csv`)
  - `docs/CAPTURE_MODE.md` workflow
- Environment check: `mcux_energy_capture` binary is not installed on this machine.
- Capture script supports external command template via `ENERGY_CAPTURE_CMD`.
