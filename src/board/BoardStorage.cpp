#include "board/BoardStorage.h"

#include <FFat.h>
#include <SD_MMC.h>
#include <driver/gpio.h>

namespace Board::Storage {

namespace {

// Base mount path and partition label for the internal-flash FAT volume. The label must match the
// `ffat` partition in the board's partition table (partitions_16MB_ffat.csv).
constexpr const char *kFlashBasePath = "/ffat";
constexpr const char *kFlashPartitionLabel = "ffat";

constexpr bool kUsesInternalFlash =
    Board::Config::STORAGE_BUS == Board::StorageBusKind::InternalFlashFat;

}  // namespace

bool setSdMmcPins() {
  return SD_MMC.setPins(Board::Config::PIN_SD_CLK, Board::Config::PIN_SD_CMD,
                        Board::Config::PIN_SD_D0);
}

void configureSdMmcSlot(sdmmc_slot_config_t &slotConfig) {
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  slotConfig.clk = static_cast<gpio_num_t>(Board::Config::PIN_SD_CLK);
  slotConfig.cmd = static_cast<gpio_num_t>(Board::Config::PIN_SD_CMD);
  slotConfig.d0 = static_cast<gpio_num_t>(Board::Config::PIN_SD_D0);
  slotConfig.d1 = GPIO_NUM_NC;
  slotConfig.d2 = GPIO_NUM_NC;
  slotConfig.d3 = GPIO_NUM_NC;
#else
  (void)slotConfig;
#endif
}

bool usesRemovableCard() { return !kUsesInternalFlash; }

fs::FS &fs() {
  if (kUsesInternalFlash) {
    return FFat;
  }
  return SD_MMC;
}

bool mountInternalFlash() {
  if (!kUsesInternalFlash) {
    return false;
  }
  // Format on first boot so a freshly flashed device comes up with an empty, usable library
  // partition instead of failing to mount.
  if (FFat.begin(true, kFlashBasePath, 10, kFlashPartitionLabel)) {
    return true;
  }
  Serial.println("[storage] FFat mount failed even after format attempt");
  return false;
}

void unmount() {
  if (kUsesInternalFlash) {
    FFat.end();
  } else {
    SD_MMC.end();
  }
}

uint64_t totalBytes() {
  if (kUsesInternalFlash) {
    return FFat.totalBytes();
  }
  return SD_MMC.totalBytes();
}

uint64_t usedBytes() {
  if (kUsesInternalFlash) {
    return FFat.usedBytes();
  }
  return SD_MMC.usedBytes();
}

}  // namespace Board::Storage
