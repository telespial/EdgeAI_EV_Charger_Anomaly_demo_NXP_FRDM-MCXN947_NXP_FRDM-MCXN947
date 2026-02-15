#include <stdbool.h>

#include "board.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "gauge_style.h"
#include "gauge_render.h"
#include "power_data_source.h"

int main(void)
{
    uint32_t print_divider = 0u;
    bool lcd_ok;

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    PRINTF("EV Charger Monitor demo baseline booted\r\n");
    GaugeStyle_LogPreset();
    PowerData_Init();
    lcd_ok = GaugeRender_Init();
    PRINTF("Gauge render: %s\r\n", lcd_ok ? "ready" : "init_failed");
    PRINTF("Power data source: %s\r\n", PowerData_ModeName());

    for (;;)
    {
        const power_sample_t *s;

        PowerData_Tick();

        print_divider++;
        if (print_divider < 120000u)
        {
            continue;
        }
        print_divider = 0u;

        s = PowerData_Get();
        if (lcd_ok)
        {
            GaugeRender_DrawFrame(s);
        }
        PRINTF("SAMPLE,mA=%u,mW=%u,mV=%u,SOC=%u,T=%u,mode=%s\r\n",
               s->current_mA,
               s->power_mW,
               s->voltage_mV,
               s->soc_pct,
               s->temp_c,
               PowerData_ModeName());
    }
}
