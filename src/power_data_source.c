#include "power_data_source.h"

#define POWER_SAMPLE_PERIOD_MS 50u
#define POWER_SIM_TIME_SCALE 60u
#define POWER_TEMP_HISTORY_LEN 20u
#define REPLAY_CYCLE_HOURS 12u
#define REPLAY_CYCLE_SECONDS (REPLAY_CYCLE_HOURS * 3600u)
#define REPLAY_CYCLE_TICKS ((REPLAY_CYCLE_SECONDS * 1000u) / POWER_SAMPLE_PERIOD_MS)

typedef struct
{
    power_data_source_mode_t mode;
    uint32_t replay_index;
    power_sample_t current;
    power_sample_t live_override;
    uint32_t elapsed_ms_real;
    uint32_t elapsed_ms_sim;

    int32_t ema_voltage_cV;
    int32_t ema_current_mA;
    int32_t ema_power_W;
    int32_t ema_power_resid_W;
    int32_t ema_temp_c10;
    int32_t ambient_c10;
    int32_t drift_c10_ema;
    int32_t pack_temp_c10;
    int32_t connector_temp_c10;
    int32_t wear_q10;
    uint16_t wear_stress_accum;

    int16_t temp_hist_c10[POWER_TEMP_HISTORY_LEN];
    uint8_t temp_hist_idx;
    bool temp_hist_ready;

    uint16_t status_hold_ticks;
    uint16_t fault_latch_ticks;
    bool replay_hour_lock;
    uint32_t replay_hour_start_tick;
    uint32_t replay_hour_end_tick;
    power_replay_profile_t replay_profile;
    bool ai_assist_enabled;
    uint16_t ai_prevented_events;
    bool ai_prevent_latch;
} power_data_state_t;

static power_data_state_t gPowerData;

#define HOUR_TICKS ((3600u * 1000u) / POWER_SAMPLE_PERIOD_MS)

static uint32_t ReplayIndexToSimSeconds(uint32_t replay_index)
{
    return (replay_index * POWER_SAMPLE_PERIOD_MS * POWER_SIM_TIME_SCALE) / 1000u;
}

static uint32_t ReplayIndexToReplaySeconds(uint32_t replay_index)
{
    return (replay_index * POWER_SAMPLE_PERIOD_MS) / 1000u;
}

static int32_t AbsI32(int32_t v)
{
    return (v < 0) ? -v : v;
}

static uint8_t ClampU8(int32_t v)
{
    if (v < 0)
    {
        return 0u;
    }
    if (v > 255)
    {
        return 255u;
    }
    return (uint8_t)v;
}

static uint16_t ClampU16(int32_t v)
{
    if (v < 0)
    {
        return 0u;
    }
    if (v > 65535)
    {
        return 65535u;
    }
    return (uint16_t)v;
}

static int32_t ClampI32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

static uint32_t LerpU32(uint32_t a, uint32_t b, uint32_t x, uint32_t x0, uint32_t x1)
{
    if (x <= x0)
    {
        return a;
    }
    if (x >= x1)
    {
        return b;
    }
    return a + (uint32_t)(((uint64_t)(b - a) * (uint64_t)(x - x0)) / (uint64_t)(x1 - x0));
}

typedef struct
{
    uint8_t start_soc_pct;
    uint8_t end_soc_pct;
    uint8_t taper_soc_pct;
    uint32_t battery_wh;
    uint8_t efficiency_pct;
    uint16_t ambient_c;
    uint16_t target_current_mA;
    uint16_t taper_end_current_mA;
    uint16_t no_load_voltage_mV;
    uint16_t load_voltage_mV;
    uint8_t temp_plateau_c;
    uint16_t ripple_voltage_mV_step;
    uint16_t ripple_current_mA_step;
    uint16_t incident_sag_mV;
    uint16_t incident_spike_mA;
    uint8_t incident_temp_c;
} replay_profile_cfg_t;

static replay_profile_cfg_t ReplayProfileCfg(power_replay_profile_t profile)
{
    replay_profile_cfg_t cfg;

    if (profile == POWER_REPLAY_PROFILE_OUTLET)
    {
        cfg.start_soc_pct = 20u;
        cfg.end_soc_pct = 90u;
        cfg.taper_soc_pct = 83u;
        cfg.battery_wh = 78000u;
        cfg.efficiency_pct = 90u;
        cfg.ambient_c = 22u;
        cfg.target_current_mA = 32000u;
        cfg.taper_end_current_mA = 3500u;
        cfg.no_load_voltage_mV = 24300u;
        cfg.load_voltage_mV = 23500u;
        cfg.temp_plateau_c = 60u;
        cfg.ripple_voltage_mV_step = 4u;
        cfg.ripple_current_mA_step = 42u;
        cfg.incident_sag_mV = 520u;
        cfg.incident_spike_mA = 3800u;
        cfg.incident_temp_c = 8u;
    }
    else
    {
        cfg.start_soc_pct = 20u;
        cfg.end_soc_pct = 90u;
        cfg.taper_soc_pct = 85u;
        cfg.battery_wh = 78000u;
        cfg.efficiency_pct = 91u;
        cfg.ambient_c = 22u;
        cfg.target_current_mA = 48000u;
        cfg.taper_end_current_mA = 7000u;
        cfg.no_load_voltage_mV = 24200u;
        cfg.load_voltage_mV = 23800u;
        cfg.temp_plateau_c = 51u;
        cfg.ripple_voltage_mV_step = 2u;
        cfg.ripple_current_mA_step = 22u;
        cfg.incident_sag_mV = 320u;
        cfg.incident_spike_mA = 2400u;
        cfg.incident_temp_c = 5u;
    }

    return cfg;
}

static power_sample_t SampleFromReplay(uint32_t index, power_replay_profile_t profile)
{
    power_sample_t out;
    replay_profile_cfg_t cfg = ReplayProfileCfg(profile);
    uint32_t t = index % REPLAY_CYCLE_TICKS;
    int32_t current_mA;
    int32_t voltage_mV;
    uint32_t soc_pct;
    int32_t temp_c;
    uint32_t ripple;
    uint32_t watts;
    uint32_t handshake_ticks = (120u * 1000u) / POWER_SAMPLE_PERIOD_MS;
    uint32_t test_ticks = (600u * 1000u) / POWER_SAMPLE_PERIOD_MS;
    uint32_t full_ramp_ticks = (300u * 1000u) / POWER_SAMPLE_PERIOD_MS;
    uint32_t startup_ticks = handshake_ticks + test_ticks + full_ramp_ticks;
    uint32_t test_1_end = handshake_ticks + (test_ticks / 5u);
    uint32_t test_2_end = handshake_ticks + ((test_ticks * 2u) / 5u);
    uint32_t test_3_end = handshake_ticks + ((test_ticks * 3u) / 5u);
    uint32_t test_4_end = handshake_ticks + ((test_ticks * 4u) / 5u);
    uint32_t test_5_end = handshake_ticks + test_ticks;
    uint32_t ramp_end = startup_ticks;
    uint32_t test_i1_mA = (cfg.target_current_mA * 18u) / 100u;
    uint32_t test_i2_mA = (cfg.target_current_mA * 32u) / 100u;
    uint32_t test_i3_mA = (cfg.target_current_mA * 48u) / 100u;
    uint32_t test_i4_mA = (cfg.target_current_mA * 38u) / 100u;
    uint32_t test_i5_mA = (cfg.target_current_mA * 58u) / 100u;
    uint32_t bulk_ticks;
    uint32_t taper_ticks;
    uint32_t bulk_end_ticks;
    uint32_t taper_end_ticks;
    uint32_t done_start_ticks;
    uint32_t bulk_power_w;
    uint32_t taper_avg_power_w;
    uint64_t energy_total_wh;
    uint64_t energy_bulk_wh;
    uint64_t energy_taper_wh;
    uint64_t bulk_s;
    uint64_t taper_s;
    uint32_t incident1_start;
    uint32_t incident1_end;
    uint32_t incident2_start;
    uint32_t incident2_end;
    uint32_t incident3_start;
    uint32_t incident3_end;
    uint32_t cool_target_c = cfg.ambient_c + 8u;

    bulk_power_w = ((uint32_t)cfg.load_voltage_mV * (uint32_t)cfg.target_current_mA) / 100000u;
    if (bulk_power_w < 1000u)
    {
        bulk_power_w = 1000u;
    }
    taper_avg_power_w = (bulk_power_w * 45u) / 100u;
    if (taper_avg_power_w < 800u)
    {
        taper_avg_power_w = 800u;
    }

    energy_total_wh = ((uint64_t)cfg.battery_wh * (uint64_t)(cfg.end_soc_pct - cfg.start_soc_pct)) / 100u;
    energy_bulk_wh = ((uint64_t)cfg.battery_wh * (uint64_t)(cfg.taper_soc_pct - cfg.start_soc_pct)) / 100u;
    if (energy_bulk_wh > energy_total_wh)
    {
        energy_bulk_wh = energy_total_wh;
    }
    energy_taper_wh = energy_total_wh - energy_bulk_wh;

    bulk_s = (energy_bulk_wh * 3600ull * 100ull) / ((uint64_t)bulk_power_w * (uint64_t)cfg.efficiency_pct);
    taper_s = (energy_taper_wh * 3600ull * 100ull) / ((uint64_t)taper_avg_power_w * (uint64_t)cfg.efficiency_pct);
    if (bulk_s < 3600ull)
    {
        bulk_s = 3600ull;
    }
    if (taper_s < 1200ull)
    {
        taper_s = 1200ull;
    }

    bulk_ticks = (uint32_t)((bulk_s * 1000ull) / POWER_SAMPLE_PERIOD_MS);
    taper_ticks = (uint32_t)((taper_s * 1000ull) / POWER_SAMPLE_PERIOD_MS);
    bulk_end_ticks = startup_ticks + bulk_ticks;
    taper_end_ticks = bulk_end_ticks + taper_ticks;
    done_start_ticks = taper_end_ticks;

    if (done_start_ticks > (REPLAY_CYCLE_TICKS - 1u))
    {
        uint32_t active_ticks = done_start_ticks - startup_ticks;
        uint32_t room_ticks = (REPLAY_CYCLE_TICKS - 1u) - startup_ticks;
        if ((active_ticks > 0u) && (room_ticks > 0u))
        {
            bulk_ticks = (uint32_t)(((uint64_t)bulk_ticks * (uint64_t)room_ticks) / (uint64_t)active_ticks);
            taper_ticks = room_ticks - bulk_ticks;
            if (taper_ticks < (900u * 1000u / POWER_SAMPLE_PERIOD_MS))
            {
                taper_ticks = (900u * 1000u / POWER_SAMPLE_PERIOD_MS);
                if (bulk_ticks > taper_ticks)
                {
                    bulk_ticks = room_ticks - taper_ticks;
                }
            }
            bulk_end_ticks = startup_ticks + bulk_ticks;
            taper_end_ticks = bulk_end_ticks + taper_ticks;
            done_start_ticks = taper_end_ticks;
        }
    }

    if (t < handshake_ticks)
    {
        /* Session start: handshake/self-check at ambient and 0 A. */
        current_mA = 0u;
        voltage_mV = cfg.no_load_voltage_mV;
        soc_pct = cfg.start_soc_pct;
        temp_c = cfg.ambient_c;
    }
    else if (t < test_1_end)
    {
        /* Controlled low-current validation. */
        current_mA = LerpU32(0u, test_i1_mA, t, handshake_ticks, test_1_end);
        voltage_mV = LerpU32(cfg.no_load_voltage_mV, cfg.no_load_voltage_mV - 140u, t, handshake_ticks, test_1_end);
        soc_pct = cfg.start_soc_pct;
        temp_c = cfg.ambient_c;
    }
    else if (t < test_2_end)
    {
        current_mA = LerpU32(test_i1_mA, test_i2_mA, t, test_1_end, test_2_end);
        voltage_mV = LerpU32(cfg.no_load_voltage_mV - 140u, cfg.no_load_voltage_mV - 220u, t, test_1_end, test_2_end);
        soc_pct = cfg.start_soc_pct;
        temp_c = LerpU32(cfg.ambient_c, cfg.ambient_c + 1u, t, test_1_end, test_2_end);
    }
    else if (t < test_3_end)
    {
        current_mA = LerpU32(test_i2_mA, test_i3_mA, t, test_2_end, test_3_end);
        voltage_mV = LerpU32(cfg.no_load_voltage_mV - 220u, cfg.no_load_voltage_mV - 320u, t, test_2_end, test_3_end);
        soc_pct = cfg.start_soc_pct;
        temp_c = LerpU32(cfg.ambient_c + 1u, cfg.ambient_c + 2u, t, test_2_end, test_3_end);
    }
    else if (t < test_4_end)
    {
        current_mA = LerpU32(test_i3_mA, test_i4_mA, t, test_3_end, test_4_end);
        voltage_mV = LerpU32(cfg.no_load_voltage_mV - 320u, cfg.no_load_voltage_mV - 270u, t, test_3_end, test_4_end);
        soc_pct = cfg.start_soc_pct;
        temp_c = LerpU32(cfg.ambient_c + 2u, cfg.ambient_c + 3u, t, test_3_end, test_4_end);
    }
    else if (t < test_5_end)
    {
        current_mA = LerpU32(test_i4_mA, test_i5_mA, t, test_4_end, test_5_end);
        voltage_mV = LerpU32(cfg.no_load_voltage_mV - 270u, cfg.no_load_voltage_mV - 360u, t, test_4_end, test_5_end);
        soc_pct = cfg.start_soc_pct;
        temp_c = LerpU32(cfg.ambient_c + 3u, cfg.ambient_c + 4u, t, test_4_end, test_5_end);
    }
    else if (t < ramp_end)
    {
        /* Final smooth ramp to full charging current. */
        current_mA = LerpU32(test_i5_mA, cfg.target_current_mA, t, test_5_end, ramp_end);
        voltage_mV = LerpU32(cfg.no_load_voltage_mV - 360u, cfg.load_voltage_mV, t, test_5_end, ramp_end);
        soc_pct = LerpU32(cfg.start_soc_pct, cfg.start_soc_pct + 2u, t, test_5_end, ramp_end);
        temp_c = LerpU32(cfg.ambient_c + 4u, cfg.ambient_c + 6u, t, test_5_end, ramp_end);
    }
    else if (t < bulk_end_ticks)
    {
        current_mA = cfg.target_current_mA;
        voltage_mV = cfg.load_voltage_mV;
        soc_pct = LerpU32(cfg.start_soc_pct + 2u, cfg.taper_soc_pct, t, startup_ticks, bulk_end_ticks);
        temp_c = LerpU32(cfg.ambient_c + 4u, cfg.temp_plateau_c, t, startup_ticks, bulk_end_ticks);
    }
    else if (t < taper_end_ticks)
    {
        current_mA = LerpU32(cfg.target_current_mA, cfg.taper_end_current_mA, t, bulk_end_ticks, taper_end_ticks);
        voltage_mV = LerpU32(cfg.load_voltage_mV, cfg.no_load_voltage_mV + 100u, t, bulk_end_ticks, taper_end_ticks);
        soc_pct = LerpU32(cfg.taper_soc_pct, cfg.end_soc_pct, t, bulk_end_ticks, taper_end_ticks);
        temp_c = LerpU32(cfg.temp_plateau_c, cfg.ambient_c + 14u, t, bulk_end_ticks, taper_end_ticks);
    }
    else
    {
        current_mA = 0u;
        voltage_mV = LerpU32(cfg.no_load_voltage_mV + 100u, cfg.no_load_voltage_mV + 200u, t, done_start_ticks, REPLAY_CYCLE_TICKS - 1u);
        soc_pct = cfg.end_soc_pct;
        temp_c = LerpU32(cfg.ambient_c + 14u, cool_target_c, t, done_start_ticks, REPLAY_CYCLE_TICKS - 1u);
    }

    incident1_start = startup_ticks + (bulk_ticks * 55u) / 100u;
    incident1_end = incident1_start + ((8u * 60u * 1000u) / POWER_SAMPLE_PERIOD_MS);
    incident2_start = startup_ticks + (bulk_ticks * 63u) / 100u;
    incident2_end = incident2_start + ((5u * 60u * 1000u) / POWER_SAMPLE_PERIOD_MS);
    incident3_start = startup_ticks + (bulk_ticks * 68u) / 100u;
    incident3_end = incident3_start + ((10u * 60u * 1000u) / POWER_SAMPLE_PERIOD_MS);

    if ((t >= incident1_start) && (t <= incident1_end))
    {
        voltage_mV -= cfg.incident_sag_mV;
    }
    if ((t >= incident2_start) && (t <= incident2_end))
    {
        current_mA += cfg.incident_spike_mA;
    }
    if ((t >= incident3_start) && (t <= incident3_end))
    {
        temp_c += cfg.incident_temp_c;
    }

    if ((t >= handshake_ticks) && (t < test_5_end))
    {
        uint32_t test_pos = t - handshake_ticks;
        uint32_t pulse_window = (20u * 1000u) / POWER_SAMPLE_PERIOD_MS;
        uint32_t pulse_period = (60u * 1000u) / POWER_SAMPLE_PERIOD_MS;
        if ((pulse_period > 0u) && ((test_pos % pulse_period) < pulse_window))
        {
            current_mA += (int32_t)((cfg.target_current_mA * 16u) / 100u);
            voltage_mV -= (int32_t)((cfg.incident_sag_mV / 2u) + 60u);
            temp_c += 1;
        }
    }

    /* Deterministic high-rate ripple to mimic realistic 20 Hz telemetry noise. */
    ripple = (uint32_t)((index * 1103515245u + 12345u) >> 27);
    if (t < startup_ticks)
    {
        voltage_mV += (int32_t)((ripple & 0x3u) - 1);
        current_mA += (int32_t)(((ripple >> 3) & 0x3u) - 1) * 2;
    }
    else
    {
        voltage_mV += (int32_t)((ripple & 0x7u) - 3) * (int32_t)cfg.ripple_voltage_mV_step;
        current_mA += (int32_t)(((ripple >> 3) & 0xFu) - 7) * (int32_t)cfg.ripple_current_mA_step;
        temp_c += (int32_t)(((ripple >> 2) & 0x3u) - 1);
    }

    current_mA = ClampI32(current_mA, 0, 62000);
    voltage_mV = ClampI32(voltage_mV, 22400, 24600);
    temp_c = ClampI32(temp_c, 20, 99);

    watts = (uint32_t)(((uint64_t)(uint32_t)voltage_mV * (uint64_t)(uint32_t)current_mA) / 100000u);
    watts = (uint32_t)(((uint64_t)watts * (97u + (ripple & 0x3u))) / 100u);

    out.current_mA = (uint16_t)current_mA;
    out.power_mW = (uint16_t)ClampI32((int32_t)watts, 0, 14000);
    out.voltage_mV = (uint16_t)voltage_mV;
    out.soc_pct = (uint8_t)ClampI32((int32_t)soc_pct, 0, 100);
    out.temp_c = (uint8_t)temp_c;
    out.anomaly_score_pct = 0u;
    out.connector_wear_pct = 0u;
    out.ai_status = AI_STATUS_NORMAL;
    out.ai_fault_flags = 0u;
    out.thermal_risk_s = 0u;
    out.degradation_drift_pct = 0u;
    out.ai_decision = AI_DECISION_NONE;
    out.ai_confidence_pct = 0u;
    out.ai_prevented_events = 0u;
    out.predicted_overtemp_s = 0u;
    out.connector_risk_pct = 0u;
    out.wire_risk_pct = 0u;
    out.thermal_damage_risk_pct = 0u;
    out.elapsed_charge_s = 0u;
    out.elapsed_charge_sim_s = 0u;
    return out;
}

static void UpdateAiModel(const power_sample_t *prev)
{
    int32_t v_cV = (int32_t)gPowerData.current.voltage_mV;
    int32_t i_mA = (int32_t)gPowerData.current.current_mA;
    int32_t p_W = (int32_t)gPowerData.current.power_mW;
    int32_t temp_c10 = (int32_t)gPowerData.current.temp_c * 10;
    int32_t dv;
    int32_t di;
    int32_t dp;
    int32_t expected_p_W;
    int32_t p_resid_W;
    int32_t expected_temp_c10;
    int32_t drift_c10;
    int32_t temp_slope_c10_per_s = 0;
    int32_t thermal_risk_s = 0;
    int32_t anomaly = 0;
    int32_t drift_pct = 0;
    uint8_t fault_flags = 0u;
    ai_status_t ai_status = AI_STATUS_NORMAL;
    bool voltage_sag;
    bool current_spike;
    bool power_unstable;
    int32_t thermal_target_c10;
    int32_t connector_target_c10;
    int32_t stress = 0;
    int32_t thermal_wear_pct;
    ai_decision_t decision = AI_DECISION_NONE;
    int32_t confidence = 0;
    bool pre_mitigation_fault = false;
    int32_t derate_pct = 100;
    int32_t connector_risk;
    int32_t wire_risk;
    int32_t thermal_damage_risk;
    int32_t predicted_overtemp_s = 0;

    gPowerData.elapsed_ms_real += POWER_SAMPLE_PERIOD_MS;
    gPowerData.elapsed_ms_sim += POWER_SAMPLE_PERIOD_MS * POWER_SIM_TIME_SCALE;
    gPowerData.current.elapsed_charge_s = gPowerData.elapsed_ms_real / 1000u;
    gPowerData.current.elapsed_charge_sim_s = gPowerData.elapsed_ms_sim / 1000u;

    di = (prev == 0) ? 0 : AbsI32(i_mA - (int32_t)prev->current_mA);
    dp = (prev == 0) ? 0 : AbsI32(p_W - (int32_t)prev->power_mW);

    if (prev == 0)
    {
        gPowerData.ema_voltage_cV = v_cV;
        gPowerData.ema_current_mA = i_mA;
        gPowerData.ema_power_W = p_W;
        gPowerData.ema_power_resid_W = 0;
        gPowerData.ema_temp_c10 = temp_c10;
        gPowerData.ambient_c10 = temp_c10;
        gPowerData.drift_c10_ema = 0;
        gPowerData.pack_temp_c10 = temp_c10;
        gPowerData.connector_temp_c10 = temp_c10;
        gPowerData.wear_q10 = 0;
        gPowerData.wear_stress_accum = 0u;
    }
    else
    {
        gPowerData.ema_voltage_cV += (v_cV - gPowerData.ema_voltage_cV) / 16;
        gPowerData.ema_current_mA += (i_mA - gPowerData.ema_current_mA) / 16;
        gPowerData.ema_power_W += (p_W - gPowerData.ema_power_W) / 16;
        gPowerData.ema_temp_c10 += (temp_c10 - gPowerData.ema_temp_c10) / 16;
    }

    if (temp_c10 < gPowerData.ambient_c10)
    {
        gPowerData.ambient_c10 += (temp_c10 - gPowerData.ambient_c10) / 8;
    }
    else
    {
        gPowerData.ambient_c10 += (temp_c10 - gPowerData.ambient_c10) / 96;
    }

    thermal_target_c10 = gPowerData.ambient_c10 + (i_mA / 520) + (p_W / 140);
    gPowerData.pack_temp_c10 += (thermal_target_c10 - gPowerData.pack_temp_c10) / 20;
    connector_target_c10 = gPowerData.pack_temp_c10 + (i_mA / 760) + (gPowerData.wear_q10 / 6);
    gPowerData.connector_temp_c10 += (connector_target_c10 - gPowerData.connector_temp_c10) / 12;
    gPowerData.connector_temp_c10 += di / 320;
    if ((gPowerData.wear_q10 > 450) && (i_mA > 24000))
    {
        gPowerData.connector_temp_c10 += (gPowerData.wear_q10 - 450) / 60;
    }
    if (i_mA < 6000)
    {
        gPowerData.connector_temp_c10 -= 1;
    }
    gPowerData.connector_temp_c10 = ClampI32(gPowerData.connector_temp_c10, 180, 990);
    gPowerData.current.temp_c = (uint8_t)ClampI32(gPowerData.connector_temp_c10 / 10, 18, 99);
    temp_c10 = (int32_t)gPowerData.current.temp_c * 10;

    if (i_mA > 18000)
    {
        stress += 1;
    }
    if (i_mA > 24000)
    {
        stress += 1;
    }
    if (i_mA > 30000)
    {
        stress += 1;
    }
    if (di > 1800)
    {
        stress += 1;
    }
    if (temp_c10 > 700)
    {
        stress += 2;
    }
    gPowerData.wear_stress_accum = (uint16_t)(gPowerData.wear_stress_accum + (uint16_t)stress);
    if (gPowerData.wear_stress_accum >= 180u)
    {
        gPowerData.wear_q10 += (int32_t)(gPowerData.wear_stress_accum / 180u);
        gPowerData.wear_stress_accum = (uint16_t)(gPowerData.wear_stress_accum % 180u);
    }
    gPowerData.wear_q10 = ClampI32(gPowerData.wear_q10, 0, 1000);
    thermal_wear_pct = gPowerData.wear_q10 / 10;

    expected_p_W = (v_cV * i_mA) / 100000;
    p_resid_W = AbsI32(p_W - expected_p_W);
    gPowerData.ema_power_resid_W += (p_resid_W - gPowerData.ema_power_resid_W) / 20;

    expected_temp_c10 = gPowerData.ambient_c10 + (i_mA / 320);
    drift_c10 = temp_c10 - expected_temp_c10;
    gPowerData.drift_c10_ema += (drift_c10 - gPowerData.drift_c10_ema) / 32;

    dv = AbsI32(v_cV - gPowerData.ema_voltage_cV);

    {
        int16_t prev_temp = gPowerData.temp_hist_c10[gPowerData.temp_hist_idx];
        gPowerData.temp_hist_c10[gPowerData.temp_hist_idx] = (int16_t)temp_c10;
        gPowerData.temp_hist_idx = (uint8_t)((gPowerData.temp_hist_idx + 1u) % POWER_TEMP_HISTORY_LEN);
        if (gPowerData.temp_hist_ready)
        {
            temp_slope_c10_per_s = temp_c10 - (int32_t)prev_temp;
        }
        else if (gPowerData.temp_hist_idx == 0u)
        {
            gPowerData.temp_hist_ready = true;
        }
    }

    voltage_sag = ((v_cV < 22800) || ((gPowerData.ema_voltage_cV - v_cV) > 130)) && (i_mA > 9000);
    current_spike = (i_mA > 3000) && (di > 2500);
    power_unstable = (i_mA > 3000) && ((dp > 700) || (p_resid_W > 900) || (gPowerData.ema_power_resid_W > 650));

    if (voltage_sag)
    {
        fault_flags |= AI_FAULT_VOLTAGE_SAG;
        anomaly += 28;
    }
    if (current_spike)
    {
        fault_flags |= AI_FAULT_CURRENT_SPIKE;
        anomaly += 26;
    }
    if (power_unstable)
    {
        fault_flags |= AI_FAULT_POWER_UNSTABLE;
        anomaly += 24;
    }

    if (fault_flags != 0u)
    {
        /* Keep warning/fault state sticky so transient spikes do not bounce to green. */
        gPowerData.fault_latch_ticks = 600u;
    }
    else if (gPowerData.fault_latch_ticks > 0u)
    {
        gPowerData.fault_latch_ticks--;
    }

    if ((temp_slope_c10_per_s > 1) && (temp_c10 < 780))
    {
        thermal_risk_s = (700 - temp_c10) / temp_slope_c10_per_s;
        if (thermal_risk_s < 0)
        {
            thermal_risk_s = 0;
        }
    }
    if ((gPowerData.drift_c10_ema > 45) && (i_mA > 24000) && ((thermal_risk_s == 0) || (thermal_risk_s > 90)))
    {
        thermal_risk_s = 90;
    }

    if ((thermal_risk_s > 0) && (thermal_risk_s <= 180))
    {
        anomaly += 22;
        if (thermal_risk_s <= 45)
        {
            anomaly += 20;
        }
    }

    if (gPowerData.drift_c10_ema > 0)
    {
        anomaly += gPowerData.drift_c10_ema / 4;
    }

    if ((temp_slope_c10_per_s > 0) && (temp_c10 < 850))
    {
        predicted_overtemp_s = (850 - temp_c10) / temp_slope_c10_per_s;
        if (predicted_overtemp_s < 0)
        {
            predicted_overtemp_s = 0;
        }
    }
    else
    {
        predicted_overtemp_s = 0;
    }

    drift_pct = (gPowerData.drift_c10_ema * 100) / 180;
    if (drift_pct < 0)
    {
        drift_pct = 0;
    }
    if (drift_pct > 100)
    {
        drift_pct = 100;
    }

    connector_risk = (thermal_wear_pct * 55 + drift_pct * 45 + anomaly * 25) / 100;
    connector_risk = ClampI32(connector_risk, 0, 99);
    wire_risk = ((temp_c10 > 600 ? (temp_c10 - 600) : 0) / 3) + (di / 120) + (thermal_wear_pct / 3);
    wire_risk = ClampI32(wire_risk, 0, 99);
    thermal_damage_risk = ((temp_c10 > 650 ? (temp_c10 - 650) : 0) / 2) + (temp_slope_c10_per_s * 18) + (anomaly / 4);
    thermal_damage_risk = ClampI32(thermal_damage_risk, 0, 99);

    if ((v_cV < 22300) || (temp_c10 >= 720) || ((fault_flags & (AI_FAULT_VOLTAGE_SAG | AI_FAULT_POWER_UNSTABLE)) == (AI_FAULT_VOLTAGE_SAG | AI_FAULT_POWER_UNSTABLE)) ||
        ((fault_flags & (AI_FAULT_CURRENT_SPIKE | AI_FAULT_POWER_UNSTABLE)) == (AI_FAULT_CURRENT_SPIKE | AI_FAULT_POWER_UNSTABLE)) ||
        ((thermal_risk_s > 0) && (thermal_risk_s <= 45)) || (anomaly >= 80))
    {
        ai_status = AI_STATUS_FAULT;
        gPowerData.status_hold_ticks = 60u;
    }
    else if ((fault_flags != 0u) || (gPowerData.fault_latch_ticks > 0u) || (anomaly >= 45) || (drift_pct >= 35) || ((thermal_risk_s > 0) && (thermal_risk_s <= 180)) || (dv > 90))
    {
        ai_status = AI_STATUS_WARNING;
        if (fault_flags != 0u)
        {
            if (gPowerData.status_hold_ticks < 160u)
            {
                gPowerData.status_hold_ticks = 160u;
            }
        }
        else if (gPowerData.fault_latch_ticks > 0u)
        {
            if (gPowerData.status_hold_ticks < 120u)
            {
                gPowerData.status_hold_ticks = 120u;
            }
        }
        else if (gPowerData.status_hold_ticks < 40u)
        {
            gPowerData.status_hold_ticks = 40u;
        }
    }
    else if (gPowerData.status_hold_ticks > 0u)
    {
        ai_status = (gPowerData.current.ai_status == AI_STATUS_FAULT) ? AI_STATUS_FAULT : AI_STATUS_WARNING;
        gPowerData.status_hold_ticks--;
    }
    pre_mitigation_fault = (ai_status == AI_STATUS_FAULT);

    if (gPowerData.ai_assist_enabled)
    {
        if (pre_mitigation_fault || (temp_c10 >= 850) || ((thermal_risk_s > 0) && (thermal_risk_s <= 30)))
        {
            decision = AI_DECISION_SHED_LOAD;
            derate_pct = 45;
            confidence = 95;
        }
        else if ((ai_status == AI_STATUS_WARNING) || (anomaly >= 60) || ((thermal_risk_s > 0) && (thermal_risk_s <= 120)))
        {
            decision = AI_DECISION_DERATE_30;
            derate_pct = 70;
            confidence = 84;
        }
        else if ((anomaly >= 35) || (drift_pct >= 30) || ((thermal_risk_s > 0) && (thermal_risk_s <= 240)))
        {
            decision = AI_DECISION_DERATE_15;
            derate_pct = 85;
            confidence = 72;
        }
        else if ((anomaly >= 20) || (drift_pct >= 18))
        {
            decision = AI_DECISION_WATCH;
            derate_pct = 100;
            confidence = 60;
        }

        if (decision >= AI_DECISION_DERATE_15)
        {
            gPowerData.current.current_mA = (uint16_t)(((uint32_t)gPowerData.current.current_mA * (uint32_t)derate_pct) / 100u);
            gPowerData.current.power_mW = (uint16_t)(((uint32_t)gPowerData.current.power_mW * (uint32_t)derate_pct) / 100u);
            if (decision == AI_DECISION_SHED_LOAD)
            {
                gPowerData.current.temp_c = (uint8_t)ClampI32((int32_t)gPowerData.current.temp_c - 6, 18, 99);
            }
            else if (decision == AI_DECISION_DERATE_30)
            {
                gPowerData.current.temp_c = (uint8_t)ClampI32((int32_t)gPowerData.current.temp_c - 4, 18, 99);
            }
            else
            {
                gPowerData.current.temp_c = (uint8_t)ClampI32((int32_t)gPowerData.current.temp_c - 2, 18, 99);
            }
        }

        if (pre_mitigation_fault && (decision >= AI_DECISION_DERATE_30))
        {
            if (!gPowerData.ai_prevent_latch)
            {
                gPowerData.ai_prevent_latch = true;
                if (gPowerData.ai_prevented_events < 999u)
                {
                    gPowerData.ai_prevented_events++;
                }
            }
            ai_status = AI_STATUS_WARNING;
            anomaly = ClampI32(anomaly - 18, 0, 65535);
            if ((thermal_risk_s > 0) && (thermal_risk_s < 180))
            {
                thermal_risk_s = 180;
            }
        }
        else if (!pre_mitigation_fault)
        {
            gPowerData.ai_prevent_latch = false;
        }

        if (decision == AI_DECISION_SHED_LOAD)
        {
            connector_risk = (connector_risk * 70) / 100;
            wire_risk = (wire_risk * 60) / 100;
            thermal_damage_risk = (thermal_damage_risk * 55) / 100;
            if ((predicted_overtemp_s > 0) && (predicted_overtemp_s < 240))
            {
                predicted_overtemp_s = 240;
            }
        }
        else if (decision == AI_DECISION_DERATE_30)
        {
            connector_risk = (connector_risk * 82) / 100;
            wire_risk = (wire_risk * 78) / 100;
            thermal_damage_risk = (thermal_damage_risk * 72) / 100;
            if ((predicted_overtemp_s > 0) && (predicted_overtemp_s < 180))
            {
                predicted_overtemp_s = 180;
            }
        }
        else if (decision == AI_DECISION_DERATE_15)
        {
            connector_risk = (connector_risk * 90) / 100;
            wire_risk = (wire_risk * 88) / 100;
            thermal_damage_risk = (thermal_damage_risk * 86) / 100;
        }
    }
    else
    {
        decision = AI_DECISION_NONE;
        confidence = 0;
        gPowerData.ai_prevent_latch = false;
    }

    gPowerData.current.ai_fault_flags = fault_flags;
    gPowerData.current.ai_status = (uint8_t)ai_status;
    gPowerData.current.anomaly_score_pct = ClampU16(anomaly);
    gPowerData.current.degradation_drift_pct = ClampU8(drift_pct);
    gPowerData.current.connector_wear_pct =
        ClampU8((thermal_wear_pct * 2 + drift_pct + ((int32_t)gPowerData.current.anomaly_score_pct / 2)) / 3);
    gPowerData.current.thermal_risk_s = (thermal_risk_s > 0) ? ClampU16(thermal_risk_s) : 0u;
    gPowerData.current.ai_decision = (uint8_t)decision;
    gPowerData.current.ai_confidence_pct = ClampU8(confidence);
    gPowerData.current.ai_prevented_events = gPowerData.ai_prevented_events;
    gPowerData.current.predicted_overtemp_s = (predicted_overtemp_s > 0) ? ClampU16(predicted_overtemp_s) : 0u;
    gPowerData.current.connector_risk_pct = ClampU8(connector_risk);
    gPowerData.current.wire_risk_pct = ClampU8(wire_risk);
    gPowerData.current.thermal_damage_risk_pct = ClampU8(thermal_damage_risk);
}

static void SeedDerivedStateFromCurrent(void)
{
    uint32_t i;
    int32_t temp_c10 = (int32_t)gPowerData.current.temp_c * 10;

    gPowerData.elapsed_ms_real = gPowerData.replay_index * POWER_SAMPLE_PERIOD_MS;
    gPowerData.elapsed_ms_sim = gPowerData.elapsed_ms_real * POWER_SIM_TIME_SCALE;
    gPowerData.ema_voltage_cV = (int32_t)gPowerData.current.voltage_mV;
    gPowerData.ema_current_mA = (int32_t)gPowerData.current.current_mA;
    gPowerData.ema_power_W = (int32_t)gPowerData.current.power_mW;
    gPowerData.ema_power_resid_W = 0;
    gPowerData.ema_temp_c10 = temp_c10;
    gPowerData.ambient_c10 = temp_c10;
    gPowerData.drift_c10_ema = 0;
    gPowerData.pack_temp_c10 = temp_c10;
    gPowerData.connector_temp_c10 = temp_c10;
    gPowerData.wear_q10 = 0;
    gPowerData.wear_stress_accum = 0u;
    gPowerData.temp_hist_idx = 0u;
    gPowerData.temp_hist_ready = false;
    gPowerData.status_hold_ticks = 0u;
    gPowerData.fault_latch_ticks = 0u;
    gPowerData.ai_prevented_events = 0u;
    gPowerData.ai_prevent_latch = false;

    for (i = 0u; i < POWER_TEMP_HISTORY_LEN; i++)
    {
        gPowerData.temp_hist_c10[i] = (int16_t)temp_c10;
    }

    UpdateAiModel(0);
    gPowerData.current.elapsed_charge_sim_s = ReplayIndexToSimSeconds(gPowerData.replay_index);
    gPowerData.current.elapsed_charge_s = ReplayIndexToReplaySeconds(gPowerData.replay_index);
}

void PowerData_Init(void)
{
    uint32_t i;

    gPowerData.mode = POWER_DATA_SOURCE_REPLAY;
    gPowerData.replay_index = 0u;
    gPowerData.replay_profile = POWER_REPLAY_PROFILE_WIRED;
    gPowerData.current = SampleFromReplay(0u, gPowerData.replay_profile);
    gPowerData.live_override = gPowerData.current;
    gPowerData.elapsed_ms_real = 0u;
    gPowerData.elapsed_ms_sim = 0u;
    gPowerData.ema_voltage_cV = (int32_t)gPowerData.current.voltage_mV;
    gPowerData.ema_current_mA = (int32_t)gPowerData.current.current_mA;
    gPowerData.ema_power_W = (int32_t)gPowerData.current.power_mW;
    gPowerData.ema_power_resid_W = 0;
    gPowerData.ema_temp_c10 = (int32_t)gPowerData.current.temp_c * 10;
    gPowerData.ambient_c10 = gPowerData.ema_temp_c10;
    gPowerData.drift_c10_ema = 0;
    gPowerData.pack_temp_c10 = gPowerData.ema_temp_c10;
    gPowerData.connector_temp_c10 = gPowerData.ema_temp_c10;
    gPowerData.wear_q10 = 0;
    gPowerData.wear_stress_accum = 0u;
    gPowerData.temp_hist_idx = 0u;
    gPowerData.temp_hist_ready = false;
    gPowerData.status_hold_ticks = 0u;
    gPowerData.fault_latch_ticks = 0u;
    gPowerData.replay_hour_lock = false;
    gPowerData.replay_hour_start_tick = 0u;
    gPowerData.replay_hour_end_tick = 0u;
    gPowerData.ai_assist_enabled = true;
    gPowerData.ai_prevented_events = 0u;
    gPowerData.ai_prevent_latch = false;

    for (i = 0u; i < POWER_TEMP_HISTORY_LEN; i++)
    {
        gPowerData.temp_hist_c10[i] = (int16_t)gPowerData.ema_temp_c10;
    }

    UpdateAiModel(0);
}

void PowerData_SetMode(power_data_source_mode_t mode)
{
    gPowerData.mode = mode;
    if (mode == POWER_DATA_SOURCE_LIVE_OVERRIDE)
    {
        gPowerData.current = gPowerData.live_override;
    }
}

power_data_source_mode_t PowerData_GetMode(void)
{
    return gPowerData.mode;
}

void PowerData_SetLiveOverride(const power_sample_t *sample)
{
    if (sample == 0)
    {
        return;
    }

    gPowerData.live_override = *sample;
    if (gPowerData.mode == POWER_DATA_SOURCE_LIVE_OVERRIDE)
    {
        power_sample_t prev = gPowerData.current;
        gPowerData.current = gPowerData.live_override;
        UpdateAiModel(&prev);
    }
}

void PowerData_Tick(void)
{
    power_sample_t prev = gPowerData.current;

    if (gPowerData.mode == POWER_DATA_SOURCE_LIVE_OVERRIDE)
    {
        gPowerData.current = gPowerData.live_override;
        UpdateAiModel(&prev);
        return;
    }

    if (gPowerData.replay_hour_lock)
    {
        gPowerData.replay_index++;
        if (gPowerData.replay_index >= gPowerData.replay_hour_end_tick)
        {
            gPowerData.replay_index = gPowerData.replay_hour_start_tick;
        }
    }
    else
    {
        gPowerData.replay_index = (gPowerData.replay_index + 1u) % REPLAY_CYCLE_TICKS;
    }
    gPowerData.current = SampleFromReplay(gPowerData.replay_index, gPowerData.replay_profile);
    UpdateAiModel(&prev);
    gPowerData.current.elapsed_charge_sim_s = ReplayIndexToSimSeconds(gPowerData.replay_index);
    gPowerData.current.elapsed_charge_s = ReplayIndexToReplaySeconds(gPowerData.replay_index);
}

const power_sample_t *PowerData_Get(void)
{
    return &gPowerData.current;
}

const char *PowerData_ModeName(void)
{
    return (gPowerData.mode == POWER_DATA_SOURCE_LIVE_OVERRIDE) ? "live_override" : "replay";
}

void PowerData_SetReplayHour(uint8_t hour)
{
    uint32_t hour_idx = (hour > 0u) ? ((uint32_t)hour - 1u) : 0u;
    uint32_t start_tick;
    uint32_t end_tick;

    if (hour_idx >= REPLAY_CYCLE_HOURS)
    {
        hour_idx = REPLAY_CYCLE_HOURS - 1u;
    }

    start_tick = hour_idx * HOUR_TICKS;
    end_tick = start_tick + HOUR_TICKS;
    if (end_tick > REPLAY_CYCLE_TICKS)
    {
        end_tick = REPLAY_CYCLE_TICKS;
    }
    if (end_tick <= start_tick)
    {
        end_tick = start_tick + 1u;
    }

    gPowerData.replay_hour_start_tick = start_tick;
    gPowerData.replay_hour_end_tick = end_tick;
    gPowerData.replay_hour_lock = true;
    gPowerData.replay_index = start_tick;
    gPowerData.current = SampleFromReplay(gPowerData.replay_index, gPowerData.replay_profile);

    if (gPowerData.mode == POWER_DATA_SOURCE_REPLAY)
    {
        SeedDerivedStateFromCurrent();
    }
}

void PowerData_SetAiAssistEnabled(bool enabled)
{
    gPowerData.ai_assist_enabled = enabled;
    gPowerData.ai_prevent_latch = false;
}

void PowerData_SetReplayProfile(power_replay_profile_t profile)
{
    if ((profile != POWER_REPLAY_PROFILE_WIRED) && (profile != POWER_REPLAY_PROFILE_OUTLET))
    {
        return;
    }

    if (gPowerData.replay_profile == profile)
    {
        return;
    }

    gPowerData.replay_profile = profile;
    gPowerData.current = SampleFromReplay(gPowerData.replay_index, gPowerData.replay_profile);
    gPowerData.current.elapsed_charge_sim_s = ReplayIndexToSimSeconds(gPowerData.replay_index);
    gPowerData.current.elapsed_charge_s = ReplayIndexToReplaySeconds(gPowerData.replay_index);
}

power_replay_profile_t PowerData_GetReplayProfile(void)
{
    return gPowerData.replay_profile;
}
