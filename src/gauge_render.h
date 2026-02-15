#ifndef EDGEAI_EV_GAUGE_RENDER_H
#define EDGEAI_EV_GAUGE_RENDER_H

#include <stdbool.h>

#include "power_data_source.h"

#define GAUGE_RENDER_AI_PILL_X0 336
#define GAUGE_RENDER_AI_PILL_Y0 5
#define GAUGE_RENDER_AI_PILL_X1 442
#define GAUGE_RENDER_AI_PILL_Y1 21

bool GaugeRender_Init(void);
void GaugeRender_DrawFrame(const power_sample_t *sample, bool ai_enabled);

#endif
