/*
 * Hardware init for EdgeAI Sand Demo (FRDM-MCXN947, cm33_core0).
 */

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_clock.h"

void BOARD_InitHardware(void)
{
    // Debug console: FLEXCOMM4
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom4Clk, 1u);
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

    // Arduino I2C: FLEXCOMM2 (GT911 touch on LCD shield)
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom2Clk, 1u);
    CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);

    // mikroBUS I2C: FLEXCOMM3
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom3Clk, 1u);
    CLOCK_AttachClk(kFRO12M_to_FLEXCOMM3);

    /* LCD (FlexIO0, PAR-LCD-S035 shield) */
    CLOCK_SetClkDiv(kCLOCK_DivFlexioClk, 1u);
    CLOCK_AttachClk(kPLL0_to_FLEXIO);

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
}
