# Gauge Style

Gauge visual direction: `cockpit_dark_v1`

Reference intent:
- deep black panel base with metallic bezel rings
- bright white primary numerals with muted secondary text
- amber main needle and restrained red/green/blue accents
- mixed analog + compact digital strip layout

Implementation status:
- style preset and layout constants are defined in:
  - `src/gauge_style.h`
  - `src/gauge_style.c`
- preset is logged at boot from `src/edgeai_ev_charger_monitor_demo.c`

Next integration steps:
- render speed/power/battery rings using preset geometry
- map charger state (`idle/charging/fault/complete`) to accent bands
- add warning overlays using red accent and high-contrast text
