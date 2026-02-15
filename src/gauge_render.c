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
static uint16_t gPrevAnomaly = 0u;
static uint8_t gPrevWear = 0u;
static uint32_t gPrevElapsedSim = 0u;
static uint8_t gTraceCurrent[96];
static uint8_t gTracePower[96];
static uint8_t gTraceVoltage[96];
static uint8_t gTraceHead = 0u;
static bool gTraceReady = false;
static uint32_t gFrameCounter = 0u;
static uint8_t gPrevBarLevel = 255u;
static uint8_t gPrevBarTemp = 255u;
static bool gPrevOverTemp = false;
static uint32_t gScopeNoiseSeed = 0x15B4A27Du;
static bool gPrevAiEnabled = false;
static uint8_t gPrevAiStatus = 255u;
static uint8_t gPrevAiFaultFlags = 255u;
static uint16_t gPrevThermalRisk = 65535u;
static uint8_t gPrevDrift = 255u;

#define RGB565(r, g, b) (uint16_t)((((uint16_t)(r) & 0xF8u) << 8) | (((uint16_t)(g) & 0xFCu) << 3) | (((uint16_t)(b) & 0xF8u) >> 3))

enum
{
    PANEL_X0 = 18,
    PANEL_Y0 = 14,
    PANEL_X1 = 462,
    PANEL_Y1 = 306,
    MAIN_CX = 236,
    MAIN_CY = 182,
    MAIN_R = 84,
    MID_TOP_CX = 98,
    MID_TOP_CY = 124,
    MID_BOT_CX = 98,
    MID_BOT_CY = 234,
    MID_R = 42,
    BATT_X = 48,
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
    BAR_X0 = 22,
    BAR_Y0 = 78,
    BAR_X1 = 47,
    BAR_Y1 = 288,
    BAR_SEGMENTS = 20,
    SECTION2_CX = 240,
    AI_PILL_X0 = GAUGE_RENDER_AI_PILL_X0,
    AI_PILL_Y0 = GAUGE_RENDER_AI_PILL_Y0,
    AI_PILL_X1 = GAUGE_RENDER_AI_PILL_X1,
    AI_PILL_Y1 = GAUGE_RENDER_AI_PILL_Y1,
    ALERT_X0 = 147,
    ALERT_Y0 = 48,
    ALERT_X1 = 325,
    ALERT_Y1 = 86,
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

static uint32_t NextNoise(void)
{
    gScopeNoiseSeed = (1103515245u * gScopeNoiseSeed) + 12345u;
    return gScopeNoiseSeed;
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
    (void)style;
    par_lcd_s035_fill_rect(SCOPE_X, SCOPE_Y, SCOPE_X + SCOPE_W, SCOPE_Y + SCOPE_H, RGB565(18, 3, 7));
    par_lcd_s035_fill_rect(SCOPE_X + 2, SCOPE_Y + 2, SCOPE_X + SCOPE_W - 2, SCOPE_Y + SCOPE_H - 2, RGB565(7, 10, 12));
    edgeai_text5x7_draw_scaled(SCOPE_X + 8, SCOPE_Y + 6, 1, "20HZ TRACE", RGB565(255, 208, 52));
}

static void DrawAiPill(const gauge_style_preset_t *style, bool ai_enabled)
{
    uint16_t fill = ai_enabled ? RGB565(22, 80, 28) : RGB565(45, 20, 18);
    uint16_t txt = ai_enabled ? RGB565(180, 255, 170) : RGB565(255, 210, 180);
    const char *label = ai_enabled ? "AI  ON" : "AI OFF";
    int32_t lx = AI_PILL_X0 + ((AI_PILL_X1 - AI_PILL_X0 + 1) - edgeai_text5x7_width(1, label)) / 2;

    par_lcd_s035_fill_rect(AI_PILL_X0, AI_PILL_Y0, AI_PILL_X1, AI_PILL_Y1, fill);
    DrawLine(AI_PILL_X0, AI_PILL_Y0, AI_PILL_X1, AI_PILL_Y0, 1, style->palette.text_primary);
    DrawLine(AI_PILL_X0, AI_PILL_Y1, AI_PILL_X1, AI_PILL_Y1, 1, style->palette.text_primary);
    DrawLine(AI_PILL_X0, AI_PILL_Y0, AI_PILL_X0, AI_PILL_Y1, 1, style->palette.text_primary);
    DrawLine(AI_PILL_X1, AI_PILL_Y0, AI_PILL_X1, AI_PILL_Y1, 1, style->palette.text_primary);
    edgeai_text5x7_draw_scaled(lx, AI_PILL_Y0 + 5, 1, label, txt);
}

static void DrawTerminalFrame(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(TERM_X, TERM_Y, TERM_X + TERM_W, TERM_Y + TERM_H, RGB565(18, 3, 7));
    par_lcd_s035_fill_rect(TERM_X + 2, TERM_Y + 2, TERM_X + TERM_W - 2, TERM_Y + TERM_H - 2, RGB565(6, 9, 12));
    edgeai_text5x7_draw_scaled(TERM_X + 6, TERM_Y + 6, 1, "STATUS", style->palette.text_primary);
    DrawLine(TERM_X + 6, TERM_Y + 16, TERM_X + TERM_W - 6, TERM_Y + 16, 1, RGB565(110, 20, 30));
}

static void DrawChargeClockDynamic(const gauge_style_preset_t *style, uint32_t elapsed_sim_s)
{
    uint32_t hh = elapsed_sim_s / 3600u;
    uint32_t mm = (elapsed_sim_s % 3600u) / 60u;
    uint32_t ss = elapsed_sim_s % 60u;
    char line[20];
    int32_t label_y = MAIN_CY + 30;
    int32_t label_x;
    int32_t value_x;

    par_lcd_s035_fill_rect(SECTION2_CX - 38, MAIN_CY + 26, SECTION2_CX + 38, MAIN_CY + 50, RGB565(138, 112, 10));
    label_x = SECTION2_CX - (edgeai_text5x7_width(1, "ELAPSED") / 2);
    edgeai_text5x7_draw_scaled(label_x, label_y, 1, "ELAPSED", style->palette.text_secondary);
    snprintf(line, sizeof(line), "%02u:%02u:%02u", (unsigned int)hh, (unsigned int)mm, (unsigned int)ss);
    value_x = SECTION2_CX - (edgeai_text5x7_width(1, line) / 2);
    edgeai_text5x7_draw_scaled(value_x, label_y + 10, 1, line, style->palette.text_primary);
}

static uint16_t AiStatusColor(const gauge_style_preset_t *style, uint8_t ai_status)
{
    if (ai_status == AI_STATUS_FAULT)
    {
        return style->palette.accent_red;
    }
    if (ai_status == AI_STATUS_WARNING)
    {
        return RGB565(255, 186, 42);
    }
    return style->palette.accent_green;
}

static const char *AiStatusText(uint8_t ai_status)
{
    if (ai_status == AI_STATUS_FAULT)
    {
        return "FAULT";
    }
    if (ai_status == AI_STATUS_WARNING)
    {
        return "WARNING";
    }
    return "NORMAL";
}

static void BuildFaultReason(const power_sample_t *sample, char *out, size_t out_len)
{
    if ((sample->ai_fault_flags & AI_FAULT_VOLTAGE_SAG) != 0u)
    {
        snprintf(out, out_len, "VOLTAGE SAG");
    }
    else if ((sample->ai_fault_flags & AI_FAULT_CURRENT_SPIKE) != 0u)
    {
        snprintf(out, out_len, "CURRENT SPIKE");
    }
    else if ((sample->ai_fault_flags & AI_FAULT_POWER_UNSTABLE) != 0u)
    {
        snprintf(out, out_len, "POWER UNSTABLE");
    }
    else if (sample->thermal_risk_s > 0u)
    {
        snprintf(out, out_len, "THERMAL RISK IN %us", sample->thermal_risk_s);
    }
    else if (sample->degradation_drift_pct >= 35u)
    {
        snprintf(out, out_len, "CONNECTOR DRIFT");
    }
    else
    {
        snprintf(out, out_len, "SYSTEM STABLE");
    }
}

static void DrawAiAlertOverlay(const gauge_style_preset_t *style, const power_sample_t *sample, bool ai_enabled)
{
    uint16_t color;
    int32_t tx;
    bool flash_on;
    char detail[30];

    par_lcd_s035_fill_rect(ALERT_X0, ALERT_Y0, ALERT_X1, ALERT_Y1, RGB565(2, 3, 5));

    if (!ai_enabled)
    {
        return;
    }

    if (sample->ai_status == AI_STATUS_NORMAL)
    {
        color = style->palette.accent_green;
        par_lcd_s035_fill_rect(ALERT_X0 + 1, ALERT_Y0 + 1, ALERT_X1 - 1, ALERT_Y1 - 1, RGB565(6, 16, 10));
        DrawLine(ALERT_X0, ALERT_Y0, ALERT_X1, ALERT_Y0, 2, color);
        DrawLine(ALERT_X0, ALERT_Y1, ALERT_X1, ALERT_Y1, 2, color);
        DrawLine(ALERT_X0, ALERT_Y0, ALERT_X0, ALERT_Y1, 2, color);
        DrawLine(ALERT_X1, ALERT_Y0, ALERT_X1, ALERT_Y1, 2, color);
        tx = ALERT_X0 + ((ALERT_X1 - ALERT_X0 + 1) - edgeai_text5x7_width(2, "SYSTEM STABLE")) / 2;
        edgeai_text5x7_draw_scaled(tx, ALERT_Y0 + 12, 2, "SYSTEM STABLE", RGB565(220, 255, 220));
        return;
    }

    flash_on = (((gFrameCounter / 5u) & 1u) == 0u);
    if (!flash_on)
    {
        return;
    }

    color = AiStatusColor(style, sample->ai_status);
    par_lcd_s035_fill_rect(ALERT_X0 + 1, ALERT_Y0 + 1, ALERT_X1 - 1, ALERT_Y1 - 1, RGB565(18, 3, 7));
    DrawLine(ALERT_X0, ALERT_Y0, ALERT_X1, ALERT_Y0, 2, color);
    DrawLine(ALERT_X0, ALERT_Y1, ALERT_X1, ALERT_Y1, 2, color);
    DrawLine(ALERT_X0, ALERT_Y0, ALERT_X0, ALERT_Y1, 2, color);
    DrawLine(ALERT_X1, ALERT_Y0, ALERT_X1, ALERT_Y1, 2, color);

    tx = ALERT_X0 + ((ALERT_X1 - ALERT_X0 + 1) - edgeai_text5x7_width(2, AiStatusText(sample->ai_status))) / 2;
    edgeai_text5x7_draw_scaled(tx, ALERT_Y0 + 8, 2, AiStatusText(sample->ai_status), color);

    BuildFaultReason(sample, detail, sizeof(detail));
    tx = ALERT_X0 + ((ALERT_X1 - ALERT_X0 + 1) - edgeai_text5x7_width(1, detail)) / 2;
    edgeai_text5x7_draw_scaled(tx, ALERT_Y0 + 28, 1, detail, style->palette.text_primary);
}

static void DrawTerminalDynamic(const gauge_style_preset_t *style, const power_sample_t *sample, uint16_t cpu_pct, bool ai_enabled)
{
    char line[32];
    uint16_t ai_color = ai_enabled ? AiStatusColor(style, sample->ai_status) : style->palette.text_secondary;
    const char *status_text = ai_enabled ? AiStatusText(sample->ai_status) : "OFF";
    const char *sys_text = ai_enabled ? AiStatusText(sample->ai_status) : "RAW";

    par_lcd_s035_fill_rect(TERM_X + 6, TERM_Y + 22, TERM_X + TERM_W - 6, TERM_Y + TERM_H - 6, RGB565(6, 9, 12));

    snprintf(line, sizeof(line), "CPU %2u%%", cpu_pct);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 26, 1, line, style->palette.text_primary);

    snprintf(line, sizeof(line), "AI %s", status_text);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 42, 1, line, ai_color);

    snprintf(line, sizeof(line), "SYS %s", sys_text);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 58, 1, line, ai_color);

    if (ai_enabled && (sample->thermal_risk_s > 0u))
    {
        snprintf(line, sizeof(line), "THERM %3us", sample->thermal_risk_s);
        edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 74, 1, line, RGB565(255, 186, 42));
    }
    else
    {
        edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 74, 1, "THERM --", style->palette.text_secondary);
    }

    snprintf(line, sizeof(line), "TEMP %2uC", sample->temp_c);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 90, 1, line, style->palette.text_primary);

    snprintf(line, sizeof(line), "SOC %2u%%", sample->soc_pct);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 106, 1, line, style->palette.text_primary);

    snprintf(line, sizeof(line), "CUR %2u.%01uA", sample->current_mA / 1000u, (sample->current_mA % 1000u) / 100u);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 122, 1, line, style->palette.text_secondary);

    snprintf(line, sizeof(line), "PWR %5uW", sample->power_mW);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 138, 1, line, style->palette.text_secondary);

    snprintf(line, sizeof(line), "DRIFT %2u%%", ai_enabled ? sample->degradation_drift_pct : 0u);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 154, 1, line, ai_color);

    snprintf(line, sizeof(line), "ANOM %2u%%", ai_enabled ? sample->anomaly_score_pct : 0u);
    edgeai_text5x7_draw_scaled(TERM_X + 8, TERM_Y + 170, 1, line, ai_color);
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
    int32_t text_x;
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

    snprintf(line, sizeof(line), "%3u%%", soc);
    text_x = BATT_X + ((BATT_W - edgeai_text5x7_width(1, line)) / 2);
    edgeai_text5x7_draw_scaled(text_x, BATT_Y + 9, 1, line, style->palette.text_primary);
}

static void DrawScopeDynamic(const gauge_style_preset_t *style, bool ai_enabled)
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
    (void)ai_enabled;

    par_lcd_s035_fill_rect(px0, py0, px0 + pw, py0 + ph, RGB565(4, 6, 8));
    DrawLine(px0, baseline_white, px0 + pw, baseline_white, 1, style->palette.text_primary);
    DrawLine(px0, baseline_red, px0 + pw, baseline_red, 1, style->palette.accent_red);

    for (i = 0; i < pw; i++)
    {
        uint8_t idx = (uint8_t)((gTraceHead + i) % (int32_t)(sizeof(gTraceCurrent)));
        int32_t n0 = (int32_t)(NextNoise() & 0x3u) - 1;
        int32_t n1 = (int32_t)(NextNoise() & 0x3u) - 1;
        int32_t n2 = (int32_t)(NextNoise() & 0x3u) - 1;
        uint8_t white = (uint8_t)ClampI32((int32_t)baseline_white - (int32_t)((gTracePower[idx] * (ph / 3)) / 255u) + n0, py0 + 1, py0 + ph - 2);
        uint8_t red = (uint8_t)ClampI32((int32_t)baseline_red - (int32_t)((gTraceVoltage[idx] * (ph / 3)) / 255u) + n1, py0 + 1, py0 + ph - 2);
        uint8_t green = (uint8_t)ClampI32((int32_t)(py0 + ph - 2) - (int32_t)((gTraceCurrent[idx] * (ph - 4)) / 255u) + n2, py0 + 1, py0 + ph - 2);

        if (i > 0)
        {
            DrawLine(px0 + i - 1, prev_white, px0 + i, white, 1, RGB565(230, 230, 230));
            DrawLine(px0 + i - 1, prev_red, px0 + i, red, 1, style->palette.accent_red);
            DrawLine(px0 + i - 1, prev_green, px0 + i, green, 1, style->palette.accent_green);
        }

        prev_white = white;
        prev_red = red;
        prev_green = green;
    }
}

static void DrawLeftBargraphFrame(const gauge_style_preset_t *style)
{
    int32_t i;
    int32_t inner_x0 = BAR_X0 + 2;
    int32_t inner_x1 = BAR_X1 - 2;
    int32_t inner_y0 = BAR_Y0 + 2;
    int32_t inner_y1 = BAR_Y1 - 2;
    int32_t seg_step = (inner_y1 - inner_y0 + 1) / BAR_SEGMENTS;

    par_lcd_s035_fill_rect(BAR_X0, BAR_Y0, BAR_X1, BAR_Y1, RGB565(8, 14, 10));
    par_lcd_s035_fill_rect(inner_x0, inner_y0, inner_x1, inner_y1, RGB565(4, 8, 6));
    DrawLine(BAR_X0, BAR_Y0, BAR_X1, BAR_Y0, 1, style->palette.text_primary);
    DrawLine(BAR_X1, BAR_Y0, BAR_X1, BAR_Y1, 1, style->palette.text_primary);
    DrawLine(BAR_X0, BAR_Y1, BAR_X1, BAR_Y1, 1, style->palette.text_primary);

    for (i = 1; i < BAR_SEGMENTS; i++)
    {
        int32_t y = inner_y1 - (i * seg_step);
        DrawLine(inner_x0, y, inner_x1, y, 1, RGB565(18, 28, 20));
    }

    par_lcd_s035_fill_rect(BAR_X0, 292, 118, 304, RGB565(2, 3, 5));
    edgeai_text5x7_draw_scaled(BAR_X0 + 2, 294, 1, "TEMP: --C", style->palette.text_secondary);
}

static void DrawLeftBargraphDynamic(const gauge_style_preset_t *style, uint8_t temp_c)
{
    int32_t i;
    int32_t level = ClampI32((((int32_t)temp_c - 20) * BAR_SEGMENTS) / 60, 0, BAR_SEGMENTS);
    int32_t inner_x0 = BAR_X0 + 3;
    int32_t inner_x1 = BAR_X1 - 3;
    int32_t inner_y0 = BAR_Y0 + 3;
    int32_t inner_y1 = BAR_Y1 - 3;
    int32_t inner_h = (inner_y1 - inner_y0 + 1);
    int32_t seg_step = inner_h / BAR_SEGMENTS;
    bool over_temp = (temp_c >= 70u);
    char line[16];

    if (((uint8_t)level == gPrevBarLevel) && (temp_c == gPrevBarTemp) && (over_temp == gPrevOverTemp))
    {
        return;
    }

    for (i = 0; i < BAR_SEGMENTS; i++)
    {
        int32_t seg_top = inner_y1 - ((i + 1) * seg_step) + 2;
        int32_t seg_bot = inner_y1 - (i * seg_step) - 1;
        uint16_t color = RGB565(8, 14, 10);

        if (seg_top < inner_y0)
        {
            seg_top = inner_y0;
        }
        if (seg_bot > inner_y1)
        {
            seg_bot = inner_y1;
        }
        if (seg_top > seg_bot)
        {
            continue;
        }

        if (i < level)
        {
            if (over_temp)
            {
                color = style->palette.accent_red;
            }
            else if (i < (BAR_SEGMENTS / 2))
            {
                color = style->palette.accent_green;
            }
            else if (i < ((BAR_SEGMENTS * 4) / 5))
            {
                color = RGB565(175, 150, 24);
            }
            else
            {
                color = style->palette.accent_red;
            }
        }

        par_lcd_s035_fill_rect(inner_x0, seg_top, inner_x1, seg_bot, color);
    }

    par_lcd_s035_fill_rect(BAR_X0, 292, 118, 304, RGB565(2, 3, 5));
    snprintf(line, sizeof(line), "TEMP: %2uC", temp_c);
    edgeai_text5x7_draw_scaled(BAR_X0 + 2, 294, 1, line, over_temp ? style->palette.accent_red : style->palette.text_secondary);

    gPrevBarLevel = (uint8_t)level;
    gPrevBarTemp = temp_c;
    gPrevOverTemp = over_temp;
}

static void DrawStaticDashboard(const gauge_style_preset_t *style)
{
    int32_t brand_x;

    par_lcd_s035_fill(RGB565(0, 0, 0));
    par_lcd_s035_fill_rect(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, RGB565(2, 3, 5));
    par_lcd_s035_fill_rect(PANEL_X0 + 4, PANEL_Y0 + 4, PANEL_X1 - 4, PANEL_Y0 + 30, RGB565(8, 10, 13));
    edgeai_text5x7_draw_scaled(PANEL_X0 + 14, PANEL_Y0 + 14, 1, "(c)RICHARD HABERKERN", style->palette.text_secondary);

    /* Left status bargraph aligned to the existing guide structure. */
    DrawLeftBargraphFrame(style);
    DrawLine(22, 78, 48, 78, 1, style->palette.text_primary);
    DrawLine(48, 78, 48, 258, 1, style->palette.text_primary);
    DrawLine(48, MID_TOP_CY + 54, 72, MID_TOP_CY + 54, 1, style->palette.text_primary);
    DrawLine(48, 258, 48, 288, 1, style->palette.text_primary);
    DrawLine(48, 288, 72, 288, 1, style->palette.text_primary);
    DrawLine(448, 78, 414, 78, 1, style->palette.text_primary);
    DrawLine(414, 78, 414, 258, 1, style->palette.text_primary);
    DrawLine(414, 258, 338, 258, 1, style->palette.text_primary);

    DrawRing(MAIN_CX, MAIN_CY, MAIN_R + 10, 6, RGB565(20, 2, 4), RGB565(6, 2, 3));
    DrawRing(MAIN_CX, MAIN_CY, MAIN_R + 2, 10, RGB565(118, 6, 12), RGB565(26, 2, 5));
    DrawRing(MAIN_CX, MAIN_CY, MAIN_R - 10, 4, RGB565(210, 26, 32), RGB565(8, 8, 10));
    DrawRing(MAIN_CX, MAIN_CY, MAIN_R - 14, 2, style->palette.text_primary, RGB565(138, 112, 10));

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
    DrawChargeClockDynamic(style, 0u);

    DrawBatteryIndicatorFrame(style);
    DrawScopeFrame(style);
    DrawTerminalFrame(style);
    DrawAiPill(style, false);
}

static void ClearDynamicValueBands(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(MAIN_CX - 40, MAIN_CY - 10, MAIN_CX + 40, MAIN_CY + 16, RGB565(138, 112, 10));
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
        gPrevBarLevel = 255u;
        gPrevBarTemp = 255u;
        gPrevOverTemp = false;
        gPrevAnomaly = 0u;
        gPrevWear = 0u;
        gPrevElapsedSim = 0u;
        gPrevAiEnabled = false;
        gPrevAiStatus = 255u;
        gPrevAiFaultFlags = 255u;
        gPrevThermalRisk = 65535u;
        gPrevDrift = 255u;
    }
    return gLcdReady;
}

void GaugeRender_DrawFrame(const power_sample_t *sample, bool ai_enabled)
{
    const gauge_style_preset_t *style;
    int32_t main_idx;
    int32_t left_idx;
    int32_t right_idx;
    uint16_t cpu_pct;
    uint16_t status_color;
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
        gPrevBarLevel = 255u;
        gPrevBarTemp = 255u;
        gPrevOverTemp = false;
        gPrevAnomaly = 0u;
        gPrevWear = 0u;
        gPrevElapsedSim = 0u;
        gPrevAiEnabled = false;
        gPrevAiStatus = 255u;
        gPrevAiFaultFlags = 255u;
        gPrevThermalRisk = 65535u;
        gPrevDrift = 255u;
    }

    main_idx = ClampI32(((int32_t)sample->voltage_mV - 20000) / 420, 0, 12);
    left_idx = ClampI32((int32_t)sample->current_mA / 4000, 0, 10);
    right_idx = ClampI32((int32_t)sample->power_mW / 1000, 0, 10);
    cpu_pct = (uint16_t)(20u + (sample->current_mA / 1400u) + (sample->temp_c / 2u));
    if (cpu_pct > 98u)
    {
        cpu_pct = 98u;
    }
    gFrameCounter++;

    gTraceCurrent[gTraceHead] = ScaleTo8(sample->current_mA, 42000u);
    gTracePower[gTraceHead] = ScaleTo8(sample->power_mW, 10000u);
    gTraceVoltage[gTraceHead] = ScaleTo8((sample->voltage_mV > 20000u) ? (sample->voltage_mV - 20000u) : 0u, 5000u);
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

    DrawScopeDynamic(style, ai_enabled);
    DrawLeftBargraphDynamic(style, sample->temp_c);
    DrawAiAlertOverlay(style, sample, ai_enabled);
    status_color = (ai_enabled ? AiStatusColor(style, sample->ai_status) : style->palette.text_primary);

    if (!gDynamicReady || gPrevCurrent != sample->current_mA || gPrevPower != sample->power_mW ||
        gPrevVoltage != sample->voltage_mV || gPrevSoc != sample->soc_pct)
    {
        ClearDynamicValueBands(style);
    }

    if (!gDynamicReady || gPrevVoltage != sample->voltage_mV)
    {
        int32_t x;
        snprintf(line, sizeof(line), "%3uV", sample->voltage_mV / 100u);
        x = MAIN_CX - (edgeai_text5x7_width(3, line) / 2);
        edgeai_text5x7_draw_scaled(x, MAIN_CY - 8, 3, line, status_color);
    }

    if (!gDynamicReady || gPrevCurrent != sample->current_mA)
    {
        snprintf(line, sizeof(line), "%2u.%01u", sample->current_mA / 1000u, (sample->current_mA % 1000u) / 100u);
        edgeai_text5x7_draw_scaled(MID_TOP_CX - 20, MID_TOP_CY - 8, 2, line, status_color);
    }

    if (!gDynamicReady || gPrevPower != sample->power_mW)
    {
        snprintf(line, sizeof(line), "%4u", sample->power_mW);
        edgeai_text5x7_draw_scaled(MID_BOT_CX - 20, MID_BOT_CY - 8, 2, line, status_color);
    }

    if (!gDynamicReady || gPrevSoc != sample->soc_pct)
    {
        DrawBatteryIndicatorDynamic(style, sample->soc_pct);
    }

    if (!gDynamicReady || gPrevCurrent != sample->current_mA || gPrevPower != sample->power_mW ||
        gPrevVoltage != sample->voltage_mV || gPrevSoc != sample->soc_pct || gPrevTemp != sample->temp_c ||
        gPrevCpuPct != cpu_pct || gPrevAnomaly != sample->anomaly_score_pct || gPrevWear != sample->connector_wear_pct ||
        gPrevAiStatus != sample->ai_status || gPrevAiFaultFlags != sample->ai_fault_flags ||
        gPrevThermalRisk != sample->thermal_risk_s || gPrevDrift != sample->degradation_drift_pct ||
        ((gFrameCounter % 16u) == 0u))
    {
        DrawTerminalDynamic(style, sample, cpu_pct, ai_enabled);
    }

    if ((!gDynamicReady) || ((sample->elapsed_charge_sim_s != gPrevElapsedSim) && ((sample->elapsed_charge_sim_s % 60u) == 0u)))
    {
        DrawChargeClockDynamic(style, sample->elapsed_charge_sim_s);
    }
    if (ai_enabled != gPrevAiEnabled)
    {
        DrawAiPill(style, ai_enabled);
    }

    gPrevCurrent = sample->current_mA;
    gPrevPower = sample->power_mW;
    gPrevVoltage = sample->voltage_mV;
    gPrevSoc = sample->soc_pct;
    gPrevTemp = sample->temp_c;
    gPrevCpuPct = cpu_pct;
    gPrevAnomaly = sample->anomaly_score_pct;
    gPrevWear = sample->connector_wear_pct;
    gPrevAiStatus = sample->ai_status;
    gPrevAiFaultFlags = sample->ai_fault_flags;
    gPrevThermalRisk = sample->thermal_risk_s;
    gPrevDrift = sample->degradation_drift_pct;
    gPrevElapsedSim = sample->elapsed_charge_sim_s;
    gPrevAiEnabled = ai_enabled;
    (void)gTraceReady;
}
