# Capture Mode (Live Telemetry Path)

## Goal
Collect real voltage/current/temperature telemetry and feed the same AI pipeline used in replay mode.

## Current State
- Default mode: replay/simulated telemetry
- Live telemetry path: scaffolded tooling, not default runtime mode

## Tools
- `tools/capture_energy_trace.sh`
- `tools/capture_uart_telemetry.sh`
- `tools/trace_convert.py`

## Example
```bash
./tools/capture_energy_trace.sh --duration 120
./tools/capture_uart_telemetry.sh --uart /dev/ttyACM0 --duration 60
```

## Convert To Replay CSV
```bash
python3 tools/trace_convert.py \
  --energy-csv captures/energy_capture_<timestamp>.csv \
  --out-csv data/replay_trace.csv
```

## Integration Direction
- Replace replay sample generation with live sensor ingestion.
- Keep the same decision model contract (`power_sample_t` + `UpdateAiModel`).
