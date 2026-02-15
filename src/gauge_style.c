#include "gauge_style.h"

#include "fsl_debug_console.h"

#define RGB565(r, g, b) (uint16_t)((((uint16_t)(r) & 0xF8u) << 8) | (((uint16_t)(g) & 0xFCu) << 3) | (((uint16_t)(b) & 0xF8u) >> 3))

static const gauge_style_preset_t kCockpitPreset = {
    .palette =
        {
            .bg_black = RGB565(0, 0, 0),
            .panel_black = RGB565(8, 10, 12),
            .bezel_dark = RGB565(22, 3, 8),
            .bezel_light = RGB565(224, 226, 230),
            .text_primary = RGB565(236, 238, 242),
            .text_secondary = RGB565(164, 168, 176),
            .needle_amber = RGB565(255, 208, 52),
            .accent_red = RGB565(224, 24, 38),
            .accent_green = RGB565(44, 198, 94),
            .accent_blue = RGB565(44, 112, 230),
        },
    .speed_gauge =
        {
            .center_x = 150,
            .center_y = 140,
            .radius = 112,
            .ring_thickness = 10,
            .major_ticks = 9,
            .minor_ticks_per_major = 4,
            .digital_strip_height = 18,
        },
    .power_gauge =
        {
            .center_x = 340,
            .center_y = 140,
            .radius = 80,
            .ring_thickness = 8,
            .major_ticks = 7,
            .minor_ticks_per_major = 3,
            .digital_strip_height = 16,
        },
    .battery_gauge =
        {
            .center_x = 418,
            .center_y = 56,
            .radius = 34,
            .ring_thickness = 5,
            .major_ticks = 6,
            .minor_ticks_per_major = 2,
            .digital_strip_height = 10,
        },
};

const gauge_style_preset_t *GaugeStyle_GetCockpitPreset(void)
{
    return &kCockpitPreset;
}

void GaugeStyle_LogPreset(void)
{
    const gauge_style_preset_t *preset = GaugeStyle_GetCockpitPreset();

    PRINTF("Gauge style: cockpit_dark_v1\r\n");
    PRINTF("  Palette RGB565 bg=%u panel=%u bezel=%u/%u needle=%u\r\n",
           preset->palette.bg_black,
           preset->palette.panel_black,
           preset->palette.bezel_dark,
           preset->palette.bezel_light,
           preset->palette.needle_amber);
    PRINTF("  Speed gauge center=(%u,%u) r=%u\r\n",
           preset->speed_gauge.center_x,
           preset->speed_gauge.center_y,
           preset->speed_gauge.radius);
}
