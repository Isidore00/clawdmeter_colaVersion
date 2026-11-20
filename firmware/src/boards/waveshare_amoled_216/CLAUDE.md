# AMOLED-2.16 (S3) — `waveshare_amoled_216`

Hardware notes for this board. Shared firmware rules are in `firmware/CLAUDE.md`.

original Waveshare ESP32-S3-Touch-AMOLED-2.16 (CO5300, 480×480 square, CST9220 touch, IMU rotation). Build env: `waveshare_amoled_216`.

## Hardware (critical pins)

- Display: **CO5300** AMOLED via QSPI (CS=12, SCLK=38, SDIO0..3=4..7, RST=2)
- Touch: **CST9220** via I2C (SDA=15, SCL=14, INT=11, addr=0x5A)
- PMU: **AXP2101** on same I2C bus (addr=0x34) — battery, USB VBUS, PWR button IRQ
- IMU: **QMI8658** on same I2C bus (addr=0x6B) — accelerometer for auto-rotation
  - Runs at 28 Hz in normal mode for the whole uptime (polled at 10 Hz). SensorLib 0.5's `configAccel()` only picks normal-mode rates; its low-power rates are reachable only through wake-on-motion. The 0.2.6 port used 21 Hz low-power: the difference is small next to the AMOLED, but it is a permanent idle cost.
- Buttons: GPIO 0 (left → Space/voice-mode), GPIO 18 (right → Shift+Tab/mode-toggle), AXP PKEY (middle → PWR: brightness; hold → pair)
- Touch axes: the CST9220 needs `setSwapXY(true)` + `setMirrorXY(true, false)`, applied in `touch.cpp::touch_hal_init()`.
- Rotation: the CO5300 can't exchange rows/columns, so `display_hal_draw_bitmap` remaps pixels on the CPU (PARTIAL render mode, 480×40 strips). A rotation change flashes the AMOLED brightness and forces a full redraw, handled in `display_hal_tick`.

## TF (microSD) slot — research only, no driver yet

Checked 2026-11-21 against:
- Docs pinout: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16 ("TF | TF card slot | SD SPI | GPIO1/2/3/41").
- Vendor pin header: https://raw.githubusercontent.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/main/examples/arduino/libraries/Mylibrary/pin_config.h
- Schematic: https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.16/ESP32-S3-Touch-AMOLED-2.16-Schematic.pdf (only its net names were read, no rendering: the nets include `MOSI`, `MISO`, `SCK`, `SDCS`, `LCD_RESET`, `TP_RESET`; no SD DAT1/DAT2 nets).

Findings:
- Bus: **SPI** (one data line each way plus a chip select), not 4-bit SDMMC. 1-bit SDMMC on CMD/CLK/D0 is what the docs call "BSP", but CS on GPIO41 is wired, so SPI is the safe choice.
- Pins (docs): MOSI/CMD = **GPIO1**, SCK/CLK = **GPIO2**, MISO/D0 = **GPIO3**, CS = **GPIO41**. All direct ESP32 GPIOs, no IO expander.
- No capacity or format limits are documented. Arduino-ESP32's `SD`/`SD_MMC` read FAT12/16/32 and exFAT only if FatFs was built with it; plan on FAT32 cards of 32 GB or less.
- No vendor SD example exists in the repo (examples 01–07 and the ESP-IDF demos have none).

**Unresolved pin clash (blocks a TF driver):** this firmware uses **GPIO2** as
`LCD_RESET` and `TP_RST` (`board.h`, from the 2035121 HAL refactor). The vendor
`pin_config.h` says `#define LCD_RESET 39` and `#define TP_RST 40`, and the docs
list GPIO39 = LCD_RESET, GPIO40 = TP_RESET, GPIO2 = SD SCK. So on this board
GPIO2 is probably the card's clock, and the reset pulses go to a pin with no
reset on it (the panel still comes up, so this went unnoticed). Before any TF
code: move `LCD_RESET` to 39 and `TP_RST` to 40, then test display and touch on
the real board. Not verified on hardware.
