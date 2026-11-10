#!/usr/bin/env bash
# Headless build for PSDR_F7 (STM32F746VG) using the CubeIDE-bundled GNU ARM toolchain.
set -o pipefail
TOOLS="C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin"
CC="$TOOLS/arm-none-eabi-gcc.exe"
OBJCOPY="$TOOLS/arm-none-eabi-objcopy.exe"
SIZE="$TOOLS/arm-none-eabi-size.exe"
ROOT="C:/Users/Michael/source/repos/PSDR"
cd "$ROOT" || exit 1
mkdir -p build
"$CC" --version | head -1

CPU="-mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
DEFS="-DSTM32F746xx -DUSE_HAL_DRIVER -DHSE_VALUE=26000000 -DDEBUG -DTRACE -DOS_USE_TRACE_SEMIHOSTING_DEBUG -DOS_INCLUDE_STARTUP_INIT_MULTIPLE_RAM_SECTIONS -DARM_MATH_CM7 -D__FPU_PRESENT=1"
INCS="-I Source/include -I Source/system/include -I Source/system/include/cmsis -I Source/system/include/stm32f7-hal -I Source/usb"
CFLAGS="$CPU $DEFS $INCS -O2 -g -ffunction-sections -fdata-sections -std=gnu11 -Wno-unused -fcommon"
LDFLAGS="$CPU -T mem.ld -T libs.ld -T sections.ld -L Source/ldscripts -nostartfiles -Wl,--gc-sections --specs=nano.specs -Wl,-Map,build/PSDR_F7.map -lm"

SRCS=$(find Source/src Source/system/src Source/DSP_Lib/Source Source/usb -name '*.c' ! -name '*_template.c' ! -name '_ref_*')
echo "source files: $(echo "$SRCS" | wc -l)"

"$CC" $CFLAGS $SRCS $LDFLAGS -o build/PSDR_F7.elf 2> build/build.log
rc=$?
echo "=== gcc exit: $rc ==="
if [ $rc -eq 0 ]; then
  "$SIZE" build/PSDR_F7.elf
  "$OBJCOPY" -O ihex build/PSDR_F7.elf build/PSDR_F7.hex
  "$OBJCOPY" -O binary build/PSDR_F7.elf build/PSDR_F7.bin
  echo "BUILD OK -> build/PSDR_F7.elf/.hex/.bin"
else
  echo "--- first errors ---"; grep -iE 'error:|undefined reference|No such file' build/build.log | head -30
  echo "--- error count ---"; grep -c 'error:' build/build.log
fi
