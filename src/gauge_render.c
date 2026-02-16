#include "gauge_render.h"

#include <stdio.h>
#include <string.h>

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
static uint8_t gTracePower[192];
static uint8_t gTraceVoltage[192];
static uint16_t gTraceCount = 0u;
static bool gTraceReady = false;
static uint32_t gFrameCounter = 0u;
static uint8_t gPrevBarLevel = 255u;
static uint8_t gPrevBarTemp = 255u;
static bool gPrevOverTemp = false;
static bool gPrevAiEnabled = false;
static uint8_t gPrevProfile = 255u;
static uint8_t gPrevAiStatus = 255u;
static uint8_t gPrevAiFaultFlags = 255u;
static uint16_t gPrevThermalRisk = 65535u;
static uint8_t gPrevDrift = 255u;
static uint16_t gAlertWarningHold = 0u;
static bool gAlertVisualValid = false;
static uint8_t gAlertVisualStatus = 255u;
static bool gAlertVisualSevere = false;
static bool gAlertVisualAiEnabled = false;
static char gAlertVisualDetail[30];
static uint32_t gLastScopeSampleSimS = 0u;
static uint8_t gTimelineHour = 3u;
static bool gTimelineTouchLatch = false;
static uint8_t gTimelineHoldTicks = 0u;
static int8_t gTimelineHoldDir = 0;

#define SCOPE_SNAPSHOT_SECONDS 21600u
#define SCOPE_TRACE_POINTS 96u
#define SCOPE_BUCKET_SECONDS (SCOPE_SNAPSHOT_SECONDS / SCOPE_TRACE_POINTS)

#define RGB565(r, g, b) (uint16_t)((((uint16_t)(r) & 0xF8u) << 8) | (((uint16_t)(g) & 0xFCu) << 3) | (((uint16_t)(b) & 0xF8u) >> 3))
#define WARN_YELLOW RGB565(255, 255, 0)
#define ALERT_RED RGB565(255, 0, 0)

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
    TIMELINE_X0 = SCOPE_X + 8,
    TIMELINE_Y0 = 4,
    TIMELINE_X1 = SCOPE_X + SCOPE_W - 8,
    TIMELINE_Y1 = 30,
    TIMELINE_BTN_W = 22,
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

static uint16_t DisplayPowerW(const power_sample_t *sample)
{
    uint32_t p_w = ((uint32_t)sample->voltage_mV * (uint32_t)sample->current_mA) / 100000u;
    if (p_w > 14000u)
    {
        p_w = 14000u;
    }
    return (uint16_t)p_w;
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

static void DrawTextUi(int32_t x, int32_t y, int32_t scale, const char *text, uint16_t fg)
{
    edgeai_text5x7_draw_scaled(x + 1, y + 1, scale, text, RGB565(0, 0, 0));
    edgeai_text5x7_draw_scaled(x, y, scale, text, fg);
}

static void DrawTerminalLine(int32_t y, const char *text, uint16_t color)
{
    par_lcd_s035_fill_rect(TERM_X + 8, y - 1, TERM_X + TERM_W - 8, y + 9, RGB565(6, 9, 12));
    DrawTextUi(TERM_X + 8, y, 1, text, color);
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

static void RefreshMainGaugeCenter(const gauge_style_preset_t *style)
{
    DrawRing(MAIN_CX, MAIN_CY, MAIN_R - 14, 2, style->palette.text_primary, RGB565(138, 112, 10));
    DrawGaugeTicks13(MAIN_CX, MAIN_CY, MAIN_R - 18, style->palette.text_primary, RGB565(170, 170, 176));
}

static void DrawScopeFrame(const gauge_style_preset_t *style)
{
    char line[8];
    int32_t cx;
    int32_t ty;
    int32_t lx0 = TIMELINE_X0;
    int32_t lx1 = TIMELINE_X0 + TIMELINE_BTN_W;
    int32_t rx0 = TIMELINE_X1 - TIMELINE_BTN_W;
    int32_t rx1 = TIMELINE_X1;

    (void)style;
    par_lcd_s035_fill_rect(SCOPE_X, SCOPE_Y, SCOPE_X + SCOPE_W, SCOPE_Y + SCOPE_H, RGB565(18, 3, 7));
    par_lcd_s035_fill_rect(SCOPE_X + 2, SCOPE_Y + 2, SCOPE_X + SCOPE_W - 2, SCOPE_Y + SCOPE_H - 2, RGB565(7, 10, 12));
    par_lcd_s035_fill_rect(TIMELINE_X0 + 1, TIMELINE_Y0 + 1, TIMELINE_X1 - 1, TIMELINE_Y1 - 1, RGB565(20, 28, 34));
    DrawLine(TIMELINE_X0, TIMELINE_Y0, TIMELINE_X1, TIMELINE_Y0, 1, RGB565(130, 130, 136));
    DrawLine(TIMELINE_X0, TIMELINE_Y1, TIMELINE_X1, TIMELINE_Y1, 1, RGB565(130, 130, 136));
    DrawLine(TIMELINE_X0, TIMELINE_Y0, TIMELINE_X0, TIMELINE_Y1, 1, RGB565(130, 130, 136));
    DrawLine(TIMELINE_X1, TIMELINE_Y0, TIMELINE_X1, TIMELINE_Y1, 1, RGB565(130, 130, 136));
    par_lcd_s035_fill_rect(lx0 + 1, TIMELINE_Y0 + 1, lx1 - 1, TIMELINE_Y1 - 1, RGB565(26, 58, 110));
    par_lcd_s035_fill_rect(rx0 + 1, TIMELINE_Y0 + 1, rx1 - 1, TIMELINE_Y1 - 1, RGB565(110, 58, 20));
    DrawLine(lx0, TIMELINE_Y0, lx1, TIMELINE_Y0, 1, RGB565(170, 210, 255));
    DrawLine(lx0, TIMELINE_Y1, lx1, TIMELINE_Y1, 1, RGB565(170, 210, 255));
    DrawLine(rx0, TIMELINE_Y0, rx1, TIMELINE_Y0, 1, RGB565(255, 215, 165));
    DrawLine(rx0, TIMELINE_Y1, rx1, TIMELINE_Y1, 1, RGB565(255, 215, 165));
    ty = TIMELINE_Y0 + ((TIMELINE_Y1 - TIMELINE_Y0 - 7) / 2);
    DrawTextUi(lx0 + 7, ty, 1, "L", RGB565(245, 250, 255));
    DrawTextUi(rx0 + 7, ty, 1, "R", RGB565(255, 248, 236));
    snprintf(line, sizeof(line), "%2uH", (unsigned int)gTimelineHour);
    cx = ((TIMELINE_X0 + TIMELINE_X1) / 2) - (edgeai_text5x7_width(1, line) / 2);
    DrawTextUi(cx, ty, 1, line, RGB565(255, 208, 52));
}

static void DrawAiPill(const gauge_style_preset_t *style, bool ai_enabled)
{
    uint16_t fill = ai_enabled ? RGB565(22, 80, 28) : RGB565(45, 20, 18);
    uint16_t txt = ai_enabled ? RGB565(180, 255, 170) : RGB565(255, 210, 180);
    const char *label = ai_enabled ? "AI  ON" : "AI OFF";
    int32_t lx = AI_PILL_X0 + ((AI_PILL_X1 - AI_PILL_X0 + 1) - edgeai_text5x7_width(1, label)) / 2;
    int32_t ly = AI_PILL_Y0 + (((AI_PILL_Y1 - AI_PILL_Y0 + 1) - 7) / 2);

    par_lcd_s035_fill_rect(AI_PILL_X0, AI_PILL_Y0, AI_PILL_X1, AI_PILL_Y1, fill);
    DrawLine(AI_PILL_X0, AI_PILL_Y0, AI_PILL_X1, AI_PILL_Y0, 1, style->palette.text_primary);
    DrawLine(AI_PILL_X0, AI_PILL_Y1, AI_PILL_X1, AI_PILL_Y1, 1, style->palette.text_primary);
    DrawLine(AI_PILL_X0, AI_PILL_Y0, AI_PILL_X0, AI_PILL_Y1, 1, style->palette.text_primary);
    DrawLine(AI_PILL_X1, AI_PILL_Y0, AI_PILL_X1, AI_PILL_Y1, 1, style->palette.text_primary);
    DrawTextUi(lx, ly, 1, label, txt);
}

static void DrawProfileSwitch(const gauge_style_preset_t *style, power_replay_profile_t profile)
{
    int32_t x0 = GAUGE_RENDER_PROFILE_X0;
    int32_t y0 = GAUGE_RENDER_PROFILE_Y0;
    int32_t x1 = GAUGE_RENDER_PROFILE_X1;
    int32_t y1 = GAUGE_RENDER_PROFILE_Y1;
    int32_t mid = (x0 + x1) / 2;
    bool wired = (profile == POWER_REPLAY_PROFILE_WIRED);
    uint16_t active_fill = RGB565(22, 80, 28);
    uint16_t inactive_fill = RGB565(28, 30, 34);
    uint16_t active_txt = RGB565(190, 255, 180);
    uint16_t inactive_txt = RGB565(175, 182, 190);

    par_lcd_s035_fill_rect(x0, y0, x1, y1, RGB565(10, 12, 16));
    DrawLine(x0, y0, x1, y0, 1, style->palette.text_primary);
    DrawLine(x0, y1, x1, y1, 1, style->palette.text_primary);
    DrawLine(x0, y0, x0, y1, 1, style->palette.text_primary);
    DrawLine(x1, y0, x1, y1, 1, style->palette.text_primary);
    DrawLine(mid, y0 + 1, mid, y1 - 1, 1, RGB565(70, 78, 88));

    par_lcd_s035_fill_rect(x0 + 1, y0 + 1, mid - 1, y1 - 1, wired ? active_fill : inactive_fill);
    par_lcd_s035_fill_rect(mid + 1, y0 + 1, x1 - 1, y1 - 1, wired ? inactive_fill : active_fill);

    DrawTextUi(x0 + 10, y0 + 2, 1, "WIRED", wired ? active_txt : inactive_txt);
    DrawTextUi(mid + 6, y0 + 2, 1, "OUTLET", wired ? inactive_txt : active_txt);
}

static void DrawTerminalFrame(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(TERM_X, TERM_Y, TERM_X + TERM_W, TERM_Y + TERM_H, RGB565(18, 3, 7));
    par_lcd_s035_fill_rect(TERM_X + 2, TERM_Y + 2, TERM_X + TERM_W - 2, TERM_Y + TERM_H - 2, RGB565(6, 9, 12));
    /* Clean any scope-axis seam artifact in the terminal header gutter. */
    par_lcd_s035_fill_rect(TERM_X + 6, TERM_Y + 1, TERM_X + 9, TERM_Y + 5, RGB565(6, 9, 12));
    DrawTextUi(TERM_X + 6, TERM_Y + 6, 1, "STATUS", style->palette.text_primary);
    DrawLine(TERM_X + 6, TERM_Y + 16, TERM_X + TERM_W - 6, TERM_Y + 16, 1, RGB565(110, 20, 30));
}

static void DrawChargeClockDynamic(const gauge_style_preset_t *style, uint32_t elapsed_s)
{
    uint32_t hh = elapsed_s / 3600u;
    uint32_t mm = (elapsed_s % 3600u) / 60u;
    uint32_t ss = elapsed_s % 60u;
    char line[20];
    int32_t label_w = edgeai_text5x7_width(1, "ELAPSED");
    int32_t value_w;
    int32_t box_half_w;
    int32_t label_y = MAIN_CY + 34;
    int32_t label_x;
    int32_t value_x;
    int32_t box_x0;
    int32_t box_x1;

    snprintf(line, sizeof(line), "%02u:%02u:%02u", (unsigned int)hh, (unsigned int)mm, (unsigned int)ss);
    value_w = edgeai_text5x7_width(1, line);
    box_half_w = ((label_w > value_w) ? label_w : value_w) / 2;
    box_x0 = SECTION2_CX - box_half_w - 3;
    box_x1 = SECTION2_CX + box_half_w + 3;

    par_lcd_s035_fill_rect(box_x0, MAIN_CY + 30, box_x1, MAIN_CY + 54, RGB565(138, 112, 10));
    label_x = SECTION2_CX - (label_w / 2);
    DrawTextUi(label_x, label_y, 1, "ELAPSED", style->palette.text_secondary);
    value_x = SECTION2_CX - (value_w / 2);
    DrawTextUi(value_x, label_y + 10, 1, line, style->palette.text_primary);
}

static uint16_t AiStatusColor(const gauge_style_preset_t *style, uint8_t ai_status)
{
    if (ai_status == AI_STATUS_FAULT)
    {
        return style->palette.accent_red;
    }
    if (ai_status == AI_STATUS_WARNING)
    {
        return WARN_YELLOW;
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

static const char *AiDecisionText(uint8_t decision)
{
    switch (decision)
    {
        case AI_DECISION_WATCH: return "WATCH";
        case AI_DECISION_DERATE_15: return "DERATE15";
        case AI_DECISION_DERATE_30: return "DERATE30";
        case AI_DECISION_SHED_LOAD: return "SHEDLOAD";
        default: return "NONE";
    }
}

static uint8_t RuleStatus(const power_sample_t *sample);

static bool IsSevereAlertCondition(const power_sample_t *sample, bool ai_enabled)
{
    if (!ai_enabled)
    {
        return RuleStatus(sample) == AI_STATUS_FAULT;
    }

    if (sample->ai_status == AI_STATUS_FAULT)
    {
        return true;
    }
    if ((sample->thermal_risk_s > 0u) && (sample->thermal_risk_s <= 180u))
    {
        return true;
    }
    if ((sample->predicted_overtemp_s > 0u) && (sample->predicted_overtemp_s <= 120u))
    {
        return true;
    }
    if (sample->ai_decision == AI_DECISION_SHED_LOAD)
    {
        return true;
    }
    if ((sample->temp_c >= 85u) || (sample->power_mW >= 12000u))
    {
        return true;
    }
    return false;
}

static uint8_t RuleStatus(const power_sample_t *sample)
{
    if ((sample->temp_c >= 85u) || (sample->power_mW >= 12000u))
    {
        return AI_STATUS_FAULT;
    }
    if ((sample->temp_c >= 75u) || (sample->power_mW >= 10000u))
    {
        return AI_STATUS_WARNING;
    }
    return AI_STATUS_NORMAL;
}

static void BuildRuleReason(const power_sample_t *sample, char *out, size_t out_len)
{
    if (sample->temp_c >= 85u)
    {
        snprintf(out, out_len, "SHUTDOWN TEMP");
    }
    else if (sample->power_mW >= 12000u)
    {
        snprintf(out, out_len, "OVERCURRENT");
    }
    else if (sample->temp_c >= 75u)
    {
        snprintf(out, out_len, "NEAR OVERTEMP");
    }
    else if (sample->power_mW >= 10000u)
    {
        snprintf(out, out_len, "OVERCURRENT WARN");
    }
    else
    {
        snprintf(out, out_len, "SYSTEM NORMAL");
    }
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
        snprintf(out, out_len, "SYSTEM NORMAL");
    }
}

static void DrawAiAlertOverlay(const gauge_style_preset_t *style, const power_sample_t *sample, bool ai_enabled)
{
    uint16_t color;
    int32_t tx;
    bool severe;
    uint8_t status = sample->ai_status;
    bool strict_normal;
    bool warn_evidence = false;
    char detail[30];

    if (!ai_enabled)
    {
        status = RuleStatus(sample);
    }

    warn_evidence = (status != AI_STATUS_NORMAL) ||
                    (sample->ai_fault_flags != 0u) ||
                    (sample->thermal_risk_s > 0u) ||
                    (sample->predicted_overtemp_s > 0u) ||
                    (sample->anomaly_score_pct >= 30u) ||
                    (sample->ai_decision >= AI_DECISION_WATCH) ||
                    (sample->ai_prevented_events > 0u);

    if (warn_evidence)
    {
        gAlertWarningHold = 240u;
    }
    else if (gAlertWarningHold > 0u)
    {
        gAlertWarningHold--;
    }

    strict_normal = (status == AI_STATUS_NORMAL) &&
                    (sample->ai_fault_flags == 0u) &&
                    (sample->thermal_risk_s == 0u) &&
                    (sample->predicted_overtemp_s == 0u) &&
                    (sample->anomaly_score_pct < 10u) &&
                    (sample->ai_decision == AI_DECISION_NONE) &&
                    (sample->ai_prevented_events == 0u) &&
                    (gAlertWarningHold == 0u);

    if (!strict_normal)
    {
        status = AI_STATUS_WARNING;
    }

    severe = IsSevereAlertCondition(sample, ai_enabled);

    if (status == AI_STATUS_NORMAL)
    {
        snprintf(detail, sizeof(detail), "SYSTEM NORMAL");
    }
    else
    {
        if (ai_enabled)
        {
            BuildFaultReason(sample, detail, sizeof(detail));
        }
        else
        {
            BuildRuleReason(sample, detail, sizeof(detail));
        }
        if ((status != AI_STATUS_NORMAL) && (strcmp(detail, "SYSTEM NORMAL") == 0))
        {
            snprintf(detail, sizeof(detail), "ANALYZING STARTUP");
        }
        if (detail[0] == '\0')
        {
            if (status == AI_STATUS_WARNING)
            {
                snprintf(detail, sizeof(detail), "ANALYZING STARTUP");
            }
            else
            {
                snprintf(detail, sizeof(detail), "CHECK POWER PATH");
            }
        }
    }

    if (gAlertVisualValid &&
        (gAlertVisualStatus == status) &&
        (gAlertVisualSevere == severe) &&
        (gAlertVisualAiEnabled == ai_enabled) &&
        (strcmp(gAlertVisualDetail, detail) == 0))
    {
        return;
    }

    par_lcd_s035_fill_rect(ALERT_X0, ALERT_Y0, ALERT_X1, ALERT_Y1, RGB565(2, 3, 5));
    if (status == AI_STATUS_NORMAL)
    {
        color = style->palette.accent_green;
        par_lcd_s035_fill_rect(ALERT_X0 + 1, ALERT_Y0 + 1, ALERT_X1 - 1, ALERT_Y1 - 1, RGB565(6, 16, 10));
        DrawLine(ALERT_X0, ALERT_Y0, ALERT_X1, ALERT_Y0, 2, color);
        DrawLine(ALERT_X0, ALERT_Y1, ALERT_X1, ALERT_Y1, 2, color);
        DrawLine(ALERT_X0, ALERT_Y0, ALERT_X0, ALERT_Y1, 2, color);
        DrawLine(ALERT_X1, ALERT_Y0, ALERT_X1, ALERT_Y1, 2, color);
        tx = ALERT_X0 + ((ALERT_X1 - ALERT_X0 + 1) - edgeai_text5x7_width(2, "SYSTEM NORMAL")) / 2;
        DrawTextUi(tx, ALERT_Y0 + 12, 2, "SYSTEM NORMAL", RGB565(220, 255, 220));
    }
    else
    {
        color = severe ? ALERT_RED : AiStatusColor(style, status);
        par_lcd_s035_fill_rect(ALERT_X0 + 1, ALERT_Y0 + 1, ALERT_X1 - 1, ALERT_Y1 - 1, RGB565(18, 3, 7));
        DrawLine(ALERT_X0, ALERT_Y0, ALERT_X1, ALERT_Y0, 2, color);
        DrawLine(ALERT_X0, ALERT_Y1, ALERT_X1, ALERT_Y1, 2, color);
        DrawLine(ALERT_X0, ALERT_Y0, ALERT_X0, ALERT_Y1, 2, color);
        DrawLine(ALERT_X1, ALERT_Y0, ALERT_X1, ALERT_Y1, 2, color);

        tx = ALERT_X0 + ((ALERT_X1 - ALERT_X0 + 1) - edgeai_text5x7_width(2, AiStatusText(status))) / 2;
        DrawTextUi(tx, ALERT_Y0 + 8, 2, AiStatusText(status), color);
        tx = ALERT_X0 + ((ALERT_X1 - ALERT_X0 + 1) - edgeai_text5x7_width(1, detail)) / 2;
        DrawTextUi(tx, ALERT_Y0 + 28, 1, detail, style->palette.text_primary);
    }

    gAlertVisualValid = true;
    gAlertVisualStatus = status;
    gAlertVisualSevere = severe;
    gAlertVisualAiEnabled = ai_enabled;
    snprintf(gAlertVisualDetail, sizeof(gAlertVisualDetail), "%s", detail);
}

static void DrawTerminalDynamic(const gauge_style_preset_t *style, const power_sample_t *sample, uint16_t cpu_pct, bool ai_enabled)
{
    char line[32];
    uint16_t power_w = DisplayPowerW(sample);
    uint8_t status = ai_enabled ? sample->ai_status : RuleStatus(sample);
    uint16_t ai_color = ai_enabled ? AiStatusColor(style, sample->ai_status) : AiStatusColor(style, status);
    bool severe = IsSevereAlertCondition(sample, ai_enabled);
    const char *status_text = ai_enabled ? AiStatusText(sample->ai_status) : "OFF";
    const char *sys_text = AiStatusText(status);
    char fault_tag[6];

    /* Refresh header band to eliminate any persistent single-pixel artifacts. */
    par_lcd_s035_fill_rect(TERM_X + 3, TERM_Y + 3, TERM_X + TERM_W - 3, TERM_Y + 16, RGB565(6, 9, 12));
    DrawTextUi(TERM_X + 6, TERM_Y + 6, 1, "STATUS", style->palette.text_primary);
    DrawLine(TERM_X + 6, TERM_Y + 16, TERM_X + TERM_W - 6, TERM_Y + 16, 1, RGB565(110, 20, 30));

    (void)cpu_pct;

    fault_tag[0] = ((sample->ai_fault_flags & AI_FAULT_VOLTAGE_SAG) != 0u) ? 'V' : '-';
    fault_tag[1] = ((sample->ai_fault_flags & AI_FAULT_CURRENT_SPIKE) != 0u) ? 'I' : '-';
    fault_tag[2] = ((sample->ai_fault_flags & AI_FAULT_POWER_UNSTABLE) != 0u) ? 'P' : '-';
    fault_tag[3] = '\0';

    if (severe)
    {
        ai_color = ALERT_RED;
    }

    snprintf(line, sizeof(line), "AI %s %s", status_text, fault_tag);
    DrawTerminalLine(TERM_Y + 26, line, ai_color);

    snprintf(line, sizeof(line), "ACT %s", AiDecisionText(sample->ai_decision));
    DrawTerminalLine(TERM_Y + 42, line, ai_color);

    if (ai_enabled)
    {
        snprintf(line, sizeof(line), "CONF %2u%% PREV %3u", sample->ai_confidence_pct, sample->ai_prevented_events);
        DrawTerminalLine(TERM_Y + 58, line, ai_color);
    }
    else
    {
        snprintf(line, sizeof(line), "RULE %s", sys_text);
        DrawTerminalLine(TERM_Y + 58, line, ai_color);
    }

    if (ai_enabled && (sample->thermal_risk_s > 0u))
    {
        snprintf(line, sizeof(line), "THRM %3us OT %3us", sample->thermal_risk_s, sample->predicted_overtemp_s);
        DrawTerminalLine(TERM_Y + 74, line, severe ? ALERT_RED : WARN_YELLOW);
    }
    else
    {
        snprintf(line, sizeof(line), "THRM --  OT %3us", sample->predicted_overtemp_s);
        DrawTerminalLine(TERM_Y + 74, line, style->palette.text_secondary);
    }

    snprintf(line, sizeof(line), "TEMP %2uC  V %3u", sample->temp_c, sample->voltage_mV / 100u);
    DrawTerminalLine(TERM_Y + 90, line, style->palette.text_primary);

    snprintf(line, sizeof(line), "CUR %u.%01uA", sample->current_mA / 1000u, (sample->current_mA % 1000u) / 100u);
    DrawTerminalLine(TERM_Y + 106, line, style->palette.text_primary);

    snprintf(line, sizeof(line), "PWR %u.%01ukW", power_w / 1000u, (power_w % 1000u) / 100u);
    DrawTerminalLine(TERM_Y + 122, line, style->palette.text_secondary);

    snprintf(line, sizeof(line), "RISK C%2u W%2u T%2u", sample->connector_risk_pct, sample->wire_risk_pct, sample->thermal_damage_risk_pct);
    DrawTerminalLine(TERM_Y + 138, line, style->palette.text_secondary);

    snprintf(line, sizeof(line), "ANOM %2u DRFT %2u", (uint16_t)sample->anomaly_score_pct, sample->degradation_drift_pct);
    DrawTerminalLine(TERM_Y + 154, line, ai_color);
    snprintf(line, sizeof(line), "SOC %2u%%", sample->soc_pct);
    DrawTerminalLine(TERM_Y + 170, line, style->palette.text_secondary);
}

static void DrawBatteryIndicatorFrame(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(BATT_X, BATT_Y, BATT_X + BATT_W, BATT_Y + BATT_H, RGB565(8, 10, 12));
    par_lcd_s035_fill_rect(BATT_X + BATT_W + 1, BATT_Y + 9, BATT_X + BATT_W + 5, BATT_Y + 17, style->palette.text_primary);
    par_lcd_s035_fill_rect(BATT_X + 1, BATT_Y + 1, BATT_X + BATT_W - 1, BATT_Y + BATT_H - 1, RGB565(72, 76, 84));
    DrawTextUi(BATT_X - 26, BATT_Y + 8, 1, "BATT", style->palette.text_secondary);
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

    par_lcd_s035_fill_rect(inner_x0, inner_y0, inner_x0 + inner_w, inner_y0 + inner_h, RGB565(82, 86, 92));
    if (fill > 0)
    {
        par_lcd_s035_fill_rect(inner_x0, inner_y0, inner_x0 + fill, inner_y0 + inner_h, fill_color);
    }

    snprintf(line, sizeof(line), "%3u%%", soc);
    text_x = BATT_X + ((BATT_W - edgeai_text5x7_width(1, line)) / 2);
    DrawTextUi(text_x, BATT_Y + 9, 1, line, style->palette.text_primary);
}

static void DrawScopeDynamic(const gauge_style_preset_t *style, bool ai_enabled)
{
    int32_t px0 = SCOPE_X + 6;
    int32_t py0 = SCOPE_Y + 18;
    int32_t pw = SCOPE_W - 12;
    int32_t ph = SCOPE_H - 24;
    int32_t y_bottom = py0 + ph - 1;
    uint16_t axis_color = RGB565(120, 120, 128);
    uint16_t n = (gTraceCount < SCOPE_TRACE_POINTS) ? gTraceCount : SCOPE_TRACE_POINTS;
    uint16_t max_start = (gTraceCount > SCOPE_TRACE_POINTS) ? (gTraceCount - SCOPE_TRACE_POINTS) : 0u;
    uint16_t start = max_start;
    int32_t prev_x = 0;
    int32_t prev_pow = 0;
    int32_t prev_tmp = 0;
    uint16_t power_color = RGB565(255, 170, 32);
    uint16_t temp_color = RGB565(48, 160, 255);
    uint16_t i;
    (void)ai_enabled;

    par_lcd_s035_fill_rect(px0, py0, px0 + pw, py0 + ph, RGB565(4, 6, 8));
    DrawLine(px0, py0, px0, py0 + ph - 1, 1, axis_color);
    DrawLine(px0, py0 + ph - 1, px0 + pw - 1, py0 + ph - 1, 1, axis_color);

    if (n == 0u)
    {
        return;
    }

    for (i = 0u; i < n; i++)
    {
        int32_t x = px0 + ((int32_t)i * (pw - 1)) / (int32_t)((n > 1u) ? (n - 1u) : 1u);
        uint16_t idx = (uint16_t)(start + i);
        int32_t y_pow = y_bottom - (int32_t)((gTracePower[idx] * (uint32_t)(ph - 4)) / 255u);
        int32_t y_tmp = y_bottom - (int32_t)((gTraceVoltage[idx] * (uint32_t)(ph - 4)) / 255u);

        if (i > 0u)
        {
            DrawLine(prev_x, prev_pow, x, y_pow, 1, power_color);
            DrawLine(prev_x, prev_tmp, x, y_tmp, 1, temp_color);
        }

        prev_x = x;
        prev_pow = y_pow;
        prev_tmp = y_tmp;
    }

    {
        char line[8];
        int32_t cx;
        int32_t ty;
        int32_t lx0 = TIMELINE_X0;
        int32_t lx1 = TIMELINE_X0 + TIMELINE_BTN_W;
        int32_t rx0 = TIMELINE_X1 - TIMELINE_BTN_W;
        int32_t rx1 = TIMELINE_X1;
        par_lcd_s035_fill_rect(TIMELINE_X0 + 1, TIMELINE_Y0 + 1, TIMELINE_X1 - 1, TIMELINE_Y1 - 1, RGB565(20, 28, 34));
        DrawLine(TIMELINE_X0, TIMELINE_Y0, TIMELINE_X1, TIMELINE_Y0, 1, RGB565(130, 130, 136));
        DrawLine(TIMELINE_X0, TIMELINE_Y1, TIMELINE_X1, TIMELINE_Y1, 1, RGB565(130, 130, 136));
        DrawLine(TIMELINE_X0, TIMELINE_Y0, TIMELINE_X0, TIMELINE_Y1, 1, RGB565(130, 130, 136));
        DrawLine(TIMELINE_X1, TIMELINE_Y0, TIMELINE_X1, TIMELINE_Y1, 1, RGB565(130, 130, 136));

        par_lcd_s035_fill_rect(lx0 + 1, TIMELINE_Y0 + 1, lx1 - 1, TIMELINE_Y1 - 1, RGB565(26, 58, 110));
        par_lcd_s035_fill_rect(rx0 + 1, TIMELINE_Y0 + 1, rx1 - 1, TIMELINE_Y1 - 1, RGB565(110, 58, 20));
        DrawLine(lx0, TIMELINE_Y0, lx1, TIMELINE_Y0, 1, RGB565(170, 210, 255));
        DrawLine(lx0, TIMELINE_Y1, lx1, TIMELINE_Y1, 1, RGB565(170, 210, 255));
        DrawLine(rx0, TIMELINE_Y0, rx1, TIMELINE_Y0, 1, RGB565(255, 215, 165));
        DrawLine(rx0, TIMELINE_Y1, rx1, TIMELINE_Y1, 1, RGB565(255, 215, 165));
        ty = TIMELINE_Y0 + ((TIMELINE_Y1 - TIMELINE_Y0 - 7) / 2);
        DrawTextUi(lx0 + 7, ty, 1, "L", RGB565(245, 250, 255));
        DrawTextUi(rx0 + 7, ty, 1, "R", RGB565(255, 248, 236));

        snprintf(line, sizeof(line), "%2uH", (unsigned int)gTimelineHour);
        cx = ((TIMELINE_X0 + TIMELINE_X1) / 2) - (edgeai_text5x7_width(1, line) / 2);
        DrawTextUi(cx, ty, 1, line, RGB565(255, 208, 52));
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
    DrawTextUi(BAR_X0 + 2, 294, 1, "TEMP: --C", style->palette.text_secondary);
}

static void DrawLeftBargraphDynamic(const gauge_style_preset_t *style, uint8_t temp_c)
{
    int32_t i;
    int32_t level = ClampI32(((int32_t)temp_c * BAR_SEGMENTS) / 100, 0, BAR_SEGMENTS);
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
    DrawTextUi(BAR_X0 + 2, 294, 1, line, over_temp ? style->palette.accent_red : style->palette.text_secondary);

    gPrevBarLevel = (uint8_t)level;
    gPrevBarTemp = temp_c;
    gPrevOverTemp = over_temp;
}

static void DrawStaticDashboard(const gauge_style_preset_t *style, power_replay_profile_t profile)
{
    int32_t brand_x;

    par_lcd_s035_fill(RGB565(0, 0, 0));
    par_lcd_s035_fill_rect(PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, RGB565(2, 3, 5));
    par_lcd_s035_fill_rect(PANEL_X0 + 4, PANEL_Y0 + 4, PANEL_X1 - 4, PANEL_Y0 + 30, RGB565(8, 10, 13));
    DrawTextUi(PANEL_X0 + 14, AI_PILL_Y0, 1, "(c)RICHARD HABERKERN", style->palette.text_secondary);
    DrawProfileSwitch(style, profile);

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

    DrawTextUi(MID_TOP_CX - (edgeai_text5x7_width(2, "A") / 2), MID_TOP_CY + 12, 2, "A", style->palette.text_secondary);
    DrawTextUi(MID_BOT_CX - (edgeai_text5x7_width(2, "kW") / 2), MID_BOT_CY + 12, 2, "kW", style->palette.text_secondary);
    DrawTextUi(MID_TOP_CX - 18, MID_TOP_CY + 50, 1, "CURRENT", style->palette.text_primary);
    DrawTextUi(MID_BOT_CX - 14, MID_BOT_CY + 50, 1, "POWER", style->palette.text_primary);
    brand_x = ((PANEL_X0 + PANEL_X1) / 2) - (edgeai_text5x7_width(2, "NXP EDGEAI") / 2);
    DrawTextUi(brand_x, 286, 2, "NXP EDGEAI", RGB565(255, 208, 52));
    DrawChargeClockDynamic(style, 0u);

    DrawBatteryIndicatorFrame(style);
    DrawScopeFrame(style);
    DrawTerminalFrame(style);
    DrawAiPill(style, false);
}

static void ClearMainVoltageBand(void)
{
    int32_t w = edgeai_text5x7_width(3, "000VAC");
    int32_t x0 = MAIN_CX - (w / 2) - 3;
    int32_t x1 = MAIN_CX + (w / 2) + 3;
    int32_t y0 = MAIN_CY + 4;
    int32_t y1 = MAIN_CY + 29;

    par_lcd_s035_fill_rect(x0, y0, x1, y1, RGB565(138, 112, 10));
}

static void ClearCurrentBand(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(MID_TOP_CX - 24, MID_TOP_CY - 10, MID_TOP_CX + 24, MID_TOP_CY + 10, style->palette.panel_black);
}

static void ClearPowerBand(const gauge_style_preset_t *style)
{
    par_lcd_s035_fill_rect(MID_BOT_CX - 24, MID_BOT_CY - 10, MID_BOT_CX + 24, MID_BOT_CY + 10, style->palette.panel_black);
}

bool GaugeRender_Init(void)
{
    const gauge_style_preset_t *style;

    gLcdReady = par_lcd_s035_init();
    if (gLcdReady)
    {
        style = GaugeStyle_GetCockpitPreset();
        DrawStaticDashboard(style, POWER_REPLAY_PROFILE_WIRED);
        gStaticReady = true;
        gDynamicReady = false;
        gPrevMainIdx = -1;
        gPrevLeftIdx = -1;
        gPrevRightIdx = -1;
        gTraceCount = 0u;
        gTraceReady = false;
        gFrameCounter = 0u;
        gPrevBarLevel = 255u;
        gPrevBarTemp = 255u;
        gPrevOverTemp = false;
        gPrevAnomaly = 0u;
        gPrevWear = 0u;
        gPrevElapsedSim = 0u;
        gPrevAiEnabled = false;
        gPrevProfile = 255u;
        gPrevAiStatus = 255u;
        gPrevAiFaultFlags = 255u;
        gPrevThermalRisk = 65535u;
        gPrevDrift = 255u;
        gAlertWarningHold = 0u;
        gAlertVisualValid = false;
        gLastScopeSampleSimS = 0u;
        gTimelineTouchLatch = false;
        gTimelineHoldTicks = 0u;
        gTimelineHoldDir = 0;
        gTimelineHour = 3u;
    }
    return gLcdReady;
}

void GaugeRender_DrawFrame(const power_sample_t *sample, bool ai_enabled, power_replay_profile_t profile)
{
    const gauge_style_preset_t *style;
    int32_t main_idx;
    int32_t left_idx;
    int32_t right_idx;
    uint16_t cpu_pct;
    uint16_t status_color;
    bool scope_advanced = false;
    bool main_face_refreshed = false;
    uint16_t power_w;
    char line[32];

    if (!gLcdReady || sample == 0)
    {
        return;
    }

    style = GaugeStyle_GetCockpitPreset();
    power_w = DisplayPowerW(sample);

    if (!gStaticReady)
    {
        DrawStaticDashboard(style, profile);
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
        gPrevProfile = 255u;
        gPrevAiStatus = 255u;
        gPrevAiFaultFlags = 255u;
        gPrevThermalRisk = 65535u;
        gPrevDrift = 255u;
        gAlertVisualValid = false;
    }

    main_idx = ClampI32(((int32_t)sample->voltage_mV - 20000) / 420, 0, 12);
    left_idx = ClampI32((int32_t)sample->current_mA / 4000, 0, 10);
    right_idx = ClampI32((int32_t)power_w / 1000, 0, 10);
    cpu_pct = (uint16_t)(20u + (sample->current_mA / 1400u) + (sample->temp_c / 2u));
    if (cpu_pct > 98u)
    {
        cpu_pct = 98u;
    }
    gFrameCounter++;

    if ((!gDynamicReady) ||
        (sample->elapsed_charge_sim_s < gLastScopeSampleSimS) ||
        ((sample->elapsed_charge_sim_s - gLastScopeSampleSimS) >= SCOPE_BUCKET_SECONDS))
    {
        uint16_t cap = (uint16_t)sizeof(gTracePower);
        if (sample->elapsed_charge_sim_s < gLastScopeSampleSimS)
        {
            /* Timeline rewind/start-over: clear prior history so selected hour is fresh. */
            gTraceCount = 0u;
            gTraceReady = false;
        }
        if (gTraceCount < cap)
        {
            gTracePower[gTraceCount] = ScaleTo8(power_w, 14000u);
            gTraceVoltage[gTraceCount] = ScaleTo8(sample->temp_c, 100u);
            gTraceCount++;
        }
        else
        {
            uint16_t i;
            for (i = 1u; i < cap; i++)
            {
                gTracePower[i - 1u] = gTracePower[i];
                gTraceVoltage[i - 1u] = gTraceVoltage[i];
            }
            gTracePower[cap - 1u] = ScaleTo8(power_w, 14000u);
            gTraceVoltage[cap - 1u] = ScaleTo8(sample->temp_c, 100u);
            gTraceReady = true;
        }
        gLastScopeSampleSimS = sample->elapsed_charge_sim_s;
        scope_advanced = true;
    }

    if (gDynamicReady)
    {
        if (gPrevMainIdx != main_idx)
        {
            DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 24, gPrevMainIdx, style->palette.panel_black, 3);
            RefreshMainGaugeCenter(style);
            DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 24, main_idx, style->palette.text_primary, 3);
            DrawNeedle13(MAIN_CX, MAIN_CY, MAIN_R - 30, main_idx, style->palette.accent_red, 1);
            gPrevMainIdx = main_idx;
            main_face_refreshed = true;
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

    if (scope_advanced)
    {
        DrawScopeDynamic(style, ai_enabled);
    }
    if (scope_advanced || !gDynamicReady)
    {
        par_lcd_s035_fill_rect(TERM_X, SCOPE_Y + SCOPE_H + 1, TERM_X + TERM_W, TERM_Y - 1, RGB565(2, 3, 5));
        DrawTextUi(SCOPE_X + 8, SCOPE_Y + SCOPE_H + 1, 1, "PWR", RGB565(255, 170, 32));
        DrawTextUi(SCOPE_X + SCOPE_W - 8 - edgeai_text5x7_width(1, "TEMP"), SCOPE_Y + SCOPE_H + 1, 1, "TEMP", RGB565(48, 160, 255));
    }
    DrawLeftBargraphDynamic(style, sample->temp_c);
    DrawAiAlertOverlay(style, sample, ai_enabled);
    status_color = (ai_enabled ? AiStatusColor(style, sample->ai_status) : style->palette.text_primary);

    if (!gDynamicReady || gPrevVoltage != sample->voltage_mV || main_face_refreshed)
    {
        int32_t x;
        snprintf(line, sizeof(line), "%uVAC", sample->voltage_mV / 100u);
        ClearMainVoltageBand();
        x = MAIN_CX - (edgeai_text5x7_width(3, line) / 2);
        DrawTextUi(x, MAIN_CY + 5, 3, line, status_color);
    }

    if (!gDynamicReady || gPrevCurrent != sample->current_mA)
    {
        ClearCurrentBand(style);
        snprintf(line, sizeof(line), "%u.%01u", sample->current_mA / 1000u, (sample->current_mA % 1000u) / 100u);
        DrawTextUi(MID_TOP_CX - 20, MID_TOP_CY - 8, 2, line, status_color);
    }

    if (!gDynamicReady || gPrevPower != power_w)
    {
        int32_t x;
        ClearPowerBand(style);
        snprintf(line, sizeof(line), "%u.%01u", power_w / 1000u, (power_w % 1000u) / 100u);
        x = MID_BOT_CX - (edgeai_text5x7_width(2, line) / 2);
        DrawTextUi(x, MID_BOT_CY - 8, 2, line, status_color);
    }

    if (!gDynamicReady || gPrevSoc != sample->soc_pct)
    {
        DrawBatteryIndicatorDynamic(style, sample->soc_pct);
    }

    if (!gDynamicReady || gPrevCurrent != sample->current_mA || gPrevPower != power_w ||
        gPrevVoltage != sample->voltage_mV || gPrevSoc != sample->soc_pct || gPrevTemp != sample->temp_c ||
        gPrevCpuPct != cpu_pct || gPrevAnomaly != sample->anomaly_score_pct || gPrevWear != sample->connector_wear_pct ||
        gPrevAiStatus != sample->ai_status || gPrevAiFaultFlags != sample->ai_fault_flags ||
        gPrevThermalRisk != sample->thermal_risk_s || gPrevDrift != sample->degradation_drift_pct ||
        ((gFrameCounter % 16u) == 0u))
    {
        DrawTerminalDynamic(style, sample, cpu_pct, ai_enabled);
    }

    if (ai_enabled != gPrevAiEnabled)
    {
        DrawAiPill(style, ai_enabled);
    }
    if (gPrevProfile != (uint8_t)profile)
    {
        DrawProfileSwitch(style, profile);
    }
    /* Redraw elapsed clock only when value changes or gauge-face redraw may have overdrawn it. */
    if ((!gDynamicReady) || (sample->elapsed_charge_s != gPrevElapsedSim) || main_face_refreshed)
    {
        DrawChargeClockDynamic(style, sample->elapsed_charge_s);
    }

    gPrevCurrent = sample->current_mA;
    gPrevPower = power_w;
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
    gPrevElapsedSim = sample->elapsed_charge_s;
    gPrevAiEnabled = ai_enabled;
    gPrevProfile = (uint8_t)profile;
    (void)gTraceReady;
}

bool GaugeRender_HandleTouch(int32_t x, int32_t y, bool pressed)
{
    bool in_timeline = (x >= TIMELINE_X0) && (x <= TIMELINE_X1) && (y >= TIMELINE_Y0) && (y <= TIMELINE_Y1);
    bool in_left = in_timeline && (x <= (TIMELINE_X0 + TIMELINE_BTN_W));
    bool in_right = in_timeline && (x >= (TIMELINE_X1 - TIMELINE_BTN_W));
    bool changed = false;
    int8_t dir = 0;

    if (in_left)
    {
        dir = -1;
    }
    else if (in_right)
    {
        dir = 1;
    }

    if (!pressed || (dir == 0))
    {
        gTimelineTouchLatch = false;
        gTimelineHoldTicks = 0u;
        gTimelineHoldDir = 0;
        return false;
    }

    if (!gTimelineTouchLatch || (dir != gTimelineHoldDir))
    {
        gTimelineTouchLatch = true;
        gTimelineHoldDir = dir;
        gTimelineHoldTicks = 0u;
        if ((dir < 0) && (gTimelineHour > 1u))
        {
            gTimelineHour--;
            changed = true;
        }
        else if ((dir > 0) && (gTimelineHour < 12u))
        {
            gTimelineHour++;
            changed = true;
        }
    }
    else
    {
        if (gTimelineHoldTicks < 255u)
        {
            gTimelineHoldTicks++;
        }

        if (gTimelineHoldTicks >= 8u)
        {
            gTimelineHoldTicks = 6u;
            if ((dir < 0) && (gTimelineHour > 1u))
            {
                gTimelineHour--;
                changed = true;
            }
            else if ((dir > 0) && (gTimelineHour < 12u))
            {
                gTimelineHour++;
                changed = true;
            }
        }
    }

    return changed;
}

uint8_t GaugeRender_GetTimelineHour(void)
{
    return gTimelineHour;
}
