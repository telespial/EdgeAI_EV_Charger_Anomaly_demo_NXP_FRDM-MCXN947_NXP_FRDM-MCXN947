#!/usr/bin/env bash
set -euo pipefail

# Capture real board power data with an external MCUX energy CLI while also
# recording UART markers from the running firmware.
#
# Required:
# - Set ENERGY_CAPTURE_CMD if mcux_energy_capture is not available.
# - Command may contain placeholders:
#     {DURATION} {OUTPUT}
#
# Example:
# ENERGY_CAPTURE_CMD="mcux_energy_capture --board FRDM-MCXN947 --duration {DURATION} --output {OUTPUT}" \
#   ./tools/capture_energy_trace.sh --duration 120

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/captures"
UART_DEV="/dev/ttyACM0"
DURATION=120
PREFIX="energy_capture"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --duration)
      DURATION="$2"; shift 2;;
    --uart)
      UART_DEV="$2"; shift 2;;
    --out-dir)
      OUT_DIR="$2"; shift 2;;
    --prefix)
      PREFIX="$2"; shift 2;;
    *)
      echo "Unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$OUT_DIR"

ENERGY_CSV="$OUT_DIR/${PREFIX}_${STAMP}.csv"
UART_LOG="$OUT_DIR/${PREFIX}_${STAMP}_uart.log"
SCENE_LOG="$OUT_DIR/${PREFIX}_${STAMP}_scenes.log"

ENERGY_CAPTURE_CMD="${ENERGY_CAPTURE_CMD:-mcux_energy_capture --duration {DURATION} --output {OUTPUT}}"

if [[ "$ENERGY_CAPTURE_CMD" == *"{DURATION}"* ]]; then
  ENERGY_CAPTURE_CMD="${ENERGY_CAPTURE_CMD//\{DURATION\}/$DURATION}"
fi
if [[ "$ENERGY_CAPTURE_CMD" == *"{OUTPUT}"* ]]; then
  ENERGY_CAPTURE_CMD="${ENERGY_CAPTURE_CMD//\{OUTPUT\}/$ENERGY_CSV}"
fi

if [[ "$ENERGY_CAPTURE_CMD" == mcux_energy_capture* ]] && ! command -v mcux_energy_capture >/dev/null 2>&1; then
  cat >&2 <<EOF
Energy CLI command is not available: mcux_energy_capture
Set ENERGY_CAPTURE_CMD to the installed MCUX energy tool command, e.g.:
  ENERGY_CAPTURE_CMD="your_energy_cli --duration {DURATION} --output {OUTPUT}" ./tools/capture_energy_trace.sh
EOF
  exit 2
fi

echo "[capture] duration: $DURATION s"
echo "[capture] uart: $UART_DEV"
echo "[capture] energy csv: $ENERGY_CSV"
echo "[capture] uart log: $UART_LOG"
echo "[capture] scene log: $SCENE_LOG"
echo "[capture] cmd: $ENERGY_CAPTURE_CMD"

timeout "$((DURATION + 10))" cat "$UART_DEV" >"$UART_LOG" &
UART_PID=$!

bash -lc "$ENERGY_CAPTURE_CMD"

wait "$UART_PID" || true

grep -E 'SCENE_START|SCENE_END' "$UART_LOG" >"$SCENE_LOG" || true

echo "[capture] done"
