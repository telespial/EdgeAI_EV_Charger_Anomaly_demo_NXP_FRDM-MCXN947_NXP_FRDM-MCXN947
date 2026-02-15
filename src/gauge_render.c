#include "gauge_render.h"

#include <stdio.h>

#include "gauge_style.h"
#include "par_lcd_s035.h"
#include "text5x7.h"

static bool gLcdReady = false;
static bool gStaticReady = false;
static bool gDynamicReady = false;
static int32_t gPrevMainIdx = -1;
static int32_t gPrevLeftIdx = -1;
static int32_t gPrevRightIdx = -1;
static uint16_t gPrevCurrent = 0u;
static uint16_t gPrevPower = 0u;
static uint16_t gPrevVoltage = 0u;
static uint8_t gPrevSoc = 0u;
static uint8_t gTraceCurrent[96];
static uint8_t gTracePower[96];
static uint8_t gTraceVoltage[96];
static uint8_t gTraceHead = 0u;
static bool gTraceReady = false;

#define RGB565(r, g, b) (uint16_t)((((uint16_t)(r) & 0xF8u) << 8) | (((uint16_t)(g) & 0xFCu) << 3) | (((uint16_t)(b) & 0xF8u) >> 3))

enum
{
    PANEL_X0 = 18,
    PANEL_Y0 = 14,
    PANEL_X1 = 462,
    PANEL_Y1 = 306,
    MAIN_CX = 236,
    MAIN_CY = 158,
    LEFT_CX = 124,
    LEFT_CY = 168,
    RIGHT_CX = 350,
    RIGHT_CY = 168,
    MAIN_R = 76,
    SIDE_R = 47,
    SCOPE_X = 304,
    SCOPE_Y = 22,
    SCOPE_W = 150,
    SCOPE_H = 78,
};

static int32_t ClampI32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint8_t ScaleTo8(uint32_t value, uint32_t max_value)
{
    if (max_value == 0u)
    {
        return 0u;
    }
    if (value >= max_value)
    {
        return 255u;
    }
    return (uint8_t)((value * 255u) / max_value);
}

static void DrawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t width, uint16_t color)
{
    int32_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;
    int32_t half = (width > 1) ? (width / 2) : 0;

    for (;;)
    {
        par_lcd_s035_fill_rect(x0 - half, y0 - half, x0 + half, y0 + half, color);
        if ((x0 == x1) && (y0 == y1))
        {
            break;
        }

        {
            int32_t e2 = 2 * err;
            if (e2 >= dy)
            {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }
}

static void DrawRing(int32_t cx, int32_t cy, int32_t r_outer, int32_t thickness, uint16_t ring, uint16_t inner)
{
    par_lcd_s035_draw_filled_circle(cx, cy, r_outer, ring);
    par_lcd_s035_draw_filled_circle(cx, cy, r_outer - thickness, inner);
}

static void DrawGaugeTicks11(int32_t cx, int32_t cy, int32_t radius, uint16_t major_color, uint16_t minor_color)
{
    static const int8_t dx_lut[11] = {-39, -35, -27, -17, -7, 0, 7, 17, 27, 35, 39};
    static const int8_t dy_lut[11] = {13, -3, -16, -27, -35, -39, -35, -27, -16, -3, 13};
    int32_t i;
    for (i = 0; i < 11; i++)
    {
        int32_t x = cx + (dx_lut[i] * radius) / 47;
        int32_t y = cy + (dy_lut[i] * radius) / 47;
        uint16_t c = ((i % 2) == 0) ? major_color : minor_color;
        par_lcd_s035_fill_rect(x - 1, y - 1, x + 1, y + 1, c);
    }
}

static void DrawGaugeTicks13(int32_t cx, int32_t cy, int32_t radius, uint16_t major_color, uint16_t minor_color)
{
    static const int8_t dx_lut[13] = {-66, -62, -56, -46, -34, -20, -7, 7, 20, 34, 46, 56, 62};
    static const int8_t dy_lut[13] = {20, 2, -16, -33, -46, -56, -62, -62, -56, -46, -33, -16, 2};
    int32_t i;
    for (i = 0; i < 13; i++)
    {
        int32_t x = cx + (dx_lut[i] * radius) / 76;
        int32_t y = cy + (dy_lut[i] * radius) / 76;
        uint16_t c = ((i % 2) == 0) ? major_color : minor_color;
        par_lcd_s035_fill_rect(x - 1, y - 1, x + 1, y + 1, c);
    }
}

static void DrawNeedle11(int32_t cx, int32_t cy, int32_t radius, int32_t idx, uint16_t color, int32_t width)
{
    static const int8_t dx_lut[11] = {-35, -31, -24, -15, -6, 0, 6, 15, 24, 31, 35};
    static const int8_t dy_lut[11] = {11, -2, -14, -23, -30, -34, -30, -23, -14, -2, 11};
    int32_t x1;
    int32_t y1;

    idx = ClampI32(idx, 0, 10);
    x1 = cx + (dx_lut[idx] * radius) / 47;
    y1 = cy + (dy_lut[idx] * radius) / 47;
    DrawLine(cx, cy, x1, y1, width, color);
    par_lcd_s035_draw_filled_circle(cx, cy, 4, color);
}

static void DrawNeedle13(int32_t cx, int32_t cy, int32_t radius, int32_t idx, uint16_t color, int32_t width)
{
    static const int8_t dx_lut[13] = {-58, -54, -48, -39, -28, -17, -6, 6, 17, 28, 39, 48, 54};
    static const int8_t dy_lut[13] = {18, 1, -15, -29, -42, -51, -56, -56, -51, -42, -29, -15, 1};
    int32_t x1;
    int32_t y1;

    idx = ClampI32(idx, 0, 12);
    x1 = cx + (dx_lut[idx] * radius) / 76;
    y1 = cy + (dy_lut[idx] * radius) / 76;
    DrawLine(cx, cy, x1, y1, width, color);
    par_lcd_s035_draw_filled_circle(cx, cy, 5, color);
}

static void DrawScopeFrame(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(SCOPE_X, SCOPE_Y, SCOPE_X + SCOPE_W, SCOPE_Y + SCOPE_H, style->palette.bezel_dark);
    par_lcd_s035_fill_rect(SCOPE_X + 2, SCOPE_Y + 2, SCOPE_X + SCOPE_W - 2, SCOPE_Y + SCOPE_H - 2, RGB565(7, 10, 12));
    edgeai_text5x7_draw_scaled(SCOPE_X + 8, SCOPE_Y + 6, 1, "20HZ TRACE", style->palette.needle_amber);
}

static void DrawScopeDynamic(const gauge_style_preset_t *style)
{
    int32_t px0 = SCOPE_X + 6;
    int32_t py0 = SCOPE_Y + 18;
    int32_t pw = SCOPE_W - 12;
    int32_t ph = SCOPE_H - 24;
    int32_t i;
    int32_t baseline_white = py0 + (ph * 3) / 4;
    int32_t baseline_red = py0 + (ph * 2) / 5;
    uint8_t prev_white = 0u;
    uint8_t prev_red = 0u;
    uint8_t prev_yellow = 0u;

    par_lcd_s035_fill_rect(px0, py0, px0 + pw, py0 + ph, RGB565(4, 6, 8));
    DrawLine(px0, baseline_white, px0 + pw, baseline_white, 1, style->palette.text_primary);
    DrawLine(px0, baseline_red, px0 + pw, baseline_red, 1, style->palette.accent_red);

    for (i = 0; i < pw; i++)
    {
        uint8_t idx = (uint8_t)((gTraceHead + i) % (int32_t)(sizeof(gTraceCurrent)));
        uint8_t white = (uint8_t)(baseline_white - ((gTracePower[idx] * (ph / 3)) / 255u));
        uint8_t red = (uint8_t)(baseline_red - ((gTraceVoltage[idx] * (ph / 3)) / 255u));
        uint8_t yellow = (uint8_t)((py0 + ph - 2) - ((gTraceCurrent[idx] * (ph - 4)) / 255u));

        if (i > 0)
        {
            DrawLine(px0 + i - 1, prev_white, px0 + i, white, 1, style->palette.text_primary);
            DrawLine(px0 + i - 1, prev_red, px0 + i, red, 1, style->palette.accent_red);
            DrawLine(px0 + i - 1, prev_yellow, px0 + i, yellow, 1, style->palette.needle_amber);
        }

        prev_white = white;
        prev_red = red;
        prev_yellow = yellow;
    }
}

static void DrawStaticDashboard(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill(style->palette.bg_black);
    par_lcd_s035_fill_rect(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, style->palette.panel_black);
    par_lcd_s035_fill_rect(PANEL_X0 + 4, PANEL_Y0 + 4, PANEL_X1 - 4, PANEL_Y0 + 34, RGB565(18, 20, 24));
    edgeai_text5x7_draw_scaled(PANEL_X0 + 14, PANEL_Y0 + 14, 2, "EV CHARGE MONITOR", style->palette.text_primary);

    DrawRing(MAIN_CX, MAIN_CY, MAIN_R, 10, style->palette.bezel_light, RGB565(10, 12, 16));
    DrawRing(MAIN_CX, MAIN_CY, MAIN_R - 10, 4, style->palette.bezel_dark, style->palette.panel_black);
    DrawRing(LEFT_CX, LEFT_CY, SIDE_R, 8, style->palette.bezel_dark, RGB565(10, 12, 16));
    DrawRing(RIGHT_CX, RIGHT_CY, SIDE_R, 8, style->palette.bezel_dark, RGB565(10, 12, 16));

    DrawGaugeTicks13(MAIN_CX, MAIN_CY, MAIN_R - 6, style->palette.text_secondary, RGB565(90, 96, 104));
    DrawGaugeTicks11(LEFT_CX, LEFT_CY, SIDE_R - 4, style->palette.text_secondary, RGB565(96, 100, 108));
    DrawGaugeTicks11(RIGHT_CX, RIGHT_CY, SIDE_R - 4, style->palette.text_secondary, RGB565(96, 100, 108));

    edgeai_text5x7_draw_scaled(MAIN_CX - 24, MAIN_CY + 28, 2, "mV", style->palette.text_secondary);
    edgeai_text5x7_draw_scaled(LEFT_CX - 30, LEFT_CY + 34, 1, "CURRENT", style->palette.text_secondary);
    edgeai_text5x7_draw_scaled(LEFT_CX - 16, LEFT_CY + 46, 1, "mA", style->palette.text_secondary);
    edgeai_text5x7_draw_scaled(RIGHT_CX - 22, RIGHT_CY + 34, 1, "POWER", style->palette.text_secondary);
    edgeai_text5x7_draw_scaled(RIGHT_CX - 14, RIGHT_CY + 46, 1, "mW", style->palette.text_secondary);
    edgeai_text5x7_draw_scaled(MAIN_CX - 20, MAIN_CY + 62, 1, "SOC", style->palette.text_secondary);

    DrawLine(MAIN_CX - 24, MAIN_CY + 74, MAIN_CX + 24, MAIN_CY + 74, 2, RGB565(40, 45, 50));
    DrawScopeFrame(style);
}

static void DrawSocBar(uint8_t soc, const gauge_style_preset_t *style)
{
    int32_t x0 = MAIN_CX - 24;
    int32_t y0 = MAIN_CY + 70;
    int32_t w = 48;
    int32_t fill = (soc * w) / 100;

    par_lcd_s035_fill_rect(x0, y0, x0 + w, y0 + 7, RGB565(24, 28, 32));
    if (fill > 0)
    {
        uint16_t c = (soc < 25u) ? style->palette.accent_red : style->palette.accent_green;
        par_lcd_s035_fill_rect(x0 + 1, y0 + 1, x0 + fill - 1, y0 + 6, c);
    }
}

static void ClearDynamicValueBands(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(MAIN_CX - 40, MAIN_CY - 10, MAIN_CX + 40, MAIN_CY + 16, style->palette.panel_black);
    par_lcd_s035_fill_rect(LEFT_CX - 24, LEFT_CY - 10, LEFT_CX + 24, LEFT_CY + 10, style->palette.panel_black);
    par_lcd_s035_fill_rect(RIGHT_CX - 24, RIGHT_CY - 10, RIGHT_CX + 24, RIGHT_CY + 10, style->palette.panel_black);
    par_lcd_s035_fill_rect(MAIN_CX + 28, MAIN_CY + 62, MAIN_CX + 66, MAIN_CY + 76, style->palette.panel_black);
}

bool GaugeRender_Init(void)
{
    const gauge_style_preset_t *style;

    gLcdReady = par_lcd_s035_init();
    if (gLcdReady)
    {
        style = GaugeStyle_GetCockpitPreset();
        DrawStaticDashboard(style);
        gStaticReady = true;
        gDynamicReady = false;
        gPrevMainIdx = -1;
        gPrevLeftIdx = -1;
        gPrevRightIdx = -1;
        gTraceHead = 0u;
        gTraceReady = false;
    }
    return gLcdReady;
}

void GaugeRender_DrawFrame(const power_sample_t *sample)
{
    const gauge_style_preset_t *style;
    int32_t main_idx;
    int32_t left_idx;
    int32_t right_idx;
    char line[32];

    if (!gLcdReady || sample == 0)
    {
        return;
    }

    style = GaugeStyle_GetCockpitPreset();

    if (!gStaticReady)
    {
        DrawStaticDashboard(style);
        gStaticReady = true;
        gDynamicReady = false;
        gPrevMainIdx = -1;
        gPrevLeftIdx = -1;
        gPrevRightIdx = -1;
    }

    main_idx = ClampI32(((int32_t)sample->voltage_mV - 3600) / 55, 0, 12);
    left_idx = ClampI32((int32_t)sample->current_mA / 20, 0, 10);
    right_idx = ClampI32((int32_t)sample->power_mW / 260, 0, 10);

    gTraceCurrent[gTraceHead] = ScaleTo8(sample->current_mA, 220u);
    gTracePower[gTraceHead] = ScaleTo8(sample->power_mW, 2600u);
    gTraceVoltage[gTraceHead] = ScaleTo8((sample->voltage_mV > 3500u) ? (sample->voltage_mV - 3500u) : 0u, 850u);
    gTraceHead = (uint8_t)((gTraceHead + 1u) % sizeof(gTraceCurrent));
    if (gTraceHead == 0u)
    {
        gTraceReady = true;
    }

    if (gDynamicReady)
    {
        if (gPrevMainIdx != main_idx)
        {
            DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 10, gPrevMainIdx, style->palette.panel_black, 3);
            DrawGaugeTicks13(MAIN_CX, MAIN_CY, MAIN_R - 6, style->palette.text_secondary, RGB565(90, 96, 104));
            DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 10, main_idx, style->palette.needle_amber, 3);
            gPrevMainIdx = main_idx;
        }
        if (gPrevLeftIdx != left_idx)
        {
            DrawNeedle11(LEFT_CX, LEFT_CY, SIDE_R - 9, gPrevLeftIdx, style->palette.panel_black, 2);
            DrawGaugeTicks11(LEFT_CX, LEFT_CY, SIDE_R - 4, style->palette.text_secondary, RGB565(96, 100, 108));
            DrawNeedle11(LEFT_CX, LEFT_CY, SIDE_R - 9, left_idx, style->palette.text_primary, 2);
            gPrevLeftIdx = left_idx;
        }
        if (gPrevRightIdx != right_idx)
        {
            DrawNeedle11(RIGHT_CX, RIGHT_CY, SIDE_R - 9, gPrevRightIdx, style->palette.panel_black, 2);
            DrawGaugeTicks11(RIGHT_CX, RIGHT_CY, SIDE_R - 4, style->palette.text_secondary, RGB565(96, 100, 108));
            DrawNeedle11(RIGHT_CX, RIGHT_CY, SIDE_R - 9, right_idx, style->palette.accent_red, 2);
            gPrevRightIdx = right_idx;
        }
    }
    else
    {
        DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 10, main_idx, style->palette.needle_amber, 3);
        DrawNeedle11(LEFT_CX, LEFT_CY, SIDE_R - 9, left_idx, style->palette.text_primary, 2);
        DrawNeedle11(RIGHT_CX, RIGHT_CY, SIDE_R - 9, right_idx, style->palette.accent_red, 2);
        gPrevMainIdx = main_idx;
        gPrevLeftIdx = left_idx;
        gPrevRightIdx = right_idx;
        gDynamicReady = true;
    }

    DrawScopeDynamic(style);

    if (!gDynamicReady || gPrevCurrent != sample->current_mA || gPrevPower != sample->power_mW ||
        gPrevVoltage != sample->voltage_mV || gPrevSoc != sample->soc_pct)
    {
        ClearDynamicValueBands(style);
    }

    if (!gDynamicReady || gPrevVoltage != sample->voltage_mV)
    {
        snprintf(line, sizeof(line), "%4u", sample->voltage_mV);
        edgeai_text5x7_draw_scaled(MAIN_CX - 34, MAIN_CY - 8, 3, line, style->palette.text_primary);
    }

    if (!gDynamicReady || gPrevCurrent != sample->current_mA)
    {
        snprintf(line, sizeof(line), "%3u", sample->current_mA);
        edgeai_text5x7_draw_scaled(LEFT_CX - 20, LEFT_CY - 8, 2, line, style->palette.text_primary);
    }

    if (!gDynamicReady || gPrevPower != sample->power_mW)
    {
        snprintf(line, sizeof(line), "%4u", sample->power_mW);
        edgeai_text5x7_draw_scaled(RIGHT_CX - 20, RIGHT_CY - 8, 2, line, style->palette.text_primary);
    }

    if (!gDynamicReady || gPrevSoc != sample->soc_pct)
    {
        DrawSocBar(sample->soc_pct, style);
        snprintf(line, sizeof(line), "%3u%%", sample->soc_pct);
        edgeai_text5x7_draw_scaled(MAIN_CX + 30, MAIN_CY + 64, 1, line, style->palette.accent_green);
    }

    gPrevCurrent = sample->current_mA;
    gPrevPower = sample->power_mW;
    gPrevVoltage = sample->voltage_mV;
    gPrevSoc = sample->soc_pct;
    (void)gTraceReady;
}
