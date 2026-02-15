#include <stdbool.h>
#include <string.h>

#include "app.h"
#include "board.h"
#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_gt911.h"
#include "fsl_gpio.h"
#include "fsl_lpi2c.h"
#include "fsl_port.h"
#include "gauge_render.h"
#include "gauge_style.h"
#include "power_data_source.h"

#define POWER_SAMPLE_PERIOD_US 50000u
#define TOUCH_I2C LPI2C2
#define TOUCH_I2C_FLEXCOMM_INDEX 2u
#define TOUCH_I2C_BAUDRATE_HZ 400000u
#define TOUCH_POINTS 5u
#define TOUCH_INT_GPIO GPIO4
#define TOUCH_INT_PORT PORT4
#define TOUCH_INT_PIN 6u
#define TOUCH_DEBOUNCE_TICKS 4u
#define TOUCH_RELEASE_TICKS 2u
#define TOUCH_HIT_PAD_X 30
#define TOUCH_HIT_PAD_Y 40
#define LCD_WIDTH 480
#define LCD_HEIGHT 320

static gt911_handle_t s_touch_handle;
static bool s_touch_ready = false;

static void TouchDelayMs(uint32_t delay_ms)
{
    SDK_DelayAtLeastUs(delay_ms * 1000u, CLOCK_GetCoreSysClkFreq());
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
    lpi2c_master_config_t i2c_cfg;
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
    const uint32_t i2c_src_hz = CLOCK_GetLPFlexCommClkFreq(TOUCH_I2C_FLEXCOMM_INDEX);

    s_touch_ready = false;
    if (i2c_src_hz == 0u)
    {
        PRINTF("TOUCH init failed: FC%u clock=0\r\n", TOUCH_I2C_FLEXCOMM_INDEX);
        return;
    }

    LPI2C_MasterGetDefaultConfig(&i2c_cfg);
    i2c_cfg.baudRate_Hz = TOUCH_I2C_BAUDRATE_HZ;
    LPI2C_MasterInit(TOUCH_I2C, &i2c_cfg, i2c_src_hz);

    if (GT911_Init(&s_touch_handle, &touch_cfg) == kStatus_Success)
    {
        s_touch_ready = true;
        PRINTF("TOUCH ready: GT911 (%u x %u)\r\n",
               (unsigned int)s_touch_handle.resolutionX,
               (unsigned int)s_touch_handle.resolutionY);
    }
    else
    {
        PRINTF("TOUCH init failed: GT911\r\n");
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

    res_x = (s_touch_handle.resolutionX > 0u) ? (int32_t)s_touch_handle.resolutionX : LCD_WIDTH;
    x = (int32_t)selected->y;
    y = res_x - (int32_t)selected->x;
    if (x < 0)
    {
        x = 0;
    }
    if (x >= LCD_WIDTH)
    {
        x = LCD_WIDTH - 1;
    }
    if (y < 0)
    {
        y = 0;
    }
    if (y >= LCD_HEIGHT)
    {
        y = LCD_HEIGHT - 1;
    }

    *x_out = x;
    *y_out = y;
    return true;
}

static bool TouchInAiPillHitbox(int32_t x, int32_t y)
{
    const int32_t x0 = (int32_t)GAUGE_RENDER_AI_PILL_X0 - TOUCH_HIT_PAD_X;
    const int32_t y0 = (int32_t)GAUGE_RENDER_AI_PILL_Y0 - TOUCH_HIT_PAD_Y;
    const int32_t x1 = (int32_t)GAUGE_RENDER_AI_PILL_X1 + TOUCH_HIT_PAD_X;
    const int32_t y1 = (int32_t)GAUGE_RENDER_AI_PILL_Y1 + TOUCH_HIT_PAD_Y;
    return (x >= x0) && (x <= x1) && (y >= y0) && (y <= y1);
}

static bool TouchInAiPillAnyOrientation(int32_t x, int32_t y)
{
    const int32_t max_x = LCD_WIDTH - 1;
    const int32_t max_y = LCD_HEIGHT - 1;
    const int32_t cands[8][2] = {
        {x, y},
        {y, x},
        {max_x - x, y},
        {x, max_y - y},
        {max_x - x, max_y - y},
        {max_x - y, max_y - x},
        {y, max_y - x},
        {max_x - y, x},
    };
    for (uint32_t i = 0u; i < 8u; i++)
    {
        if (TouchInAiPillHitbox(cands[i][0], cands[i][1]))
        {
            return true;
        }
    }
    return false;
}

int main(void)
{
    uint32_t print_divider = 0u;
    uint32_t touch_debounce = 0u;
    uint32_t touch_release_ticks = 0u;
    bool ai_enabled = true;
    bool ai_touch_armed = true;
    bool touch_int_prev_low = false;
    bool lcd_ok;
    const power_sample_t *s;
    power_sample_t last_drawn_sample;
    bool last_drawn_ai_enabled = true;
    bool has_last = false;
    gpio_pin_config_t input_cfg = {kGPIO_DigitalInput, 1u};

    BOARD_InitHardware();
    GPIO_PinInit(TOUCH_INT_GPIO, TOUCH_INT_PIN, &input_cfg);
    TouchInit();

    PRINTF("EV Charger Monitor demo baseline booted\r\n");
    GaugeStyle_LogPreset();
    PowerData_Init();
    lcd_ok = GaugeRender_Init();
    PRINTF("Gauge render: %s\r\n", lcd_ok ? "ready" : "init_failed");
    PRINTF("Power data source: %s\r\n", PowerData_ModeName());

    s = PowerData_Get();
    if (lcd_ok)
    {
        GaugeRender_DrawFrame(s, ai_enabled);
        last_drawn_sample = *s;
        last_drawn_ai_enabled = ai_enabled;
        has_last = true;
    }

    for (;;)
    {
        bool touch_down;
        bool touch_int_low;
        bool in_ai_pill = false;
        int32_t tx = 0;
        int32_t ty = 0;

        PowerData_Tick();
        s = PowerData_Get();

        touch_down = TouchGetPoint(&tx, &ty);
        touch_int_low = (GPIO_PinRead(TOUCH_INT_GPIO, TOUCH_INT_PIN) == 0u);
        if (touch_down)
        {
            in_ai_pill = TouchInAiPillAnyOrientation(tx, ty);
            touch_release_ticks = 0u;
        }
        else if (touch_int_low && !touch_int_prev_low)
        {
            /* Fallback: use touch INT falling edge as a single tap event. */
            touch_down = true;
            in_ai_pill = true;
            touch_release_ticks = 0u;
        }
        touch_int_prev_low = touch_int_low;

        if ((touch_debounce == 0u) && ai_touch_armed && in_ai_pill)
        {
            ai_enabled = !ai_enabled;
            touch_debounce = TOUCH_DEBOUNCE_TICKS;
            ai_touch_armed = false;
            PRINTF("AI_TOGGLE,%s,src=touch_gt911\r\n", ai_enabled ? "ON" : "OFF");
        }

        if (!touch_down)
        {
            if (touch_release_ticks < TOUCH_RELEASE_TICKS)
            {
                touch_release_ticks++;
            }
            if (touch_release_ticks >= TOUCH_RELEASE_TICKS)
            {
                ai_touch_armed = true;
            }
        }

        if (touch_debounce > 0u)
        {
            touch_debounce--;
        }

        if (lcd_ok &&
            (!has_last || memcmp(s, &last_drawn_sample, sizeof(last_drawn_sample)) != 0 || (ai_enabled != last_drawn_ai_enabled)))
        {
            GaugeRender_DrawFrame(s, ai_enabled);
            last_drawn_sample = *s;
            last_drawn_ai_enabled = ai_enabled;
            has_last = true;
        }

        print_divider++;
        if (print_divider >= 20u)
        {
            print_divider = 0u;
            PRINTF("SAMPLE,mA=%u,mW=%u,mV=%u,SOC=%u,T=%u,ANOM=%u,WEAR=%u,ST=%u,TRISK=%u,sim_s=%u,mode=%s\r\n",
                   s->current_mA,
                   s->power_mW,
                   s->voltage_mV,
                   s->soc_pct,
                   s->temp_c,
                   s->anomaly_score_pct,
                   s->connector_wear_pct,
                   s->ai_status,
                   s->thermal_risk_s,
                   (unsigned int)s->elapsed_charge_sim_s,
                   PowerData_ModeName());
        }

        SDK_DelayAtLeastUs(POWER_SAMPLE_PERIOD_US, CLOCK_GetCoreSysClkFreq());
    }
}
