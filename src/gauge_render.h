#ifndef EDGEAI_EV_GAUGE_RENDER_H
#define EDGEAI_EV_GAUGE_RENDER_H

#include <stdbool.h>

#include "power_data_source.h"

bool GaugeRender_Init(void);
void GaugeRender_DrawFrame(const power_sample_t *sample);

#endif
