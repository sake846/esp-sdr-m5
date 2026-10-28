# Windows development setup

ESP-SDR pins ESP-IDF in `firmware-targets.json`. For the ESP32 and ESP32-S3 profiles, use commit `25fe69f946311abdaf9ad56591f25fedbc20ac98`.

## Install ESP-IDF

Install Python 3.12, then clone ESP-IDF and install the tools for both targets:

```powershell
git clone https://github.com/espressif/esp-idf.git "$env:USERPROFILE\esp\esp-idf-esp-sdr"
Set-Location "$env:USERPROFILE\esp\esp-idf-esp-sdr"
git checkout 25fe69f946311abdaf9ad56591f25fedbc20ac98
git submodule update --init
.\install.ps1 'esp32,esp32s3'
```

## Activate the environment

Run these commands in each PowerShell session. Adjust the Python and ESP-IDF paths if they were installed elsewhere.

```powershell
$pyDir = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312'
$env:PATH = "$pyDir;$env:PATH"
$env:PYTHONUTF8 = '1'
. "$env:USERPROFILE\esp\esp-idf-esp-sdr\export.ps1"
```

## Build both profiles

From the ESP-SDR repository root:

```powershell
idf.py -B build-esp32 -DIDF_TARGET=esp32 -DSDKCONFIG=build-esp32/sdkconfig '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32' build
idf.py -B build-esp32s3 -DIDF_TARGET=esp32s3 -DSDKCONFIG=build-esp32s3/sdkconfig '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32s3' build
```

The firmware images are written to `build-esp32/esp_sdr.bin` and `build-esp32s3/esp_sdr.bin`.

## Basic resets into normal boot instead of download mode

If `idf.py flash` reports `Wrong boot mode detected (0x17)` on a Windows
USB-UART connection, while the browser flasher connects successfully, use the
project's reset wrapper. It changes DTR and RTS together through the Windows
serial driver. It does not modify the installed SDK or erase the whole flash.

After activating ESP-IDF, run from the repository root:

```powershell
$env:ESPTOOL_WRAPPER = (Resolve-Path .\tools\esptool_windows.py).Path
idf.py -B build-m5stack-basic reconfigure
idf.py -B build-m5stack-basic -p COM3 flash
```

Close the browser's serial connection and any serial monitors before flashing.
Set `ESPTOOL_WRAPPER` in each new shell before reconfiguring the build. To return
to the SDK's default reset, remove the variable and reconfigure:

```powershell
Remove-Item Env:ESPTOOL_WRAPPER
idf.py -B build-m5stack-basic reconfigure
```
