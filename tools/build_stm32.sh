#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
PROJECT="$REPO_ROOT/STM32/Ultrasonik_G4_Master"
BUILD="$PROJECT/build-stm32"

CC="${CC:-arm-none-eabi-gcc}"
OBJCOPY="${OBJCOPY:-arm-none-eabi-objcopy}"
SIZE="${SIZE:-arm-none-eabi-size}"

CFLAGS=(
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -DDEBUG
    -DUSE_HAL_DRIVER
    -DSTM32G474xx
    "-I$PROJECT/Core/Inc"
    "-I$PROJECT/Drivers/STM32G4xx_HAL_Driver/Inc"
    "-I$PROJECT/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy"
    "-I$PROJECT/Drivers/CMSIS/Device/ST/STM32G4xx/Include"
    "-I$PROJECT/Drivers/CMSIS/Include"
    -Og
    -g3
    -ffunction-sections
    -fdata-sections
)

LDFLAGS=(
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    "-T$PROJECT/STM32G474RETX_FLASH.ld"
    -Wl,--gc-sections
    "-Wl,-Map=$BUILD/Ultrasonik_G4_Master.map"
)

rm -rf "$BUILD"
mkdir -p "$BUILD"

echo "=== STM32 CLEAN BUILD ==="
echo "Repository: $REPO_ROOT"
echo "Project:    $PROJECT"
echo "Build:      $BUILD"

for src in "$PROJECT"/Core/Src/*.c; do
    name="$(basename "$src" .c)"
    echo "[CC] Core/Src/$name.c"
    "$CC" "${CFLAGS[@]}" -c "$src" -o "$BUILD/$name.o"
done

for src in "$PROJECT"/Core/Startup/*.s; do
    name="$(basename "$src" .s)"
    echo "[AS] Core/Startup/$name.s"
    "$CC" "${CFLAGS[@]}" -x assembler-with-cpp -c "$src" -o "$BUILD/$name.o"
done

for src in "$PROJECT"/Drivers/STM32G4xx_HAL_Driver/Src/*.c; do
    name="$(basename "$src" .c)"
    echo "[HAL CC] $name.c"
    "$CC" "${CFLAGS[@]}" -c "$src" -o "$BUILD/$name.o"
done

echo "[LINK] Ultrasonik_G4_Master.elf"

"$CC" "${LDFLAGS[@]}" \
    "$BUILD"/*.o \
    -o "$BUILD/Ultrasonik_G4_Master.elf"

echo "[BIN] Ultrasonik_G4_Master.bin"
"$OBJCOPY" -O binary \
    "$BUILD/Ultrasonik_G4_Master.elf" \
    "$BUILD/Ultrasonik_G4_Master.bin"

echo "[HEX] Ultrasonik_G4_Master.hex"
"$OBJCOPY" -O ihex \
    "$BUILD/Ultrasonik_G4_Master.elf" \
    "$BUILD/Ultrasonik_G4_Master.hex"

echo
echo "=== MEMORY USAGE ==="
"$SIZE" "$BUILD/Ultrasonik_G4_Master.elf"

echo
echo "=== BUILD SUCCESS ==="
echo "ELF : $BUILD/Ultrasonik_G4_Master.elf"
echo "BIN : $BUILD/Ultrasonik_G4_Master.bin"
echo "HEX : $BUILD/Ultrasonik_G4_Master.hex"
