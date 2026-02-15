/*
 * Pin mux for EdgeAI Sand Demo (FRDM-MCXN947)
 *
 * Focus:
 * - Debug console on FLEXCOMM4
 * - mikroBUS I2C on FLEXCOMM3 (FC3_P0/FC3_P1 on PIO1_0/PIO1_1)
 * - mikroBUS INT is intentionally not configured yet (I2C bringup first)
 */

#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "pin_mux.h"

void BOARD_InitBootPins(void)
{
    BOARD_InitPins();
}

void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_Port0);
    CLOCK_EnableClock(kCLOCK_Port1);
    CLOCK_EnableClock(kCLOCK_Port4);

    // SWO
    const port_pin_config_t port0_2_pinB16_config = {
        .pullSelect = kPORT_PullDisable,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainDisable,
        .driveStrength = kPORT_HighDriveStrength,
        .mux = kPORT_MuxAlt1,
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(PORT0, 2U, &port0_2_pinB16_config);

    // Debug UART (FLEXCOMM4): PIO1_8/PIO1_9 -> FC4_P0/FC4_P1
    const port_pin_config_t port1_8_pinA1_config = {
        .pullSelect = kPORT_PullUp,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainDisable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt2,
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(PORT1, 8U, &port1_8_pinA1_config);

    const port_pin_config_t port1_9_pinB1_config = {
        .pullSelect = kPORT_PullDisable,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainDisable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt2,
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(PORT1, 9U, &port1_9_pinB1_config);

    // mikroBUS I2C on FC3: PIO1_0 = FC3_P0 (SDA), PIO1_1 = FC3_P1 (SCL)
    const port_pin_config_t port1_0_i2c_sda = {
        .pullSelect = kPORT_PullUp,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_SlowSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainEnable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt2, // FC3_P0
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(PORT1, 0U, &port1_0_i2c_sda);

    const port_pin_config_t port1_1_i2c_scl = {
        .pullSelect = kPORT_PullUp,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_SlowSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainEnable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt2, // FC3_P1
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(PORT1, 1U, &port1_1_i2c_scl);

    // Arduino I2C on FC2: PIO4_0 = FC2_P0 (SDA), PIO4_1 = FC2_P1 (SCL)
    const port_pin_config_t port4_0_i2c_sda = {
        .pullSelect = kPORT_PullUp,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_SlowSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainEnable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt2, // FC2_P0
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(PORT4, 0U, &port4_0_i2c_sda);

    const port_pin_config_t port4_1_i2c_scl = {
        .pullSelect = kPORT_PullUp,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_SlowSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainEnable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt2, // FC2_P1
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(PORT4, 1U, &port4_1_i2c_scl);
}

/* LCD shield pins for PAR-LCD-S035 (ST7796S, FlexIO 8080).
 *
 * This is a minimal subset copied from the MCUX SDK FRDM-MCXN947 LVGL example
 * pin mux (BOARD_InitLcdPins) so this project can bring up the same display.
 */
void BOARD_InitLcdPins(void)
{
    /* Enables the clock for GPIO0: Enables clock */
    CLOCK_EnableClock(kCLOCK_Gpio0);
    /* Enables the clock for GPIO4: Enables clock */
    CLOCK_EnableClock(kCLOCK_Gpio4);
    /* Enables the clock for PORT0 controller: Enables clock */
    CLOCK_EnableClock(kCLOCK_Port0);
    /* Enables the clock for PORT2: Enables clock */
    CLOCK_EnableClock(kCLOCK_Port2);
    /* Enables the clock for PORT4: Enables clock */
    CLOCK_EnableClock(kCLOCK_Port4);

    gpio_pin_config_t gpio0_pinC13_config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic  = 0U
    };
    /* Initialize GPIO functionality on pin PIO0_7 (pin C13)  */
    GPIO_PinInit(GPIO0, 7U, &gpio0_pinC13_config);

    gpio_pin_config_t gpio0_pinD11_config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic  = 0U
    };
    /* Initialize GPIO functionality on pin PIO0_12 (pin D11)  */
    GPIO_PinInit(GPIO0, 12U, &gpio0_pinD11_config);

    gpio_pin_config_t gpio0_pinF12_config = {
        .pinDirection = kGPIO_DigitalInput,
        .outputLogic  = 0U
    };
    /* Initialize GPIO functionality on pin PIO0_13 (pin F12)  */
    GPIO_PinInit(GPIO0, 13U, &gpio0_pinF12_config);

    gpio_pin_config_t gpio4_pinN7_config = {
        .pinDirection = kGPIO_DigitalInput,
        .outputLogic  = 0U
    };
    /* Initialize GPIO functionality on pin PIO4_6 (pin N7)  */
    GPIO_PinInit(GPIO4, 6U, &gpio4_pinN7_config);

    gpio_pin_config_t gpio4_pinT4_config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic  = 0U
    };
    /* Initialize GPIO functionality on pin PIO4_7 (pin T4)  */
    GPIO_PinInit(GPIO4, 7U, &gpio4_pinT4_config);

    const port_pin_config_t port0_12_pinD11_config = {/* Internal pull-up/down resistor is disabled */
                                                      kPORT_PullDisable,
                                                      /* Low internal pull resistor value is selected. */
                                                      kPORT_LowPullResistor,
                                                      /* Fast slew rate is configured */
                                                      kPORT_FastSlewRate,
                                                      /* Passive input filter is disabled */
                                                      kPORT_PassiveFilterDisable,
                                                      /* Open drain output is disabled */
                                                      kPORT_OpenDrainDisable,
                                                      /* Low drive strength is configured */
                                                      kPORT_LowDriveStrength,
                                                      /* Pin is configured as PIO0_12 */
                                                      kPORT_MuxAlt0,
                                                      /* Digital input enabled */
                                                      kPORT_InputBufferEnable,
                                                      /* Digital input is not inverted */
                                                      kPORT_InputNormal,
                                                      /* Pin Control Register fields [15:0] are not locked */
                                                      kPORT_UnlockRegister};
    /* PORT0_12 (pin D11) is configured as PIO0_12 */
    PORT_SetPinConfig(PORT0, 12U, &port0_12_pinD11_config);

    const port_pin_config_t port0_13_pinF12_config = {/* Internal pull-up/down resistor is disabled */
                                                      kPORT_PullDisable,
                                                      /* Low internal pull resistor value is selected. */
                                                      kPORT_LowPullResistor,
                                                      /* Fast slew rate is configured */
                                                      kPORT_FastSlewRate,
                                                      /* Passive input filter is disabled */
                                                      kPORT_PassiveFilterDisable,
                                                      /* Open drain output is disabled */
                                                      kPORT_OpenDrainDisable,
                                                      /* Low drive strength is configured */
                                                      kPORT_LowDriveStrength,
                                                      /* Pin is configured as PIO0_13 */
                                                      kPORT_MuxAlt0,
                                                      /* Digital input enabled */
                                                      kPORT_InputBufferEnable,
                                                      /* Digital input is not inverted */
                                                      kPORT_InputNormal,
                                                      /* Pin Control Register fields [15:0] are not locked */
                                                      kPORT_UnlockRegister};
    /* PORT0_13 (pin F12) is configured as PIO0_13 */
    PORT_SetPinConfig(PORT0, 13U, &port0_13_pinF12_config);

    const port_pin_config_t port0_7_pinC13_config = {/* Internal pull-up/down resistor is disabled */
                                                     kPORT_PullDisable,
                                                     /* Low internal pull resistor value is selected. */
                                                     kPORT_LowPullResistor,
                                                     /* Fast slew rate is configured */
                                                     kPORT_FastSlewRate,
                                                     /* Passive input filter is disabled */
                                                     kPORT_PassiveFilterDisable,
                                                     /* Open drain output is disabled */
                                                     kPORT_OpenDrainDisable,
                                                     /* Low drive strength is configured */
                                                     kPORT_LowDriveStrength,
                                                     /* Pin is configured as PIO0_7 */
                                                     kPORT_MuxAlt0,
                                                     /* Digital input enabled */
                                                     kPORT_InputBufferEnable,
                                                     /* Digital input is not inverted */
                                                     kPORT_InputNormal,
                                                     /* Pin Control Register fields [15:0] are not locked */
                                                     kPORT_UnlockRegister};
    /* PORT0_7 (pin C13) is configured as PIO0_7 */
    PORT_SetPinConfig(PORT0, 7U, &port0_7_pinC13_config);

    const port_pin_config_t port0_8_pinC12_config = {/* Internal pull-up resistor is enabled */
                                                     kPORT_PullUp,
                                                     /* Low internal pull resistor value is selected. */
                                                     kPORT_LowPullResistor,
                                                     /* Fast slew rate is configured */
                                                     kPORT_FastSlewRate,
                                                     /* Passive input filter is disabled */
                                                     kPORT_PassiveFilterDisable,
                                                     /* Open drain output is disabled */
                                                     kPORT_OpenDrainDisable,
                                                     /* Low drive strength is configured */
                                                     kPORT_LowDriveStrength,
                                                     /* Pin is configured as FLEXIO0_D0 */
                                                     kPORT_MuxAlt6,
                                                     /* Digital input enabled */
                                                     kPORT_InputBufferEnable,
                                                     /* Digital input is not inverted */
                                                     kPORT_InputNormal,
                                                     /* Pin Control Register fields [15:0] are not locked */
                                                     kPORT_UnlockRegister};
    /* PORT0_8 (pin C12) is configured as FLEXIO0_D0 */
    PORT_SetPinConfig(PORT0, 8U, &port0_8_pinC12_config);

    const port_pin_config_t port0_9_pinA12_config = {/* Internal pull-up resistor is enabled */
                                                     kPORT_PullUp,
                                                     /* Low internal pull resistor value is selected. */
                                                     kPORT_LowPullResistor,
                                                     /* Fast slew rate is configured */
                                                     kPORT_FastSlewRate,
                                                     /* Passive input filter is disabled */
                                                     kPORT_PassiveFilterDisable,
                                                     /* Open drain output is disabled */
                                                     kPORT_OpenDrainDisable,
                                                     /* Low drive strength is configured */
                                                     kPORT_LowDriveStrength,
                                                     /* Pin is configured as FLEXIO0_D1 */
                                                     kPORT_MuxAlt6,
                                                     /* Digital input enabled */
                                                     kPORT_InputBufferEnable,
                                                     /* Digital input is not inverted */
                                                     kPORT_InputNormal,
                                                     /* Pin Control Register fields [15:0] are not locked */
                                                     kPORT_UnlockRegister};
    /* PORT0_9 (pin A12) is configured as FLEXIO0_D1 */
    PORT_SetPinConfig(PORT0, 9U, &port0_9_pinA12_config);

    /* FlexIO data pins D16..D31 live on PORT2 and PORT4; configure the same way as SDK. */
    const port_pin_config_t flexio_data_config = {
        .pullSelect          = kPORT_PullDisable,
        .pullValueSelect     = kPORT_LowPullResistor,
        .slewRate            = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable     = kPORT_OpenDrainDisable,
        .driveStrength       = kPORT_LowDriveStrength,
        .mux                 = kPORT_MuxAlt6,
        .inputBuffer         = kPORT_InputBufferEnable,
        .invertInput         = kPORT_InputNormal,
        .lockRegister        = kPORT_UnlockRegister,
    };

    PORT_SetPinConfig(PORT2, 8U,  &flexio_data_config);  /* PIO2_8  FLEXIO0_D16 */
    PORT_SetPinConfig(PORT2, 9U,  &flexio_data_config);  /* PIO2_9  FLEXIO0_D17 */
    PORT_SetPinConfig(PORT2, 10U, &flexio_data_config);  /* PIO2_10 FLEXIO0_D18 */
    PORT_SetPinConfig(PORT2, 11U, &flexio_data_config);  /* PIO2_11 FLEXIO0_D19 */

    PORT_SetPinConfig(PORT4, 12U, &flexio_data_config);  /* PIO4_12 FLEXIO0_D20 */
    PORT_SetPinConfig(PORT4, 13U, &flexio_data_config);  /* PIO4_13 FLEXIO0_D21 */
    PORT_SetPinConfig(PORT4, 14U, &flexio_data_config);  /* PIO4_14 FLEXIO0_D22 */
    PORT_SetPinConfig(PORT4, 15U, &flexio_data_config);  /* PIO4_15 FLEXIO0_D23 */
    PORT_SetPinConfig(PORT4, 16U, &flexio_data_config);  /* PIO4_16 FLEXIO0_D24 */
    PORT_SetPinConfig(PORT4, 17U, &flexio_data_config);  /* PIO4_17 FLEXIO0_D25 */
    PORT_SetPinConfig(PORT4, 18U, &flexio_data_config);  /* PIO4_18 FLEXIO0_D26 */
    PORT_SetPinConfig(PORT4, 19U, &flexio_data_config);  /* PIO4_19 FLEXIO0_D27 */
    PORT_SetPinConfig(PORT4, 20U, &flexio_data_config);  /* PIO4_20 FLEXIO0_D28 */
    PORT_SetPinConfig(PORT4, 21U, &flexio_data_config);  /* PIO4_21 FLEXIO0_D29 */
    PORT_SetPinConfig(PORT4, 22U, &flexio_data_config);  /* PIO4_22 FLEXIO0_D30 */
    PORT_SetPinConfig(PORT4, 23U, &flexio_data_config);  /* PIO4_23 FLEXIO0_D31 */

    /* Touch/LCD shared INT + RST pins (GPIO) */
    const port_pin_config_t port4_gpio_config = {
        .pullSelect          = kPORT_PullDisable,
        .pullValueSelect     = kPORT_LowPullResistor,
        .slewRate            = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable     = kPORT_OpenDrainDisable,
        .driveStrength       = kPORT_LowDriveStrength,
        .mux                 = kPORT_MuxAlt0,
        .inputBuffer         = kPORT_InputBufferEnable,
        .invertInput         = kPORT_InputNormal,
        .lockRegister        = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(PORT4, 6U, &port4_gpio_config); /* PIO4_6  GPIO (touch INT) */
    PORT_SetPinConfig(PORT4, 7U, &port4_gpio_config); /* PIO4_7  GPIO (LCD RST) */
}
