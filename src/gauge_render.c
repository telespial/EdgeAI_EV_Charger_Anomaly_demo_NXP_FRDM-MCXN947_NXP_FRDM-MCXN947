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
static uint8_t gPrevTemp = 0u;
static uint16_t gPrevCpuPct = 0u;
static uint8_t gTraceCurrent[96];
static uint8_t gTracePower[96];
static uint8_t gTraceVoltage[96];
static uint8_t gTraceHead = 0u;
static bool gTraceReady = false;
static uint32_t gFrameCounter = 0u;

#define RGB565(r, g, b) (uint16_t)((((uint16_t)(r) & 0xF8u) << 8) | (((uint16_t)(g) & 0xFCu) << 3) | (((uint16_t)(b) & 0xF8u) >> 3))

enum
{
    PANEL_X0 = 18,
    PANEL_Y0 = 14,
    PANEL_X1 = 462,
    PANEL_Y1 = 306,
    MAIN_CX = 236,
    MAIN_CY = 170,
    MAIN_R = 84,
    MID_TOP_CX = 98,
    MID_TOP_CY = 110,
    MID_BOT_CX = 98,
    MID_BOT_CY = 234,
    MID_R = 42,
    BATT_X = MAIN_CX - 41,
    BATT_Y = 48,
    BATT_W = 82,
    BATT_H = 26,
    SCOPE_X = 324,
    SCOPE_Y = 24,
    SCOPE_W = 128,
    SCOPE_H = 92,
    TERM_X = 324,
    TERM_Y = 124,
    TERM_W = 128,
    TERM_H = 176,
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
    static const int8_t dx_lut[11] = {-94, -83, -67, -47, -24, 0, 24, 47, 67, 83, 94};
    static const int8_t dy_lut[11] = {-34, -56, -74, -88, -97, -100, -97, -88, -74, -56, -34};
    int32_t i;
    for (i = 0; i < 11; i++)
    {
        int32_t x = cx + (dx_lut[i] * radius) / 100;
        int32_t y = cy + (dy_lut[i] * radius) / 100;
        uint16_t c = ((i % 2) == 0) ? major_color : minor_color;
        par_lcd_s035_fill_rect(x - 1, y - 1, x + 1, y + 1, c);
    }
}

static void DrawGaugeTicks13(int32_t cx, int32_t cy, int32_t radius, uint16_t major_color, uint16_t minor_color)
{
    static const int8_t dx_lut[13] = {-94, -85, -73, -57, -40, -20, 0, 20, 40, 57, 73, 85, 94};
    static const int8_t dy_lut[13] = {-34, -52, -69, -82, -92, -98, -100, -98, -92, -82, -69, -52, -34};
    int32_t i;
    for (i = 0; i < 13; i++)
    {
        int32_t x = cx + (dx_lut[i] * radius) / 100;
        int32_t y = cy + (dy_lut[i] * radius) / 100;
        uint16_t c = ((i % 2) == 0) ? major_color : minor_color;
        par_lcd_s035_fill_rect(x - 1, y - 1, x + 1, y + 1, c);
    }
}

static void DrawNeedle11(int32_t cx, int32_t cy, int32_t radius, int32_t idx, uint16_t color, int32_t width)
{
    static const int8_t dx_lut[11] = {-94, -83, -67, -47, -24, 0, 24, 47, 67, 83, 94};
    static const int8_t dy_lut[11] = {-34, -56, -74, -88, -97, -100, -97, -88, -74, -56, -34};
    int32_t x1;
    int32_t y1;

    idx = ClampI32(idx, 0, 10);
    x1 = cx + (dx_lut[idx] * radius) / 100;
    y1 = cy + (dy_lut[idx] * radius) / 100;
    DrawLine(cx, cy, x1, y1, width, color);
    par_lcd_s035_draw_filled_circle(cx, cy, 4, color);
}

static void DrawNeedle13(int32_t cx, int32_t cy, int32_t radius, int32_t idx, uint16_t color, int32_t width)
{
    static const int8_t dx_lut[13] = {-94, -85, -73, -57, -40, -20, 0, 20, 40, 57, 73, 85, 94};
    static const int8_t dy_lut[13] = {-34, -52, -69, -82, -92, -98, -100, -98, -92, -82, -69, -52, -34};
    int32_t x1;
    int32_t y1;

    idx = ClampI32(idx, 0, 12);
    x1 = cx + (dx_lut[idx] * radius) / 100;
    y1 = cy + (dy_lut[idx] * radius) / 100;
    DrawLine(cx, cy, x1, y1, width, color);
    par_lcd_s035_draw_filled_circle(cx, cy, 5, color);
}

static void DrawScopeFrame(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(SCOPE_X, SCOPE_Y, SCOPE_X + SCOPE_W, SCOPE_Y + SCOPE_H, RGB565(18, 3, 7));
    par_lcd_s035_fill_rect(SCOPE_X + 2, SCOPE_Y + 2, SCOPE_X + SCOPE_W - 2, SCOPE_Y + SCOPE_H - 2, RGB565(7, 10, 12));
    edgeai_text5x7_draw_scaled(SCOPE_X + 8, SCOPE_Y + 6, 1, "20HZ TRACE", RGB565(255, 208, 52));
    edgeai_text5x7_draw_scaled(SCOPE_X + 34, SCOPE_Y + SCOPE_H - 11, 1, "NXP EDGE AI", RGB565(255, 208, 52));
}

static void DrawTerminalFrame(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(TERM_X, TERM_Y, TERM_X + TERM_W, TERM_Y + TERM_H, RGB565(18, 3, 7));
    par_lcd_s035_fill_rect(TERM_X + 2, TERM_Y + 2, TERM_X + TERM_W - 2, TERM_Y + TERM_H - 2, RGB565(6, 9, 12));
    edgeai_text5x7_draw_scaled(TERM_X + 6, TERM_Y + 6, 1, "STATUS", style->palette.text_primary);
    DrawLine(TERM_X + 6, TERM_Y + 16, TERM_X + TERM_W - 6, TERM_Y + 16, 1, RGB565(110, 20, 30));
}

static void DrawTerminalDynamic(const gauge_style_preset_t *style, const power_sample_t *sample, uint16_t cpu_pct)
{
    char line[24];
    uint16_t ai_color = (sample->temp_c >= 50u || sample->soc_pct <= 18u) ? style->palette.accent_red : style->palette.accent_green;
    const char *ai_state = (ai_color == style->palette.accent_red) ? "ALERT" : "NORM";
    const char *mode_state = "MODE REP";

    par_lcd_s035_fill_rect(TERM_X + 6, TERM_Y + 22, TERM_X + TERM_W - 6, TERM_Y + TERM_H - 6, RGB565(6, 9, 12));

    snprintf(line, sizeof(line), "CPU %2u%%", cpu_pct);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 26, 1, line, style->palette.text_primary);

    snprintf(line, sizeof(line), "AI %s", ai_state);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 44, 1, line, ai_color);

    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 62, 1, mode_state, style->palette.text_secondary);

    snprintf(line, sizeof(line), "TEMP %2uC", sample->temp_c);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 80, 1, line, style->palette.text_primary);

    snprintf(line, sizeof(line), "SOC %2u%%", sample->soc_pct);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 98, 1, line, style->palette.text_primary);

    snprintf(line, sizeof(line), "CUR %3u", sample->current_mA);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 116, 1, line, style->palette.text_secondary);

    snprintf(line, sizeof(line), "PWR %4u", sample->power_mW);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 134, 1, line, style->palette.text_secondary);
}

static void DrawBatteryIndicatorFrame(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(BATT_X, BATT_Y, BATT_X + BATT_W, BATT_Y + BATT_H, RGB565(8, 10, 12));
    par_lcd_s035_fill_rect(BATT_X + BATT_W + 1, BATT_Y + 6, BATT_X + BATT_W + 5, BATT_Y + 14, style->palette.text_primary);
    par_lcd_s035_fill_rect(BATT_X + 1, BATT_Y + 1, BATT_X + BATT_W - 1, BATT_Y + BATT_H - 1, RGB565(22, 24, 28));
    edgeai_text5x7_draw_scaled(BATT_X - 20, BATT_Y + 6, 1, "BAT", style->palette.text_secondary);
}

static void DrawBatteryIndicatorDynamic(const gauge_style_preset_t *style, uint8_t soc)
{
    int32_t inner_x0 = BATT_X + 3;
    int32_t inner_y0 = BATT_Y + 3;
    int32_t inner_w = BATT_W - 6;
    int32_t inner_h = BATT_H - 6;
    int32_t fill = (soc * inner_w) / 100;
    char line[8];
    uint16_t fill_color = style->palette.accent_green;

    if (soc < 25u)
    {
        fill_color = style->palette.accent_red;
    }
    else if (soc < 50u)
    {
        fill_color = RGB565(255, 180, 24);
    }

    par_lcd_s035_fill_rect(inner_x0, inner_y0, inner_x0 + inner_w, inner_y0 + inner_h, RGB565(10, 12, 16));
    if (fill > 0)
    {
        par_lcd_s035_fill_rect(inner_x0, inner_y0, inner_x0 + fill, inner_y0 + inner_h, fill_color);
    }

    par_lcd_s035_fill_rect(BATT_X + BATT_W + 8, BATT_Y + 4, BATT_X + BATT_W + 38, BATT_Y + 16, RGB565(2, 3, 5));
    snprintf(line, sizeof(line), "%3u%%", soc);
    edgeai_text5x7_draw_scaled(BATT_X + BATT_W + 10, BATT_Y + 6, 1, line, style->palette.text_primary);
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
    uint8_t prev_green = 0u;

    par_lcd_s035_fill_rect(px0, py0, px0 + pw, py0 + ph, RGB565(4, 6, 8));
    DrawLine(px0, baseline_white, px0 + pw, baseline_white, 1, style->palette.text_primary);
    DrawLine(px0, baseline_red, px0 + pw, baseline_red, 1, style->palette.accent_red);

    for (i = 0; i < pw; i++)
    {
        uint8_t idx = (uint8_t)((gTraceHead + i) % (int32_t)(sizeof(gTraceCurrent)));
        uint8_t white = (uint8_t)(baseline_white - ((gTracePower[idx] * (ph / 3)) / 255u));
        uint8_t red = (uint8_t)(baseline_red - ((gTraceVoltage[idx] * (ph / 3)) / 255u));
        uint8_t green = (uint8_t)((py0 + ph - 2) - ((gTraceCurrent[idx] * (ph - 4)) / 255u));

        if (i > 0)
        {
            DrawLine(px0 + i - 1, prev_white, px0 + i, white, 1, style->palette.text_primary);
            DrawLine(px0 + i - 1, prev_red, px0 + i, red, 1, style->palette.accent_red);
            DrawLine(px0 + i - 1, prev_green, px0 + i, green, 1, style->palette.accent_green);
        }

        prev_white = white;
        prev_red = red;
        prev_green = green;
    }
}

static void DrawStaticDashboard(const gauge_style_preset_t *style)
{
    int32_t brand_x;

    par_lcd_s035_fill(RGB565(0, 0, 0));
    par_lcd_s035_fill_rect(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, RGB565(2, 3, 5));
    par_lcd_s035_fill_rect(PANEL_X0 + 4, PANEL_Y0 + 4, PANEL_X1 - 4, PANEL_Y0 + 30, RGB565(8, 10, 13));
    edgeai_text5x7_draw_scaled(PANEL_X0 + 14, PANEL_Y0 + 14, 1, "(c)RICHARD HABERKERN", style->palette.text_secondary);

    /* Left status accent bar: full vertical run up to the white guide line. */
    par_lcd_s035_fill_rect(22, 78, 47, 288, style->palette.accent_green);
    DrawLine(22, 78, 48, 78, 1, style->palette.text_primary);
    DrawLine(48, 78, 48, 258, 1, style->palette.text_primary);
    DrawLine(48, 164, 72, 164, 1, style->palette.text_primary);
    DrawLine(48, 258, 48, 288, 1, style->palette.text_primary);
    DrawLine(48, 288, 72, 288, 1, style->palette.text_primary);
    DrawLine(448, 78, 414, 78, 1, style->palette.text_primary);
    DrawLine(414, 78, 414, 258, 1, style->palette.text_primary);
    DrawLine(414, 258, 338, 258, 1, style->palette.text_primary);

    DrawRing(MAIN_CX, MAIN_CY, MAIN_R + 10, 6, RGB565(20, 2, 4), RGB565(6, 2, 3));
    DrawRing(MAIN_CX, MAIN_CY, MAIN_R + 2, 10, RGB565(118, 6, 12), RGB565(26, 2, 5));
    DrawRing(MAIN_CX, MAIN_CY, MAIN_R - 10, 4, RGB565(210, 26, 32), RGB565(8, 8, 10));
    DrawRing(MAIN_CX, MAIN_CY, MAIN_R - 14, 2, style->palette.text_primary, RGB565(9, 11, 14));

    DrawRing(MID_TOP_CX, MID_TOP_CY, MID_R, 8, style->palette.text_primary, RGB565(6, 8, 10));
    DrawRing(MID_TOP_CX, MID_TOP_CY, MID_R - 8, 4, RGB565(34, 34, 36), RGB565(8, 8, 10));
    DrawRing(MID_BOT_CX, MID_BOT_CY, MID_R, 8, style->palette.text_primary, RGB565(6, 8, 10));
    DrawRing(MID_BOT_CX, MID_BOT_CY, MID_R - 8, 4, RGB565(34, 34, 36), RGB565(8, 8, 10));

    DrawGaugeTicks13(MAIN_CX, MAIN_CY, MAIN_R - 18, style->palette.text_primary, RGB565(170, 170, 176));
    DrawGaugeTicks11(MID_TOP_CX, MID_TOP_CY, MID_R - 12, style->palette.text_primary, RGB565(140, 140, 145));
    DrawGaugeTicks11(MID_BOT_CX, MID_BOT_CY, MID_R - 12, style->palette.text_primary, RGB565(140, 140, 145));

    edgeai_text5x7_draw_scaled(MID_TOP_CX - 7, MID_TOP_CY + 10, 3, "A", style->palette.text_secondary);
    edgeai_text5x7_draw_scaled(MID_BOT_CX - 7, MID_BOT_CY + 10, 3, "W", style->palette.text_secondary);
    edgeai_text5x7_draw_scaled(MID_TOP_CX - 18, MID_TOP_CY + 50, 1, "CURRENT", style->palette.text_primary);
    edgeai_text5x7_draw_scaled(MID_BOT_CX - 14, MID_BOT_CY + 50, 1, "POWER", style->palette.text_primary);
    brand_x = ((PANEL_X0 + PANEL_X1) / 2) - (edgeai_text5x7_width(2, "NXP EDGEAI") / 2);
    edgeai_text5x7_draw_scaled(brand_x, 286, 2, "NXP EDGEAI", RGB565(255, 208, 52));

    DrawBatteryIndicatorFrame(style);
    DrawScopeFrame(style);
    DrawTerminalFrame(style);
}

static void ClearDynamicValueBands(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(MAIN_CX - 40, MAIN_CY - 10, MAIN_CX + 40, MAIN_CY + 16, style->palette.panel_black);
    par_lcd_s035_fill_rect(MID_TOP_CX - 24, MID_TOP_CY - 10, MID_TOP_CX + 24, MID_TOP_CY + 10, style->palette.panel_black);
    par_lcd_s035_fill_rect(MID_BOT_CX - 24, MID_BOT_CY - 10, MID_BOT_CX + 24, MID_BOT_CY + 10, style->palette.panel_black);
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
        gFrameCounter = 0u;
    }
    return gLcdReady;
}

void GaugeRender_DrawFrame(const power_sample_t *sample)
{
    const gauge_style_preset_t *style;
    int32_t main_idx;
    int32_t left_idx;
    int32_t right_idx;
    uint16_t cpu_pct;
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
    cpu_pct = (uint16_t)(22u + (sample->current_mA / 9u) + (sample->temp_c / 2u));
    if (cpu_pct > 98u)
    {
        cpu_pct = 98u;
    }
    gFrameCounter++;

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
            DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 24, gPrevMainIdx, style->palette.panel_black, 3);
            DrawGaugeTicks13(MAIN_CX, MAIN_CY, MAIN_R - 18, style->palette.text_primary, RGB565(170, 170, 176));
            DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 24, main_idx, style->palette.text_primary, 3);
            DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 30, main_idx, style->palette.accent_red, 1);
            gPrevMainIdx = main_idx;
        }
        if (gPrevLeftIdx != left_idx)
        {
            DrawNeedle11(MID_TOP_CX, MID_TOP_CY, MID_R - 20, gPrevLeftIdx, style->palette.panel_black, 2);
            DrawGaugeTicks11(MID_TOP_CX, MID_TOP_CY, MID_R - 12, style->palette.text_primary, RGB565(140, 140, 145));
            DrawNeedle11(MID_TOP_CX, MID_TOP_CY, MID_R - 20, left_idx, style->palette.text_primary, 2);
            gPrevLeftIdx = left_idx;
        }
        if (gPrevRightIdx != right_idx)
        {
            DrawNeedle11(MID_BOT_CX, MID_BOT_CY, MID_R - 20, gPrevRightIdx, style->palette.panel_black, 2);
            DrawGaugeTicks11(MID_BOT_CX, MID_BOT_CY, MID_R - 12, style->palette.text_primary, RGB565(140, 140, 145));
            DrawNeedle11(MID_BOT_CX, MID_BOT_CY, MID_R - 20, right_idx, style->palette.accent_red, 2);
            gPrevRightIdx = right_idx;
        }
    }
    else
    {
        DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 24, main_idx, style->palette.text_primary, 3);
        DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 30, main_idx, style->palette.accent_red, 1);
        DrawNeedle11(MID_TOP_CX, MID_TOP_CY, MID_R - 20, left_idx, style->palette.text_primary, 2);
        DrawNeedle11(MID_BOT_CX, MID_BOT_CY, MID_R - 20, right_idx, style->palette.accent_red, 2);
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
        int32_t x;
        snprintf(line, sizeof(line), "%4u", sample->voltage_mV);
        x = MAIN_CX - (edgeai_text5x7_width(3, line) / 2);
        edgeai_text5x7_draw_scaled(x, MAIN_CY - 8, 3, line, style->palette.text_primary);
    }

    if (!gDynamicReady || gPrevCurrent != sample->current_mA)
    {
        snprintf(line, sizeof(line), "%3u", sample->current_mA);
        edgeai_text5x7_draw_scaled(MID_TOP_CX - 20, MID_TOP_CY - 8, 2, line, style->palette.text_primary);
    }

    if (!gDynamicReady || gPrevPower != sample->power_mW)
    {
        snprintf(line, sizeof(line), "%4u", sample->power_mW);
        edgeai_text5x7_draw_scaled(MID_BOT_CX - 20, MID_BOT_CY - 8, 2, line, style->palette.text_primary);
    }

    if (!gDynamicReady || gPrevSoc != sample->soc_pct)
    {
        DrawBatteryIndicatorDynamic(style, sample->soc_pct);
    }

    if (!gDynamicReady || gPrevCurrent != sample->current_mA || gPrevPower != sample->power_mW ||
        gPrevVoltage != sample->voltage_mV || gPrevSoc != sample->soc_pct || gPrevTemp != sample->temp_c ||
        gPrevCpuPct != cpu_pct || ((gFrameCounter % 16u) == 0u))
    {
        DrawTerminalDynamic(style, sample, cpu_pct);
    }

    gPrevCurrent = sample->current_mA;
    gPrevPower = sample->power_mW;
    gPrevVoltage = sample->voltage_mV;
    gPrevSoc = sample->soc_pct;
    gPrevTemp = sample->temp_c;
    gPrevCpuPct = cpu_pct;
    (void)gTraceReady;
}
