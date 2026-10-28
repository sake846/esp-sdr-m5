# Build setup

Use this guide once to prepare the repository and ESP-IDF. Then use the
matching device guide for its build and flash commands.

## 1. Clone the repository

Install Git and Python, then clone the repository with its ESP-DSP, M5Unified
and M5GFX submodules:

~~~sh
git clone --recursive https://github.com/sake846/esp-sdr-m5.git
cd esp-sdr-m5
~~~

If the repository was cloned without submodules, initialize them from its root:

~~~sh
git submodule update --init --recursive
~~~

## 2. Install the pinned ESP-IDF

The M5Stack, LILYGO and ATS-mini profiles currently use ESP-IDF commit
`25fe69f946311abdaf9ad56591f25fedbc20ac98`. The revision listed for the selected
profile in [firmware-targets.json](../firmware-targets.json) is authoritative.
Install the host prerequisites for your operating system first. On Windows,
use Python 3.12 and ensure Git and Python are on PATH; see the
[Windows development setup](windows-development-setup.md).

The commands below install ESP-IDF beside this repository and return to the
repository root. Start in the `esp-sdr-m5` directory.

**sh (Linux/macOS, Bash or Zsh)**

~~~sh
cd ..
git clone https://github.com/espressif/esp-idf.git esp-idf-esp-sdr
cd esp-idf-esp-sdr
git checkout 25fe69f946311abdaf9ad56591f25fedbc20ac98
git submodule update --init --recursive
./install.sh esp32,esp32s3
. ./export.sh
cd ../esp-sdr-m5
~~~

**PowerShell (Windows)**

~~~powershell
Set-Location ..
git clone https://github.com/espressif/esp-idf.git esp-idf-esp-sdr
Set-Location esp-idf-esp-sdr
git checkout 25fe69f946311abdaf9ad56591f25fedbc20ac98
git submodule update --init --recursive
.\install.ps1 'esp32,esp32s3'
. .\export.ps1
Set-Location ..\esp-sdr-m5
~~~

These target packages cover the M5Stack, LILYGO and ATS-mini profiles listed
in the main README. If you build another profile, install its chip target as
listed in `firmware-targets.json`.

## 3. Activate ESP-IDF in each terminal

Run the activation command from the repository root in every new terminal
session before using `idf.py`.

**sh**

~~~sh
. ../esp-idf-esp-sdr/export.sh
~~~

**PowerShell**

~~~powershell
. ..\esp-idf-esp-sdr\export.ps1
~~~

## 4. Build and flash your device

Connect the device with a USB data cable, then follow its device guide for the
profile-specific build command and hardware notes. Replace `PORT` with the
device's serial port, such as `COM5`, `/dev/ttyUSB0`, `/dev/ttyACM0` or
`/dev/cu.usbmodem...`. Keep a separate build directory for each profile.

For versioned firmware artifacts, use `tools/build_firmware.py` as described in
the device guide. It reads the target, defaults, required options and pinned
ESP-IDF revision from `firmware-targets.json`.
