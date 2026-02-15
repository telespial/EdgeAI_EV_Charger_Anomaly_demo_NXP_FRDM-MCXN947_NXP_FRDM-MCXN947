# Capture Mode (Real Energy Data)

Goal:
- collect real current/power traces from the dev board using MCUX energy measurement tooling,
- convert traces into replay data for standalone dashboard mode.

## Prerequisites
- FRDM-MCXN947 connected on debug USB.
- UART available (default `/dev/ttyACM0`).
- MCUX energy capture CLI command installed.
  - If command is not `mcux_energy_capture`, set `ENERGY_CAPTURE_CMD`.

## 1) Capture energy + UART markers
```bash
./tools/capture_energy_trace.sh --duration 120
```

If required, override command template:
```bash
ENERGY_CAPTURE_CMD="your_energy_cli --duration {DURATION} --output {OUTPUT}" \
./tools/capture_energy_trace.sh --duration 120
```

Outputs:
- `captures/energy_capture_<timestamp>.csv`
- `captures/energy_capture_<timestamp>_uart.log`
- `captures/energy_capture_<timestamp>_scenes.log`

## 2) Convert energy CSV to replay CSV
```bash
python3 tools/trace_convert.py \
  --energy-csv captures/energy_capture_<timestamp>.csv \
  --out-csv data/replay_trace.csv
```

## 3) Regenerate firmware trace header
```bash
./tools/trace_csv_to_header.py --in data/replay_trace.csv --out src/replay_trace_generated.h
```

## 4) Rebuild + flash
```bash
./tools/build_frdmmcxn947.sh debug
./tools/flash_frdmmcxn947.sh
```
