#ifndef EDGEAI_EV_POWER_DATA_SOURCE_H
#define EDGEAI_EV_POWER_DATA_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    AI_STATUS_NORMAL = 0,
    AI_STATUS_WARNING = 1,
    AI_STATUS_FAULT = 2,
} ai_status_t;

typedef enum
{
    AI_DECISION_NONE = 0,
    AI_DECISION_WATCH = 1,
    AI_DECISION_DERATE_15 = 2,
    AI_DECISION_DERATE_30 = 3,
    AI_DECISION_SHED_LOAD = 4,
} ai_decision_t;

enum
{
    AI_FAULT_VOLTAGE_SAG = (1u << 0),
    AI_FAULT_CURRENT_SPIKE = (1u << 1),
    AI_FAULT_POWER_UNSTABLE = (1u << 2),
};

typedef struct
{
    uint16_t current_mA;
    uint16_t power_mW;
    uint16_t voltage_mV;
    uint8_t soc_pct;
    uint8_t temp_c;
    uint16_t anomaly_score_pct;
    uint8_t connector_wear_pct;
    uint8_t ai_status;
    uint8_t ai_fault_flags;
    uint16_t thermal_risk_s;
    uint8_t degradation_drift_pct;
    uint8_t ai_decision;
    uint8_t ai_confidence_pct;
    uint16_t ai_prevented_events;
    uint16_t predicted_overtemp_s;
    uint8_t connector_risk_pct;
    uint8_t wire_risk_pct;
    uint8_t thermal_damage_risk_pct;
    uint32_t elapsed_charge_s;
    uint32_t elapsed_charge_sim_s;
} power_sample_t;

typedef enum
{
    POWER_DATA_SOURCE_REPLAY = 0,
    POWER_DATA_SOURCE_LIVE_OVERRIDE = 1,
} power_data_source_mode_t;

typedef enum
{
    POWER_REPLAY_PROFILE_WIRED = 0,
    POWER_REPLAY_PROFILE_OUTLET = 1,
} power_replay_profile_t;

void PowerData_Init(void);
void PowerData_SetMode(power_data_source_mode_t mode);
power_data_source_mode_t PowerData_GetMode(void);
void PowerData_SetLiveOverride(const power_sample_t *sample);
void PowerData_Tick(void);
const power_sample_t *PowerData_Get(void);
const char *PowerData_ModeName(void);
void PowerData_SetReplayHour(uint8_t hour);
void PowerData_SetAiAssistEnabled(bool enabled);
void PowerData_SetReplayProfile(power_replay_profile_t profile);
power_replay_profile_t PowerData_GetReplayProfile(void);

#endif
