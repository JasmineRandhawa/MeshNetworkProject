@echo off
cd /D C:\Fall2020\Projects\7570\MeshNetworkProject\zephyrproject\zephyr\build\zephyr\kconfig || (set FAIL_LINE=2& goto :ABORT)
"C:\Program Files\CMake\bin\cmake.exe" -E env ZEPHYR_BASE=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb PYTHON_EXECUTABLE=C:/Python38/python.exe srctree=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr KERNELVERSION=0x2046300 KCONFIG_CONFIG=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/build/zephyr/.config ARCH=arm ARCH_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/arch BOARD_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/boards/arm/reel_board SHIELD_AS_LIST= KCONFIG_BINARY_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/build/Kconfig TOOLCHAIN_KCONFIG_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/cmake/toolchain/gnuarmemb EDT_PICKLE=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/build/zephyr/edt.pickle ZEPHYR_CMSIS_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/cmsis ZEPHYR_ATMEL_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/atmel ZEPHYR_ALTERA_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/altera ZEPHYR_CANOPENNODE_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/lib/canopennode ZEPHYR_CIVETWEB_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/lib/civetweb ZEPHYR_ESP-IDF_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/esp-idf ZEPHYR_FATFS_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/fs/fatfs ZEPHYR_CYPRESS_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/cypress ZEPHYR_INFINEON_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/infineon ZEPHYR_NORDIC_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/nordic ZEPHYR_OPENISA_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/openisa ZEPHYR_NUVOTON_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/nuvoton ZEPHYR_MICROCHIP_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/microchip ZEPHYR_SILABS_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/silabs ZEPHYR_ST_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/st ZEPHYR_STM32_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/stm32 ZEPHYR_TI_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/ti ZEPHYR_LIBMETAL_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/libmetal ZEPHYR_QUICKLOGIC_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/quicklogic ZEPHYR_LVGL_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/lib/gui/lvgl ZEPHYR_MBEDTLS_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/crypto/mbedtls ZEPHYR_MCUBOOT_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/bootloader/mcuboot ZEPHYR_MCUMGR_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/lib/mcumgr ZEPHYR_NXP_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/nxp ZEPHYR_OPEN-AMP_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/lib/open-amp ZEPHYR_LORAMAC-NODE_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/lib/loramac-node ZEPHYR_OPENTHREAD_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/lib/openthread ZEPHYR_SEGGER_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/debug/segger ZEPHYR_TINYCBOR_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/lib/tinycbor ZEPHYR_TINYCRYPT_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/crypto/tinycrypt ZEPHYR_LITTLEFS_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/fs/littlefs ZEPHYR_MIPI-SYS-T_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/debug/mipi-sys-t ZEPHYR_NRF_HW_MODELS_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/bsim_hw_models/nrf_hw_models ZEPHYR_XTENSA_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/hal/xtensa ZEPHYR_TFM_MODULE_DIR=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/modules/tee/tfm EXTRA_DTC_FLAGS= DTS_POST_CPP=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/build/zephyr/reel_board_v2.dts.pre.tmp DTS_ROOT_BINDINGS=C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/dts/bindings C:/Python38/python.exe C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/scripts/kconfig/menuconfig.py C:/Fall2020/Projects/7570/MeshNetworkProject/zephyrproject/zephyr/Kconfig || (set FAIL_LINE=3& goto :ABORT)
goto :EOF

:ABORT
set ERROR_CODE=%ERRORLEVEL%
echo Batch file failed at line %FAIL_LINE% with errorcode %ERRORLEVEL%
exit /b %ERROR_CODE%