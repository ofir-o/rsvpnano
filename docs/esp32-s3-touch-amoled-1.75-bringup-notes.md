# ESP32-S3 Touch AMOLED 1.75 Bring-Up Notes

This port targets the Waveshare `ESP32-S3-Touch-AMOLED-1.75` (and the cased
`1.75C` variant, which shares the same electronics). It is an **experimental,
not-yet-hardware-verified** target: the profile is derived from Waveshare's
published `pin_config.h` and example sketches plus the existing 2.16 board
profile, but it has not been confirmed on a physical unit yet. Treat the first
flash as bring-up.

## Board Summary

- MCU: `ESP32-S3R8` (16 MB flash, 8 MB PSRAM)
- Display: `CO5300`, `466x466` **round** AMOLED, QSPI
- Touch: `CST9217` (CST92xx-family protocol) on I2C address `0x5A`
- PMU: `AXP2101`
- IMU: `QMI8658` on I2C address `0x6B`
- Audio: `ES8311` codec
- Storage: `SD_MMC` 1-bit

## Pins Used

- LCD `CS=12`
- LCD `SCLK=38`
- LCD `DATA0=4`
- LCD `DATA1=5`
- LCD `DATA2=6`
- LCD `DATA3=7`
- LCD `RST=39`
- Touch / PMU / IMU / codec shared I2C `SDA=15`, `SCL=14`
- Touch `RST=40`
- Touch `INT=11` (present, but firmware polls instead of gating on it)
- SD `CLK=2`
- SD `CMD=1`
- SD `D0=3`
- Audio `MCLK=16`, `BCLK=9`, `WS=45`, `DOUT=8`, `DIN=10`
- `BOOT` button `GPIO0`
- `PWR` button via `AXP2101` PMU power key, not a dedicated ESP32 GPIO
- No dedicated `KEY` button (unlike the 2.16)

Pin source: `waveshareteam/ESP32-S3-Touch-AMOLED-1.75`
`examples/Arduino-v3.3.5/libraries/Mylibrary/pin_config.h` and the `01_Hello_world`
/ `08_ES8311` example sketches.

## Current Port Shape

- Added build target `waveshare_esp32s3_touch_amoled_175` (plus a
  `benchmark_*` variant) in `platformio.ini`.
- Added board profile `src/platforms/waveshare_amoled_175/`. All backend files
  (`BoardDisplay`, `BoardTouch`, `BoardPower`, `BoardButtons`, `BoardSystem`,
  `BoardAudio`) are reused verbatim from the 2.16 profile; only `BoardConfig.h`
  is board-specific.
- Reused the existing `CO5300` display, `CST92xx` touch, `AXP2101` power, and
  `ES8311` audio drivers.
- Added a `web/firmware/manifest-esp32-s3-touch-amoled-1.75.json` flasher
  manifest and wired the target into `install-firmware.js`,
  `export_web_firmware.py`, `fetch_release_firmware.py`, and the release /
  preview workflows.

## Display Notes

- The CO5300 driver gained board-configurable `DISPLAY_COL_OFFSET` /
  `DISPLAY_ROW_OFFSET` constants. The 1.75 round panel uses a **column offset of
  6** (`col_offset=6, row_offset=0`, matching Waveshare's
  `Arduino_CO5300(..., 466, 466, 6, 0, 0, 0)` constructor). Existing square
  panels (2.16, 1.8 V2) set both offsets to 0, so their behavior is unchanged.
- Orientation: native, `UI_ROTATED_180=false`. If the panel comes up mirrored or
  rotated on hardware, adjust `UI_ROTATED_180` and/or the column offset.

## Round Display Notes

- The codebase has no circular masking; the reader is rectangular with per-board
  chrome margins. RSVP shows one centered word, which suits a round screen.
- The corner chrome (battery / footer / menu labels) is the only thing the
  curved bezel can clip. `READER_CHROME_MARGIN_*` and `READER_BATTERY_MARGIN_*`
  are set larger than the square 2.16 profile so corner labels stay inside the
  inscribed circle. These are a best-effort estimate and should be eyeballed and
  tuned on hardware.

## Touch Notes

- `CST9217` uses the same 16-bit `0xD000` read command, `0xAB` ACK, and 5-byte
  point format already implemented in `drivers/touch/cst92xx`, at the same
  `0x5A` address as the 2.16. No new driver was needed.
- If touch reports nothing on hardware, the most likely cause is that the
  CST9217 needs an explicit command-mode wake (`0x01` -> register `0xD101`)
  before it streams coordinates. If so, add that write to `Board::Touch::configure()`.

## Button Mapping Notes

- With no `KEY` button, `BOOT` carries the reader/menu shortcuts:
  `BOOT_BUTTON_TOGGLES_READER`, `BOOT_BUTTON_BACKS_OUT_OF_MENU`, and
  `BOOT_BUTTON_HOLD_STARTS_STANDBY` are all enabled (as on the 1.8 V2).
- `PWR` (AXP2101 power key) handles power on/off; firmware power-button handling
  stays enabled with PMU power-key IRQs.

## Build Verification

- `pio run -e waveshare_esp32s3_touch_amoled_175` is expected to compile (shares
  the 2.16 driver set). Hardware behavior is unverified.

## Expected First Hardware Checks

1. Display lights up, image is not shifted/wrapped horizontally (validates the
   column offset).
2. No bright/dark seam at the panel edges.
3. Touch responds and coordinates are not inverted/mirrored.
4. Battery percentage reads sensibly via AXP2101.
5. SD card mounts and the library loads.
6. BOOT tap toggles play/pause; BOOT hold enters standby; PWR powers off.

## Source References

- Waveshare repo: <https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75>
- Product page: <https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm>
- Wiki: <https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75>
