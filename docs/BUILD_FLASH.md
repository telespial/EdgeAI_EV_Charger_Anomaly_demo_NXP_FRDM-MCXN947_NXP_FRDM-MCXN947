# Build And Flash

## Prerequisites
- Ubuntu bootstrap complete
- LinkServer installed and available on `PATH`

## Setup
```bash
./tools/setup_mcuxsdk_ws.sh
```

If local setup fails on optional middleware sync, use a known-good existing workspace:
```bash
WS_DIR=/home/user/python_projects/codemaster/projects/nxp/frdm-mcxn947/EdgeAI_sphere_demo_NXP_FRDM-MCXN947/mcuxsdk_ws
MCUX_EXAMPLES_DIR="$WS_DIR/mcuxsdk/examples" ./sdk_example/install_mcux_overlay.sh
```

## Build
```bash
./tools/build_frdmmcxn947.sh debug
```

Known-good build command used on 2026-02-15:
```bash
PRJ=/home/user/python_projects/codemaster/projects/nxp/frdm-mcxn947/EdgeAI_EV_Charger_Monitor_demo_NXP_FRDM-MCXN947
cd "$WS_DIR"
EDGEAI_EV_CHARGER_MONITOR_DEMO_ROOT="$PRJ" \
west build -d "$WS_DIR/build_ev_charger_try3" \
  mcuxsdk/examples/demo_apps/edgeai_ev_charger_monitor_demo \
  --toolchain armgcc --config debug -b frdmmcxn947 -Dcore_id=cm33_core0
```

## Flash
```bash
./tools/flash_frdmmcxn947.sh
```

Known-good flash command used on 2026-02-15:
```bash
WS_DIR=/home/user/python_projects/codemaster/projects/nxp/frdm-mcxn947/EdgeAI_sphere_demo_NXP_FRDM-MCXN947/mcuxsdk_ws \
BUILD_DIR=/home/user/python_projects/codemaster/projects/nxp/frdm-mcxn947/EdgeAI_sphere_demo_NXP_FRDM-MCXN947/mcuxsdk_ws/build_ev_charger_try3 \
./tools/flash_frdmmcxn947.sh
```

## Debug UART
```bash
ls -l /dev/serial/by-id
screen /dev/ttyACM0 115200
```

Expected marker:
- `EV Charger Monitor demo baseline booted`
