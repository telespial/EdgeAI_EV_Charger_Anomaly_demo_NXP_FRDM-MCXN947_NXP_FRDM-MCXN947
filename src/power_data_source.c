#include "power_data_source.h"

#include "replay_trace_generated.h"

#define POWER_SAMPLE_PERIOD_MS 50u
#define POWER_SIM_TIME_SCALE 60u
#define POWER_TEMP_HISTORY_LEN 20u

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
} power_data_state_t;

static power_data_state_t gPowerData;

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

static power_sample_t SampleFromReplay(uint32_t index)
{
    power_sample_t out;
    const replay_trace_point_t *src = &kReplayTrace_Default[index % REPLAY_TRACE_DEFAULT_LEN];

    out.current_mA = src->current_mA;
    out.power_mW = src->power_mW;
    out.voltage_mV = src->voltage_mV;
    out.soc_pct = src->soc_pct;
    out.temp_c = src->temp_c;
    out.anomaly_score_pct = 0u;
    out.connector_wear_pct = 0u;
    out.ai_status = AI_STATUS_NORMAL;
    out.ai_fault_flags = 0u;
    out.thermal_risk_s = 0u;
    out.degradation_drift_pct = 0u;
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
    int32_t drift_pct;
    uint8_t fault_flags = 0u;
    ai_status_t ai_status = AI_STATUS_NORMAL;
    bool voltage_sag;
    bool current_spike;
    bool power_unstable;
    int32_t thermal_target_c10;
    int32_t connector_target_c10;
    int32_t stress = 0;
    int32_t thermal_wear_pct;

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
    current_spike = di > 2500;
    power_unstable = (dp > 700) || (p_resid_W > 900) || (gPowerData.ema_power_resid_W > 650);

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

    drift_pct = (gPowerData.drift_c10_ema * 100) / 180;
    if (drift_pct < 0)
    {
        drift_pct = 0;
    }
    if (drift_pct > 100)
    {
        drift_pct = 100;
    }

    if ((v_cV < 22300) || (temp_c10 >= 720) || ((fault_flags & (AI_FAULT_VOLTAGE_SAG | AI_FAULT_POWER_UNSTABLE)) == (AI_FAULT_VOLTAGE_SAG | AI_FAULT_POWER_UNSTABLE)) ||
        ((fault_flags & (AI_FAULT_CURRENT_SPIKE | AI_FAULT_POWER_UNSTABLE)) == (AI_FAULT_CURRENT_SPIKE | AI_FAULT_POWER_UNSTABLE)) ||
        ((thermal_risk_s > 0) && (thermal_risk_s <= 45)) || (anomaly >= 80))
    {
        ai_status = AI_STATUS_FAULT;
        gPowerData.status_hold_ticks = 60u;
    }
    else if ((anomaly >= 45) || (drift_pct >= 35) || ((thermal_risk_s > 0) && (thermal_risk_s <= 180)) || (dv > 90))
    {
        ai_status = AI_STATUS_WARNING;
        if (gPowerData.status_hold_ticks < 20u)
        {
            gPowerData.status_hold_ticks = 20u;
        }
    }
    else if (gPowerData.status_hold_ticks > 0u)
    {
        ai_status = (gPowerData.current.ai_status == AI_STATUS_FAULT) ? AI_STATUS_FAULT : AI_STATUS_WARNING;
        gPowerData.status_hold_ticks--;
    }

    gPowerData.current.ai_fault_flags = fault_flags;
    gPowerData.current.ai_status = (uint8_t)ai_status;
    gPowerData.current.anomaly_score_pct = ClampU16(anomaly);
    gPowerData.current.degradation_drift_pct = ClampU8(drift_pct);
    gPowerData.current.connector_wear_pct =
        ClampU8((thermal_wear_pct * 2 + drift_pct + ((int32_t)gPowerData.current.anomaly_score_pct / 2)) / 3);
    gPowerData.current.thermal_risk_s = (thermal_risk_s > 0) ? ClampU16(thermal_risk_s) : 0u;
}

void PowerData_Init(void)
{
    uint32_t i;

    gPowerData.mode = POWER_DATA_SOURCE_REPLAY;
    gPowerData.replay_index = 0u;
    gPowerData.current = SampleFromReplay(0u);
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

    gPowerData.replay_index = (gPowerData.replay_index + 1u) % REPLAY_TRACE_DEFAULT_LEN;
    gPowerData.current = SampleFromReplay(gPowerData.replay_index);
    UpdateAiModel(&prev);
}

const power_sample_t *PowerData_Get(void)
{
    return &gPowerData.current;
}

const char *PowerData_ModeName(void)
{
    return (gPowerData.mode == POWER_DATA_SOURCE_LIVE_OVERRIDE) ? "live_override" : "replay";
}
