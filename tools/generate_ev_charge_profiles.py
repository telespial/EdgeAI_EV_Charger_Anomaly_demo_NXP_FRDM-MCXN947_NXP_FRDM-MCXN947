#!/usr/bin/env python3
import csv
import math
import random
from pathlib import Path


SAMPLE_HZ = 20
DURATION_MIN = 12
SIM_HOURS_PER_REAL_MIN = 1.0


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def target_current_a_from_soc(soc):
    if soc < 70.0:
        return 39.0 - (soc * 0.035)
    if soc < 90.0:
        return 36.5 - ((soc - 70.0) * 1.25)
    return 11.0 - ((soc - 90.0) * 0.78)


def in_window(t_s, start_s, end_s):
    return (t_s >= start_s) and (t_s < end_s)


def make_profile_rows(profile_name, wear_gain):
    n = DURATION_MIN * 60 * SAMPLE_HZ
    dt = 1.0 / SAMPLE_HZ
    rows = []
    rng = random.Random(9470 + wear_gain)
    wear = 0.0
    connector_temp = 26.0
    prev_current = 0.0

    for i in range(n):
        t_s = i * dt
        real_min = t_s / 60.0
        sim_h = real_min * SIM_HOURS_PER_REAL_MIN
        soc = clamp((sim_h / 12.0) * 100.0, 0.0, 100.0)

        startup = 1.0 - math.exp(-sim_h * 3.2)
        target_current = target_current_a_from_soc(soc) * startup
        current_noise = rng.gauss(0.0, 0.16 + (wear * 0.004))
        current_a = clamp(target_current + current_noise, 0.0, 40.0)

        line_noise_v = 1.5 * math.sin(2.0 * math.pi * 60.0 * t_s) + rng.gauss(0.0, 0.30)
        line_noise_v += 0.35 * math.sin(2.0 * math.pi * 180.0 * t_s)
        charger_voltage_v = 240.0 + line_noise_v + rng.gauss(0.0, 0.08)

        if profile_name in ("wear", "fault") and in_window(t_s, 390.0, 470.0):
            # Progressive connector wear period in mid/late session.
            wear += 0.015

        anomaly_label = "NONE"

        if profile_name == "fault":
            if in_window(t_s, 148.0, 170.0):
                # Short current spikes from intermittent contact.
                if (i % 19) == 0:
                    current_a = clamp(current_a + rng.uniform(2.5, 5.0), 0.0, 40.0)
                    anomaly_label = "CURRENT_SPIKE"
            if in_window(t_s, 292.0, 328.0):
                # Sustained voltage sag.
                charger_voltage_v -= rng.uniform(8.0, 14.0)
                anomaly_label = "VOLTAGE_SAG"
            if in_window(t_s, 430.0, 500.0):
                # Power instability phase near high-wear period.
                osc = math.sin(2.0 * math.pi * 1.8 * t_s)
                current_a = clamp(current_a + (1.8 * osc), 0.0, 40.0)
                charger_voltage_v += 2.8 * math.sin(2.0 * math.pi * 2.4 * t_s)
                anomaly_label = "POWER_UNSTABLE"

        pack_voltage_v = 298.0 + (soc * 1.05) + rng.gauss(0.0, 0.35)
        pf = 0.97 - 0.03 * (soc / 100.0)
        charge_power_kw = (charger_voltage_v * current_a * pf) / 1000.0

        if profile_name == "fault" and in_window(t_s, 430.0, 500.0):
            charge_power_kw *= (0.93 + 0.07 * math.sin(2.0 * math.pi * 2.1 * t_s))

        pack_temp_c = 24.5 + (current_a * 0.20) + (soc * 0.06) + rng.gauss(0.0, 0.20)

        wear += wear_gain * (current_a / 40.0) * dt * 0.025
        wear = clamp(wear, 0.0, 100.0)

        expected_conn = pack_temp_c + (current_a * 0.10)
        connector_temp += (expected_conn - connector_temp) * 0.09
        connector_temp += (wear * 0.010) + (abs(current_a - prev_current) * 0.06)
        connector_temp += rng.gauss(0.0, 0.08 + wear * 0.0015)

        if profile_name in ("wear", "fault"):
            connector_temp += wear * 0.004

        if profile_name == "fault" and in_window(t_s, 515.0, 700.0):
            # Late-cycle thermal drift from degraded connector contact.
            connector_temp += 0.03
            if anomaly_label == "NONE":
                anomaly_label = "CONNECTOR_DEGRADATION"

        connector_excess = connector_temp - expected_conn
        anomaly_score = clamp((connector_excess * 0.12) + (wear * 0.010) + (abs(current_a - prev_current) * 0.08), 0.0, 1.0)

        if profile_name == "normal":
            anomaly_score *= 0.4
        elif profile_name == "wear":
            anomaly_score *= 0.8

        if anomaly_score > 0.58 and anomaly_label == "NONE":
            anomaly_label = "WEAR_TREND_ALERT"

        if profile_name == "normal":
            anomaly_label = "NONE" if anomaly_score < 0.45 else "WEAR_TREND_ALERT"

        rows.append(
            {
                "timestamp_s": round(t_s, 3),
                "real_min": round(real_min, 4),
                "sim_hour": round(sim_h, 4),
                "session_state": "CC_CV_CHARGING" if soc < 99.5 else "TOP_OFF",
                "ac_voltage_v": round(charger_voltage_v, 3),
                "pack_voltage_v": round(pack_voltage_v, 3),
                "charge_current_a": round(current_a, 3),
                "charge_power_kw": round(charge_power_kw, 3),
                "soc_pct": round(soc, 3),
                "pack_temp_c": round(pack_temp_c, 3),
                "connector_temp_c": round(connector_temp, 3),
                "connector_wear_pct": round(wear, 3),
                "pilot_duty_pct": round(clamp((current_a / 40.0) * 100.0, 10.0, 100.0), 3),
                "line_freq_hz": round(60.0 + rng.gauss(0.0, 0.03), 4),
                "line_noise_v": round(line_noise_v, 3),
                "dI_dt_a_per_s": round((current_a - prev_current) * SAMPLE_HZ, 3),
                "anomaly_label": anomaly_label,
                "anomaly_score_0_1": round(anomaly_score, 4),
            }
        )
        prev_current = current_a
    return rows


def write_csv(path, rows):
    if not rows:
        return
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def write_firmware_replay(path, source_rows):
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(
            f, fieldnames=["current_mA", "power_mW", "voltage_mV", "soc_pct", "temp_c"]
        )
        writer.writeheader()
        for row in source_rows:
            current_mA = int(clamp(round(row["charge_current_a"] * 1000.0), 0, 65535))
            power_w = int(clamp(round(row["charge_power_kw"] * 1000.0), 0, 65535))
            voltage_cV = int(clamp(round(row["ac_voltage_v"] * 100.0), 0, 65535))
            soc_pct = int(clamp(round(row["soc_pct"]), 0, 100))
            temp_c = int(clamp(round(row["connector_temp_c"]), 0, 120))
            writer.writerow(
                {
                    "current_mA": current_mA,
                    "power_mW": power_w,
                    "voltage_mV": voltage_cV,
                    "soc_pct": soc_pct,
                    "temp_c": temp_c,
                }
            )


def main():
    root = Path(__file__).resolve().parents[1]
    data_dir = root / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    normal_rows = make_profile_rows("normal", wear_gain=0.4)
    wear_rows = make_profile_rows("wear", wear_gain=1.2)
    fault_rows = make_profile_rows("fault", wear_gain=2.0)

    write_csv(data_dir / "ev_charge_12min_normal_20hz.csv", normal_rows)
    write_csv(data_dir / "ev_charge_12min_wear_20hz.csv", wear_rows)
    write_csv(data_dir / "ev_charge_12min_fault_20hz.csv", fault_rows)

    # Use the fault profile for firmware replay so AI demo events are always visible.
    write_firmware_replay(data_dir / "replay_trace.csv", fault_rows)
    print("Generated:")
    print(" - data/ev_charge_12min_normal_20hz.csv")
    print(" - data/ev_charge_12min_wear_20hz.csv")
    print(" - data/ev_charge_12min_fault_20hz.csv")
    print(" - data/replay_trace.csv (from fault profile, firmware-ready)")


if __name__ == "__main__":
    main()
