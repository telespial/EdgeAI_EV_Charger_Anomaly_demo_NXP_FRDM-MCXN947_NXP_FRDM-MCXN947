#ifndef EDGEAI_EV_GAUGE_RENDER_H
#define EDGEAI_EV_GAUGE_RENDER_H

#include <stdbool.h>

#include "power_data_source.h"

#define GAUGE_RENDER_AI_PILL_X0 191
#define GAUGE_RENDER_AI_PILL_Y0 4
#define GAUGE_RENDER_AI_PILL_X1 288
#define GAUGE_RENDER_AI_PILL_Y1 30
#define GAUGE_RENDER_PROFILE_X0 34
#define GAUGE_RENDER_PROFILE_Y0 20
#define GAUGE_RENDER_PROFILE_X1 180
#define GAUGE_RENDER_PROFILE_Y1 30

bool GaugeRender_Init(void);
void GaugeRender_DrawFrame(const power_sample_t *sample, bool ai_enabled, power_replay_profile_t profile);
bool GaugeRender_HandleTouch(int32_t x, int32_t y, bool pressed);
uint8_t GaugeRender_GetTimelineHour(void);

#endif
