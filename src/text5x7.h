#pragma once

#include <stdint.h>

/* Minimal 5x7 text renderer for direct-to-LCD drawing (via fill-rect).
 * This is used for boot titles and small status text in raster mode.
 */

int32_t edgeai_text5x7_width(int32_t scale, const char *s);
void edgeai_text5x7_draw_scaled(int32_t x, int32_t y, int32_t scale, const char *s, uint16_t rgb565);

