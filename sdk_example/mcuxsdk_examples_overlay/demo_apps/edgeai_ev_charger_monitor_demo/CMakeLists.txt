cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(edgeai_ev_charger_monitor_demo LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH ${board_root}/${board}/demo_apps/edgeai_ev_charger_monitor_demo)

include(${SdkRootDirPath}/CMakeLists.txt)

if(DEFINED ENV{EDGEAI_EV_CHARGER_MONITOR_DEMO_ROOT})
    set(EDGEAI_ROOT "$ENV{EDGEAI_EV_CHARGER_MONITOR_DEMO_ROOT}")
else()
    get_filename_component(EDGEAI_ROOT "${SdkRootDirPath}/../.." ABSOLUTE)
endif()

mcux_add_source(
    BASE_PATH ${EDGEAI_ROOT}
    SOURCES src/edgeai_ev_charger_monitor_demo.c
            src/gauge_style.c
            src/power_data_source.c
            src/gauge_render.c
            src/par_lcd_s035.c
            src/text5x7.c
)

mcux_add_include(
    BASE_PATH ${EDGEAI_ROOT}
    INCLUDES src
)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES components/touch/gt911/fsl_gt911.c
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES components/touch/gt911
)

include(${SdkRootDirPath}/${board_root}/${board}/demo_apps/edgeai_ev_charger_monitor_demo/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
