#include "drivers/display/co5300/co5300.h"

#include <Arduino.h>
#include <SPI.h>
#include <cstring>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include "board/BoardConfig.h"

namespace {

constexpr int kSpiFrequency = Board::Config::DISPLAY_QSPI_CLOCK_HZ;
constexpr int kSendBufferRows =
    Board::Config::DISPLAY_TX_CHUNK_BYTES /
    (Board::Config::PANEL_NATIVE_WIDTH * static_cast<int>(sizeof(uint16_t)));
static_assert(kSendBufferRows > 0, "CO5300 transfer buffer must hold at least one full row");
constexpr int kSendBufferPixels = Board::Config::PANEL_NATIVE_WIDTH * kSendBufferRows;
constexpr uint8_t kRamWriteCommand = 0x2C;
constexpr uint8_t kRamWriteContinueCommand = 0x3C;
static const char *kCo5300Tag = "co5300";

struct LcdCommand {
  uint8_t cmd;
  uint8_t data[4];
  uint8_t len;
  uint16_t delayMs;
};

// Keep the panel memory in its native orientation and let the shared mapping layer handle the
// landscape transform, just like the stabilized 2.41 port.
constexpr uint8_t kDefaultMadctl = Board::Config::UI_ROTATED_180 ? 0xC0 : 0x00;

void sendCommand(Co5300::Context &context, uint8_t command, const uint8_t *data,
                 uint32_t length) {
  if (context.spi == nullptr) {
    return;
  }

  spi_transaction_t transaction = {};
  transaction.cmd = 0x02;
  transaction.addr = static_cast<uint32_t>(command) << 8;
  if (length != 0) {
    transaction.tx_buffer = data;
    transaction.length = length * 8;
  }

  ESP_ERROR_CHECK(spi_device_polling_transmit(context.spi, &transaction));
}

void setColumnWindow(Co5300::Context &context, uint16_t x1, uint16_t x2) {
  const uint8_t data[] = {
      static_cast<uint8_t>(x1 >> 8),
      static_cast<uint8_t>(x1),
      static_cast<uint8_t>(x2 >> 8),
      static_cast<uint8_t>(x2),
  };
  sendCommand(context, 0x2A, data, sizeof(data));
}

void setRowWindow(Co5300::Context &context, uint16_t y1, uint16_t y2) {
  const uint8_t data[] = {
      static_cast<uint8_t>(y1 >> 8),
      static_cast<uint8_t>(y1),
      static_cast<uint8_t>(y2 >> 8),
      static_cast<uint8_t>(y2),
  };
  sendCommand(context, 0x2B, data, sizeof(data));
}

void applyBrightness(Co5300::Context &context) {
  const uint8_t level =
      context.displayOn
          ? static_cast<uint8_t>((static_cast<uint16_t>(context.brightnessPercent) * 255U) / 100U)
          : 0;
  sendCommand(context, 0x51, &level, 1);
}

}  // namespace

namespace Co5300 {

void init(Context &context) {
  if (Board::Config::PIN_LCD_RST >= 0) {
    pinMode(Board::Config::PIN_LCD_RST, OUTPUT);
    digitalWrite(Board::Config::PIN_LCD_RST, HIGH);
    delay(10);
    digitalWrite(Board::Config::PIN_LCD_RST, LOW);
    delay(200);
    digitalWrite(Board::Config::PIN_LCD_RST, HIGH);
    delay(200);
  }

  if (!context.busReady) {
    spi_bus_config_t busConfig = {};
    busConfig.data0_io_num = Board::Config::PIN_LCD_DATA0;
    busConfig.data1_io_num = Board::Config::PIN_LCD_DATA1;
    busConfig.sclk_io_num = Board::Config::PIN_LCD_SCLK;
    busConfig.data2_io_num = Board::Config::PIN_LCD_DATA2;
    busConfig.data3_io_num = Board::Config::PIN_LCD_DATA3;
    busConfig.max_transfer_sz = (kSendBufferPixels * static_cast<int>(sizeof(uint16_t))) + 8;
    busConfig.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.command_bits = 8;
    deviceConfig.address_bits = 24;
    deviceConfig.mode = SPI_MODE0;
    deviceConfig.clock_speed_hz = kSpiFrequency;
    deviceConfig.spics_io_num = Board::Config::PIN_LCD_CS;
    deviceConfig.flags = SPI_DEVICE_HALFDUPLEX;
    deviceConfig.queue_size = 10;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &busConfig, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &deviceConfig, &context.spi));
    context.busReady = true;
  }

  const auto cmd1 = [&](uint8_t command, uint8_t value) {
    const uint8_t data = value;
    sendCommand(context, command, &data, 1);
  };

  sendCommand(context, 0x11, nullptr, 0);  // Sleep out
  delay(120);

  if (Board::Config::CO5300_EXTRA_PANEL_TUNING) {
    // Page-0x20 source/gate tuning required by the 480-class CO5300 panels (2.16 / 1.8 V2).
    // The 466 round 1.75 panel must NOT receive these or it renders evenly-spaced vertical
    // stripes; it uses the plain CO5300 init below.
    cmd1(0xFE, 0x20);
    cmd1(0x19, 0x10);
    cmd1(0x1C, 0xA0);
  }

  cmd1(0xFE, 0x00);  // Command page 0
  cmd1(0xC4, 0x80);  // QSPI write mode
  cmd1(0x3A, 0x55);  // 16bpp RGB565
  cmd1(0x35, 0x00);  // Tearing effect on
  cmd1(0x53, 0x20);  // Brightness control block on
  cmd1(0x51, 0xFF);  // Display brightness
  cmd1(0x63, 0xFF);  // HBM brightness

  // Power-on address window from the panel's native resolution and column/row offset. The 466
  // round panel starts at column 6; square panels keep offset 0. pushColors re-sets this per frame.
  setColumnWindow(context, Board::Config::DISPLAY_COL_OFFSET,
                  static_cast<uint16_t>(Board::Config::DISPLAY_COL_OFFSET +
                                        Board::Config::PANEL_NATIVE_WIDTH - 1));
  setRowWindow(context, Board::Config::DISPLAY_ROW_OFFSET,
               static_cast<uint16_t>(Board::Config::DISPLAY_ROW_OFFSET +
                                     Board::Config::PANEL_NATIVE_HEIGHT - 1));

  cmd1(0x36, kDefaultMadctl);              // MADCTL
  sendCommand(context, 0x29, nullptr, 0);  // Display on
  delay(10);

  context.displayOn = true;
  applyBrightness(context);
  ESP_LOGI(kCo5300Tag, "CO5300 QSPI init complete");
}

void setDisplayOn(Context &context, bool on) {
  context.displayOn = on;
  sendCommand(context, on ? 0x29 : 0x28, nullptr, 0);
  applyBrightness(context);
}

void setBrightnessPercent(Context &context, uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  context.brightnessPercent = percent;
  applyBrightness(context);
}

void sleep(Context &context) {
  context.displayOn = false;
  sendCommand(context, 0x28, nullptr, 0);
  delay(10);
  sendCommand(context, 0x10, nullptr, 0);
  delay(120);
}

void wake(Context &context) {
  sendCommand(context, 0x11, nullptr, 0);
  delay(120);
  context.displayOn = true;
  sendCommand(context, 0x29, nullptr, 0);
  applyBrightness(context);
}

void pushColors(Context &context, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                const uint16_t *data) {
  if (context.spi == nullptr || data == nullptr || width == 0 || height == 0) {
    return;
  }

  bool firstSend = true;
  size_t pixelsRemaining = static_cast<size_t>(width) * height;
  const uint16_t *cursor = data;

  // When the source frame lives in PSRAM (whole-frame flush boards), copy each chunk through an
  // internal DMA-capable bounce buffer so the SPI transfer always reads from internal RAM.
  uint16_t *bounce = nullptr;
  if (Board::Config::DISPLAY_FLUSH_WHOLE_FRAME) {
    static uint16_t *sBounce = static_cast<uint16_t *>(
        heap_caps_malloc(static_cast<size_t>(kSendBufferPixels) * sizeof(uint16_t),
                         MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    bounce = sBounce;
  }

  // Round panels such as the 1.75 address the CO5300 RAM with a non-zero column start; existing
  // square panels keep both offsets at zero so their windows are unchanged.
  const uint16_t colStart = static_cast<uint16_t>(x + Board::Config::DISPLAY_COL_OFFSET);
  const uint16_t rowStart = static_cast<uint16_t>(y + Board::Config::DISPLAY_ROW_OFFSET);
  setColumnWindow(context, colStart, static_cast<uint16_t>(colStart + width - 1));
  setRowWindow(context, rowStart, static_cast<uint16_t>(rowStart + height - 1));
  sendCommand(context, kRamWriteCommand, nullptr, 0);

  while (pixelsRemaining > 0) {
    size_t chunkPixels = pixelsRemaining;
    if (chunkPixels > static_cast<size_t>(kSendBufferPixels)) {
      chunkPixels = kSendBufferPixels;
    }

    const uint16_t *txPtr = cursor;
    if (bounce != nullptr) {
      memcpy(bounce, cursor, chunkPixels * sizeof(uint16_t));
      txPtr = bounce;
    }

    spi_transaction_ext_t transaction = {};
    transaction.base.flags = SPI_TRANS_MODE_QIO;
    transaction.base.cmd = 0x32;
    transaction.base.addr = static_cast<uint32_t>(firstSend ? kRamWriteCommand
                                                            : kRamWriteContinueCommand)
                            << 8;
    transaction.base.tx_buffer = txPtr;
    transaction.base.length = chunkPixels * 16;

    ESP_ERROR_CHECK(
        spi_device_polling_transmit(context.spi,
                                    reinterpret_cast<spi_transaction_t *>(&transaction)));

    firstSend = false;
    pixelsRemaining -= chunkPixels;
    cursor += chunkPixels;
  }
}

}  // namespace Co5300
