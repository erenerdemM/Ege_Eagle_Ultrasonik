$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path "$PSScriptRoot\.."
$Project = "$RepoRoot\STM32\Ultrasonik_G4_Master"
$Build = "$Project\build-stm32"

$ToolchainBin = "C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin"
$CC = "$ToolchainBin\arm-none-eabi-gcc.exe"
$OBJCOPY = "$ToolchainBin\arm-none-eabi-objcopy.exe"
$SIZE = "$ToolchainBin\arm-none-eabi-size.exe"

If (Test-Path $Build) {
    Remove-Item -Recurse -Force $Build
}
New-Item -ItemType Directory -Path $Build | Out-Null

Write-Host "=== STM32 CLEAN BUILD (PowerShell) ==="
Write-Host "Repository: $RepoRoot"
Write-Host "Project:    $Project"
Write-Host "Build:      $Build"

$IncDirs = @(
    "$Project\Core\Inc",
    "$Project\Drivers\STM32G4xx_HAL_Driver\Inc",
    "$Project\Drivers\STM32G4xx_HAL_Driver\Inc\Legacy",
    "$Project\Drivers\CMSIS\Device\ST\STM32G4xx\Include",
    "$Project\Drivers\CMSIS\Include"
)

$CFlags = @(
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-DDEBUG",
    "-DUSE_HAL_DRIVER",
    "-DSTM32G474xx",
    "-Og",
    "-g3",
    "-ffunction-sections",
    "-fdata-sections"
)

foreach ($inc in $IncDirs) {
    $CFlags += "-I$inc"
}

$ObjFiles = @()

# Compile C source files
$SrcFiles = Get-ChildItem -Path "$Project\Core\Src" -Filter "*.c"
$HalSrcFiles = Get-ChildItem -Path "$Project\Drivers\STM32G4xx_HAL_Driver\Src" -Filter "*.c"

foreach ($file in ($SrcFiles + $HalSrcFiles)) {
    $obj = "$Build\" + $file.BaseName + ".o"
    Write-Host "[CC] $($file.Name)"
    & $CC $CFlags -c $file.FullName -o $obj
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $ObjFiles += $obj
}

# Assemble startup files
$StartupFiles = Get-ChildItem -Path "$Project\Core\Startup" -Filter "*.s"
foreach ($file in $StartupFiles) {
    $obj = "$Build\" + $file.BaseName + ".o"
    Write-Host "[AS] $($file.Name)"
    & $CC $CFlags -x assembler-with-cpp -c $file.FullName -o $obj
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $ObjFiles += $obj
}

# Link ELF
$Elf = "$Build\Ultrasonik_G4_Master.elf"
$Map = "$Build\Ultrasonik_G4_Master.map"
$LdScript = "$Project\STM32G474RETX_FLASH.ld"

$LDFlags = @(
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-T$LdScript",
    "-Wl,--gc-sections",
    "-Wl,-Map=$Map",
    "-Wl,--start-group",
    "-lc",
    "-lm",
    "-Wl,--end-group"
)

Write-Host "[LD] Ultrasonik_G4_Master.elf"
& $CC $LDFlags $ObjFiles -o $Elf
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Generate HEX & BIN
$Hex = "$Build\Ultrasonik_G4_Master.hex"
$Bin = "$Build\Ultrasonik_G4_Master.bin"

Write-Host "[OBJCOPY] .hex"
& $OBJCOPY -O ihex $Elf $Hex

Write-Host "[OBJCOPY] .bin"
& $OBJCOPY -O binary -S $Elf $Bin

Write-Host "=== SIZE ==="
& $SIZE $Elf

Write-Host "SUCCESS: STM32 Firmware compiled successfully."
