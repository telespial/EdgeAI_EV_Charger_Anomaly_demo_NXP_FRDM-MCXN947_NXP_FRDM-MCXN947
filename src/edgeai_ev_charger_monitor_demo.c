#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "board.h"
#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_gt911.h"
#include "fsl_lpi2c.h"
#include "fsl_port.h"
#include "gauge_render.h"
#include "power_data_source.h"

#define TOUCH_I2C LPI2C2
#define TOUCH_I2C_FLEXCOMM_INDEX 2u
#define TOUCH_POINTS 5u
#define TOUCH_INT_PORT PORT4
#define TOUCH_INT_PIN 6u

#define TOUCH_POLL_DELAY_US 10000u
#define POWER_TICK_PERIOD_US 50000u
#define DISPLAY_REFRESH_PERIOD_US 500000u

static gt911_handle_t s_touch_handle;
static bool s_touch_ready = false;
static bool s_touch_was_down = false;
static bool s_touch_i2c_inited = false;

static void TouchDelayMs(uint32_t delay_ms)
{
    SDK_DelayAtLeastUs(delay_ms * 1000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
}

static bool TouchI2CInit(void)
{
    uint32_t src_hz;
    lpi2c_master_config_t cfg;

    if (s_touch_i2c_inited)
    {
        return true;
    }

    CLOCK_SetClkDiv(kCLOCK_DivFlexcom2Clk, 1u);
    CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);

    src_hz = CLOCK_GetLPFlexCommClkFreq(TOUCH_I2C_FLEXCOMM_INDEX);
    if (src_hz == 0u)
    {
        PRINTF("TOUCH i2c init failed: FC2 clock=0\r\n");
        return false;
    }

    LPI2C_MasterGetDefaultConfig(&cfg);
    cfg.baudRate_Hz = 400000u;
    LPI2C_MasterInit(TOUCH_I2C, &cfg, src_hz);
    s_touch_i2c_inited = true;
    return true;
}

static status_t TouchI2CSend(uint8_t deviceAddress,
                             uint32_t subAddress,
                             uint8_t subaddressSize,
                             const uint8_t *txBuff,
                             uint8_t txBuffSize)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.flags = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress = deviceAddress;
    xfer.direction = kLPI2C_Write;
    xfer.subaddress = subAddress;
    xfer.subaddressSize = subaddressSize;
    xfer.data = (uint8_t *)(uintptr_t)txBuff;
    xfer.dataSize = txBuffSize;
    return LPI2C_MasterTransferBlocking(TOUCH_I2C, &xfer);
}

static status_t TouchI2CReceive(uint8_t deviceAddress,
                                uint32_t subAddress,
                                uint8_t subaddressSize,
                                uint8_t *rxBuff,
                                uint8_t rxBuffSize)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.flags = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress = deviceAddress;
    xfer.direction = kLPI2C_Read;
    xfer.subaddress = subAddress;
    xfer.subaddressSize = subaddressSize;
    xfer.data = rxBuff;
    xfer.dataSize = rxBuffSize;
    return LPI2C_MasterTransferBlocking(TOUCH_I2C, &xfer);
}

static void TouchConfigIntPin(gt911_int_pin_mode_t mode)
{
    CLOCK_EnableClock(kCLOCK_Port4);

    port_pin_config_t cfg = {
        .pullSelect = kPORT_PullDown,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainDisable,
        .driveStrength = kPORT_LowDriveStrength,
#if defined(FSL_FEATURE_PORT_HAS_DRIVE_STRENGTH1) && FSL_FEATURE_PORT_HAS_DRIVE_STRENGTH1
        .driveStrength1 = kPORT_NormalDriveStrength,
#endif
        .mux = kPORT_MuxAlt0,
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };

    switch (mode)
    {
        case kGT911_IntPinPullUp:
            cfg.pullSelect = kPORT_PullUp;
            break;
        case kGT911_IntPinPullDown:
            cfg.pullSelect = kPORT_PullDown;
            break;
        case kGT911_IntPinInput:
            cfg.pullSelect = kPORT_PullDisable;
            break;
        default:
            break;
    }

    PORT_SetPinConfig(TOUCH_INT_PORT, TOUCH_INT_PIN, &cfg);
}

static void TouchConfigResetPin(bool pullUp)
{
    (void)pullUp;
}

static void TouchInit(void)
{
    gt911_config_t touch_cfg = {
        .I2C_SendFunc = TouchI2CSend,
        .I2C_ReceiveFunc = TouchI2CReceive,
        .timeDelayMsFunc = TouchDelayMs,
        .intPinFunc = TouchConfigIntPin,
        .pullResetPinFunc = TouchConfigResetPin,
        .touchPointNum = TOUCH_POINTS,
        .i2cAddrMode = kGT911_I2cAddrAny,
        .intTrigMode = kGT911_IntFallingEdge,
    };
    status_t st;

    s_touch_ready = false;
    s_touch_was_down = false;

    if (!TouchI2CInit())
    {
        return;
    }

    st = GT911_Init(&s_touch_handle, &touch_cfg);
    if (st == kStatus_Success)
    {
        s_touch_ready = true;
        PRINTF("TOUCH ready: GT911 (%u x %u)\r\n",
               (unsigned int)s_touch_handle.resolutionX,
               (unsigned int)s_touch_handle.resolutionY);
    }
    else
    {
        PRINTF("TOUCH init failed: GT911 status=%d\r\n", (int)st);
    }
}

static bool TouchGetPoint(int32_t *x_out, int32_t *y_out)
{
    touch_point_t points[TOUCH_POINTS];
    uint8_t point_count = TOUCH_POINTS;
    const touch_point_t *selected = NULL;
    int32_t x;
    int32_t y;
    int32_t res_x;

    if (!s_touch_ready || (x_out == NULL) || (y_out == NULL))
    {
        return false;
    }

    if (GT911_GetMultiTouch(&s_touch_handle, &point_count, points) != kStatus_Success)
    {
        return false;
    }

    for (uint8_t i = 0u; i < point_count; i++)
    {
        if (points[i].valid && (points[i].touchID == 0u))
        {
            selected = &points[i];
            break;
        }
    }

    if (selected == NULL)
    {
        for (uint8_t i = 0u; i < point_count; i++)
        {
            if (points[i].valid)
            {
                selected = &points[i];
                break;
            }
        }
    }

    if (selected == NULL)
    {
        return false;
    }

    res_x = (s_touch_handle.resolutionX > 0u) ? (int32_t)s_touch_handle.resolutionX : 480;
    x = (int32_t)selected->y;
    y = res_x - (int32_t)selected->x;

    if (x < 0) x = 0;
    if (x > 479) x = 479;
    if (y < 0) y = 0;
    if (y > 319) y = 319;

    *x_out = x;
    *y_out = y;
    return true;
}

static bool TouchInAiPill(int32_t x, int32_t y)
{
    return (x >= GAUGE_RENDER_AI_PILL_X0) && (x <= GAUGE_RENDER_AI_PILL_X1) &&
           (y >= GAUGE_RENDER_AI_PILL_Y0) && (y <= GAUGE_RENDER_AI_PILL_Y1);
}

static bool TouchInProfileSwitch(int32_t x, int32_t y)
{
    return (x >= GAUGE_RENDER_PROFILE_X0) && (x <= GAUGE_RENDER_PROFILE_X1) &&
           (y >= GAUGE_RENDER_PROFILE_Y0) && (y <= GAUGE_RENDER_PROFILE_Y1);
}

static bool TimingCounterInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    return (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u;
}

int main(void)
{
    bool ai_enabled = true;
    bool lcd_ok;
    uint64_t data_tick_accum_cycles = 0u;
    uint64_t render_tick_accum_cycles = 0u;
    uint32_t data_tick_period_cycles = 0u;
    uint32_t render_tick_period_cycles = 0u;
    uint32_t last_cycle = 0u;
    bool use_cycle_timing = false;
    const power_sample_t *sample;

    BOARD_InitHardware();

    lcd_ok = GaugeRender_Init();
    PRINTF("EV dash LCD: %s\r\n", lcd_ok ? "ready" : "init_failed");

    PowerData_Init();
    PowerData_SetAiAssistEnabled(ai_enabled);
    TouchInit();
    PowerData_SetReplayHour(GaugeRender_GetTimelineHour());
    {
        uint32_t core_hz = CLOCK_GetCoreSysClkFreq();
        use_cycle_timing = TimingCounterInit();
        if (core_hz != 0u)
        {
            data_tick_period_cycles = (uint32_t)(((uint64_t)core_hz * POWER_TICK_PERIOD_US) / 1000000ull);
            render_tick_period_cycles = (uint32_t)(((uint64_t)core_hz * DISPLAY_REFRESH_PERIOD_US) / 1000000ull);
        }
        if ((data_tick_period_cycles == 0u) || (render_tick_period_cycles == 0u))
        {
            use_cycle_timing = false;
        }
        last_cycle = DWT->CYCCNT;
    }

    sample = PowerData_Get();
    if (lcd_ok && (sample != NULL))
    {
        GaugeRender_DrawFrame(sample, ai_enabled, PowerData_GetReplayProfile());
    }

    PRINTF("EV dash app ready\r\n");

    for (;;)
    {
        if (use_cycle_timing)
        {
            uint32_t now_cycle = DWT->CYCCNT;
            uint32_t delta_cycles = now_cycle - last_cycle;
            last_cycle = now_cycle;
            data_tick_accum_cycles += delta_cycles;
            render_tick_accum_cycles += delta_cycles;
        }
        int32_t tx = 0;
        int32_t ty = 0;
        bool pressed = TouchGetPoint(&tx, &ty);
        bool in_pill = pressed && TouchInAiPill(tx, ty);
        bool in_profile = pressed && TouchInProfileSwitch(tx, ty);
        bool timeline_changed = GaugeRender_HandleTouch(tx, ty, pressed);

        if (in_pill && !s_touch_was_down)
        {
            ai_enabled = !ai_enabled;
            PowerData_SetAiAssistEnabled(ai_enabled);
            PRINTF("AI_TOGGLE,%s\r\n", ai_enabled ? "ON" : "OFF");
            if (lcd_ok)
            {
                GaugeRender_DrawFrame(PowerData_Get(), ai_enabled, PowerData_GetReplayProfile());
            }
        }
        if (in_profile && !s_touch_was_down)
        {
            int32_t mid = (GAUGE_RENDER_PROFILE_X0 + GAUGE_RENDER_PROFILE_X1) / 2;
            power_replay_profile_t profile = (tx <= mid) ? POWER_REPLAY_PROFILE_WIRED : POWER_REPLAY_PROFILE_OUTLET;
            PowerData_SetReplayProfile(profile);
            PRINTF("PROFILE,%s\r\n", (profile == POWER_REPLAY_PROFILE_WIRED) ? "WIRED" : "OUTLET");
            if (lcd_ok)
            {
                GaugeRender_DrawFrame(PowerData_Get(), ai_enabled, profile);
            }
        }
        if (timeline_changed && lcd_ok)
        {
            PowerData_SetReplayHour(GaugeRender_GetTimelineHour());
            GaugeRender_DrawFrame(PowerData_Get(), ai_enabled, PowerData_GetReplayProfile());
        }
        s_touch_was_down = pressed;

        if (use_cycle_timing)
        {
            while (data_tick_accum_cycles >= data_tick_period_cycles)
            {
                data_tick_accum_cycles -= data_tick_period_cycles;
                PowerData_Tick();
            }

            if (lcd_ok && (render_tick_accum_cycles >= render_tick_period_cycles))
            {
                render_tick_accum_cycles -= render_tick_period_cycles;
                GaugeRender_DrawFrame(PowerData_Get(), ai_enabled, PowerData_GetReplayProfile());
            }
        }
        else
        {
            static uint32_t data_tick_accum_us = 0u;
            static uint32_t render_tick_accum_us = 0u;
            data_tick_accum_us += TOUCH_POLL_DELAY_US;
            render_tick_accum_us += TOUCH_POLL_DELAY_US;

            if (data_tick_accum_us >= POWER_TICK_PERIOD_US)
            {
                data_tick_accum_us = 0u;
                PowerData_Tick();
            }

            if (lcd_ok && (render_tick_accum_us >= DISPLAY_REFRESH_PERIOD_US))
            {
                render_tick_accum_us = 0u;
                GaugeRender_DrawFrame(PowerData_Get(), ai_enabled, PowerData_GetReplayProfile());
            }
        }

        SDK_DelayAtLeastUs(TOUCH_POLL_DELAY_US, CLOCK_GetCoreSysClkFreq());
    }
}
