#include "drivers/audio/es8311/Es8311.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace BoardDrivers::Es8311 {
namespace {

constexpr char kTag[] = "es8311";

constexpr uint8_t kResetReg = 0x00;
constexpr uint8_t kClkManagerReg01 = 0x01;
constexpr uint8_t kClkManagerReg02 = 0x02;
constexpr uint8_t kClkManagerReg03 = 0x03;
constexpr uint8_t kClkManagerReg04 = 0x04;
constexpr uint8_t kClkManagerReg05 = 0x05;
constexpr uint8_t kClkManagerReg06 = 0x06;
constexpr uint8_t kClkManagerReg07 = 0x07;
constexpr uint8_t kClkManagerReg08 = 0x08;
constexpr uint8_t kSdPinReg09 = 0x09;
constexpr uint8_t kSdPoutReg0A = 0x0A;
constexpr uint8_t kSystemReg0B = 0x0B;
constexpr uint8_t kSystemReg0C = 0x0C;
constexpr uint8_t kSystemReg0D = 0x0D;
constexpr uint8_t kSystemReg0E = 0x0E;
constexpr uint8_t kSystemReg10 = 0x10;
constexpr uint8_t kSystemReg11 = 0x11;
constexpr uint8_t kSystemReg12 = 0x12;
constexpr uint8_t kSystemReg13 = 0x13;
constexpr uint8_t kSystemReg14 = 0x14;
constexpr uint8_t kAdcReg15 = 0x15;
constexpr uint8_t kAdcReg16 = 0x16;
constexpr uint8_t kAdcReg17 = 0x17;
constexpr uint8_t kAdcReg1B = 0x1B;
constexpr uint8_t kAdcReg1C = 0x1C;
constexpr uint8_t kDacReg31 = 0x31;
constexpr uint8_t kDacReg32 = 0x32;
constexpr uint8_t kDacReg37 = 0x37;
constexpr uint8_t kGpioReg44 = 0x44;
constexpr uint8_t kGpReg45 = 0x45;
constexpr uint8_t kChipId1RegFD = 0xFD;
constexpr uint8_t kChipId2RegFE = 0xFE;
constexpr uint8_t kDacVolumeMax = 0xFF;

Config gConfig;
bool gAvailable = false;
bool gI2sInitialized = false;

bool readRegister(uint8_t reg, uint8_t &value) {
  if (gConfig.wire == nullptr) {
    return false;
  }

  gConfig.wire->beginTransmission(gConfig.address);
  gConfig.wire->write(reg);
  if (gConfig.wire->endTransmission(false) != 0) {
    return false;
  }
  if (gConfig.wire->requestFrom(static_cast<int>(gConfig.address), 1, 1) != 1) {
    return false;
  }

  value = gConfig.wire->read();
  return true;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  if (gConfig.wire == nullptr) {
    return false;
  }

  gConfig.wire->beginTransmission(gConfig.address);
  gConfig.wire->write(reg);
  gConfig.wire->write(value);
  return gConfig.wire->endTransmission(true) == 0;
}

bool initI2s() {
  if (gI2sInitialized) {
    return true;
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = gConfig.sampleRateHz;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = 0;
  config.dma_buf_count = 4;
  config.dma_buf_len = 240;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;

  esp_err_t result = i2s_driver_install(gConfig.i2sPort, &config, 0, nullptr);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to install I2S driver: %s", esp_err_to_name(result));
    return false;
  }

  i2s_pin_config_t pinConfig = {};
  pinConfig.mck_io_num = gConfig.mclkPin;
  pinConfig.bck_io_num = gConfig.bclkPin;
  pinConfig.ws_io_num = gConfig.wsPin;
  pinConfig.data_out_num = gConfig.dataOutPin;
  pinConfig.data_in_num = I2S_PIN_NO_CHANGE;

  result = i2s_set_pin(gConfig.i2sPort, &pinConfig);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set I2S pins: %s", esp_err_to_name(result));
    return false;
  }

  result = i2s_set_clk(gConfig.i2sPort, gConfig.sampleRateHz, I2S_BITS_PER_SAMPLE_16BIT,
                       I2S_CHANNEL_STEREO);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set I2S clock: %s", esp_err_to_name(result));
    return false;
  }

  i2s_zero_dma_buffer(gConfig.i2sPort);
  gI2sInitialized = true;
  return true;
}

bool detectCodec() {
  uint8_t chipId1 = 0;
  uint8_t chipId2 = 0;
  if (readRegister(kChipId1RegFD, chipId1) && readRegister(kChipId2RegFE, chipId2)) {
    ESP_LOGI(kTag, "ES8311 detected: id=%02X %02X", chipId1, chipId2);
  }
  return true;
}

bool configureSampleFormat() {
  uint8_t dacIface = 0;
  uint8_t adcIface = 0;
  uint8_t reg = 0;

  if (!readRegister(kSdPinReg09, dacIface) || !readRegister(kSdPoutReg0A, adcIface)) {
    return false;
  }

  dacIface = static_cast<uint8_t>((dacIface & 0xE0U) | 0x0CU);
  adcIface = static_cast<uint8_t>((adcIface & 0xE0U) | 0x0CU);

  return writeRegister(kSdPinReg09, dacIface) && writeRegister(kSdPoutReg0A, adcIface) &&
         readRegister(kClkManagerReg02, reg) &&
         writeRegister(kClkManagerReg02, static_cast<uint8_t>(reg & 0x07U)) &&
         writeRegister(kClkManagerReg05, 0x00) && readRegister(kClkManagerReg03, reg) &&
         writeRegister(kClkManagerReg03, static_cast<uint8_t>((reg & 0x80U) | 0x10U)) &&
         readRegister(kClkManagerReg04, reg) &&
         writeRegister(kClkManagerReg04, static_cast<uint8_t>((reg & 0x80U) | 0x10U)) &&
         readRegister(kClkManagerReg07, reg) &&
         writeRegister(kClkManagerReg07, static_cast<uint8_t>(reg & 0xC0U)) &&
         writeRegister(kClkManagerReg08, 0xFF) && readRegister(kClkManagerReg06, reg) &&
         writeRegister(kClkManagerReg06, static_cast<uint8_t>((reg & 0xE0U) | 0x03U));
}

bool startCodec() {
  uint8_t dacIface = 0;
  uint8_t adcIface = 0;
  uint8_t dacMute = 0;

  if (!writeRegister(kResetReg, 0x80) || !writeRegister(kClkManagerReg01, 0x3F) ||
      !readRegister(kSdPinReg09, dacIface) || !readRegister(kSdPoutReg0A, adcIface)) {
    return false;
  }

  dacIface &= static_cast<uint8_t>(~(1U << 6));
  adcIface |= static_cast<uint8_t>(1U << 6);

  const bool started =
      writeRegister(kSdPinReg09, dacIface) && writeRegister(kSdPoutReg0A, adcIface) &&
      writeRegister(kAdcReg17, 0xBF) && writeRegister(kSystemReg0E, 0x02) &&
      writeRegister(kSystemReg12, 0x00) && writeRegister(kSystemReg14, 0x1A) &&
      writeRegister(kSystemReg0D, 0x01) && writeRegister(kAdcReg15, 0x40) &&
      writeRegister(kDacReg37, 0x08) && writeRegister(kGpReg45, 0x00);

  if (!started || !readRegister(kDacReg31, dacMute)) {
    return false;
  }

  dacMute &= 0x9F;
  return writeRegister(kDacReg31, dacMute) && writeRegister(kDacReg32, kDacVolumeMax);
}

bool configureCodec() {
  uint8_t reg = 0;

  const bool opened =
      writeRegister(kGpioReg44, 0x08) && writeRegister(kGpioReg44, 0x08) &&
      writeRegister(kClkManagerReg01, 0x30) && writeRegister(kClkManagerReg02, 0x00) &&
      writeRegister(kClkManagerReg03, 0x10) && writeRegister(kAdcReg16, 0x24) &&
      writeRegister(kClkManagerReg04, 0x10) && writeRegister(kClkManagerReg05, 0x00) &&
      writeRegister(kSystemReg0B, 0x00) && writeRegister(kSystemReg0C, 0x00) &&
      writeRegister(kSystemReg10, 0x1F) && writeRegister(kSystemReg11, 0x7F) &&
      writeRegister(kResetReg, 0x80) && readRegister(kResetReg, reg) &&
      writeRegister(kResetReg, static_cast<uint8_t>(reg & 0xBF)) &&
      writeRegister(kClkManagerReg01, 0x3F) && readRegister(kClkManagerReg06, reg) &&
      writeRegister(kClkManagerReg06, static_cast<uint8_t>(reg & ~0x20U)) &&
      writeRegister(kSystemReg13, 0x10) && writeRegister(kAdcReg1B, 0x0A) &&
      writeRegister(kAdcReg1C, 0x6A) && writeRegister(kGpioReg44, 0x58);

  return opened && configureSampleFormat() && startCodec();
}

}  // namespace

bool begin(const Config &config) {
  gConfig = config;
  if (gAvailable) {
    return true;
  }

  if (!initI2s() || !detectCodec() || !configureCodec()) {
    ESP_LOGW(kTag, "Audio codec setup failed");
    return false;
  }

  gAvailable = true;
  ESP_LOGI(kTag, "Speaker path ready");
  return true;
}

bool prepareOutput() {
  if (!gAvailable || !gI2sInitialized) {
    return false;
  }

  uint8_t dacMute = 0;
  if (readRegister(kDacReg31, dacMute)) {
    dacMute &= 0x9F;
    writeRegister(kDacReg31, dacMute);
  }
  writeRegister(kDacReg32, kDacVolumeMax);
  return true;
}

bool recoverOutputPath() {
  if (!startCodec() || !gI2sInitialized) {
    return false;
  }

  i2s_stop(gConfig.i2sPort);
  const esp_err_t result = i2s_start(gConfig.i2sPort);
  if (result != ESP_OK) {
    ESP_LOGW(kTag, "Failed to restart I2S TX: %s", esp_err_to_name(result));
    return false;
  }

  i2s_zero_dma_buffer(gConfig.i2sPort);
  return true;
}

bool writeSamples(const int16_t *samples, size_t sampleCount, uint32_t timeoutMs) {
  if (samples == nullptr || sampleCount == 0) {
    return false;
  }

  const uint8_t *data = reinterpret_cast<const uint8_t *>(samples);
  const size_t totalSize = sampleCount * sizeof(int16_t);
  size_t totalWritten = 0;

  while (totalWritten < totalSize) {
    size_t bytesWritten = 0;
    const esp_err_t result =
        i2s_write(gConfig.i2sPort, data + totalWritten, totalSize - totalWritten, &bytesWritten,
                  pdMS_TO_TICKS(timeoutMs));
    if (result != ESP_OK || bytesWritten == 0) {
      ESP_LOGW(kTag, "Sample write failed: %s", esp_err_to_name(result));
      return false;
    }
    totalWritten += bytesWritten;
  }

  return true;
}

bool available() { return gAvailable; }

}  // namespace BoardDrivers::Es8311
