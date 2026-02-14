#ifndef EDGEAI_EV_GAUGE_STYLE_H
#define EDGEAI_EV_GAUGE_STYLE_H

#include <stdint.h>

typedef struct
{
    uint16_t bg_black;
    uint16_t panel_black;
    uint16_t bezel_dark;
    uint16_t bezel_light;
    uint16_t text_primary;
    uint16_t text_secondary;
    uint16_t needle_amber;
    uint16_t accent_red;
    uint16_t accent_green;
    uint16_t accent_blue;
} gauge_palette_t;

typedef struct
{
    uint16_t center_x;
    uint16_t center_y;
    uint16_t radius;
    uint8_t ring_thickness;
    uint8_t major_ticks;
    uint8_t minor_ticks_per_major;
    uint8_t digital_strip_height;
} gauge_layout_t;

typedef struct
{
    gauge_palette_t palette;
    gauge_layout_t speed_gauge;
    gauge_layout_t power_gauge;
    gauge_layout_t battery_gauge;
} gauge_style_preset_t;

const gauge_style_preset_t *GaugeStyle_GetCockpitPreset(void);
void GaugeStyle_LogPreset(void);

#endif
