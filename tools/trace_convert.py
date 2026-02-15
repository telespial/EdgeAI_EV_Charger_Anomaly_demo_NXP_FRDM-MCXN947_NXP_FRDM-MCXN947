#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path


def pick(row, names, default="0"):
    low = {k.lower(): v for k, v in row.items()}
    for n in names:
        if n.lower() in low:
            return low[n.lower()]
    return default


def clamp_u16(v):
    v = int(float(v))
    if v < 0:
        return 0
    if v > 65535:
        return 65535
    return v


def clamp_u8(v):
    v = int(float(v))
    if v < 0:
        return 0
    if v > 255:
        return 255
    return v


def main():
    p = argparse.ArgumentParser(description="Convert captured energy CSV to replay_trace.csv format.")
    p.add_argument("--energy-csv", required=True, help="Input energy CSV")
    p.add_argument("--out-csv", required=True, help="Output replay CSV")
    p.add_argument("--default-voltage-mv", type=int, default=12000)
    p.add_argument("--default-soc", type=int, default=80)
    p.add_argument("--default-temp-c", type=int, default=30)
    args = p.parse_args()

    src = Path(args.energy_csv)
    dst = Path(args.out_csv)
    dst.parent.mkdir(parents=True, exist_ok=True)

    out_rows = []
    with src.open("r", newline="") as f:
        r = csv.DictReader(f)
        for row in r:
            cur = pick(row, ["current_mA", "current_ma", "current", "i_ma"])
            pwr = pick(row, ["power_mW", "power_mw", "power", "p_mw"])
            vol = pick(row, ["voltage_mV", "voltage_mv", "voltage", "v_mv"], str(args.default_voltage_mv))

            out_rows.append(
                {
                    "current_mA": clamp_u16(cur),
                    "power_mW": clamp_u16(pwr),
                    "voltage_mV": clamp_u16(vol),
                    "soc_pct": clamp_u8(args.default_soc),
                    "temp_c": clamp_u8(args.default_temp_c),
                }
            )

    if not out_rows:
        raise SystemExit("No rows read from energy CSV")

    with dst.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["current_mA", "power_mW", "voltage_mV", "soc_pct", "temp_c"])
        w.writeheader()
        w.writerows(out_rows)

    print(f"Wrote {dst} ({len(out_rows)} rows)")


if __name__ == "__main__":
    main()
