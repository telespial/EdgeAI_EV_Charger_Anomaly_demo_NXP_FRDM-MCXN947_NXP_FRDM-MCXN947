# Build And Flash

## Prerequisites
- MCUX workspace set up (`./tools/setup_mcuxsdk_ws.sh`)
- LinkServer on `PATH`
- FRDM-MCXN947 connected over debug USB

## Recommended Build Directory
After project rename, use a dedicated build dir:
```bash
BUILD_DIR=mcuxsdk_ws/build_anomaly
```

## Build
```bash
BUILD_DIR=mcuxsdk_ws/build_anomaly ./tools/build_frdmmcxn947.sh debug
```

## Flash
```bash
BUILD_DIR=mcuxsdk_ws/build_anomaly ./tools/flash_frdmmcxn947.sh
```

## UART
```bash
ls -l /dev/serial/by-id
screen /dev/ttyACM0 115200
```

## Failsafe Restore
```bash
LinkServer flash --probe '#1' --update-mode none MCXN947:FRDM-MCXN947 \
  load --addr 0x0 failsafe/<golden_binary>.bin
```
