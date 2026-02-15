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
    uint32_t elapsed_charge_s;
    uint32_t elapsed_charge_sim_s;
} power_sample_t;

typedef enum
{
    POWER_DATA_SOURCE_REPLAY = 0,
    POWER_DATA_SOURCE_LIVE_OVERRIDE = 1,
} power_data_source_mode_t;

void PowerData_Init(void);
void PowerData_SetMode(power_data_source_mode_t mode);
power_data_source_mode_t PowerData_GetMode(void);
void PowerData_SetLiveOverride(const power_sample_t *sample);
void PowerData_Tick(void);
const power_sample_t *PowerData_Get(void);
const char *PowerData_ModeName(void);

#endif
