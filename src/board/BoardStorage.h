#pragma once

#include <Arduino.h>
#include <FS.h>
#include <driver/sdmmc_host.h>

#include "board/BoardConfig.h"

namespace Board::Storage {

// SD-MMC pin/slot helpers (used only by the removable-card path).
bool setSdMmcPins();
void configureSdMmcSlot(sdmmc_slot_config_t &slotConfig);

// Generic library filesystem accessor. Returns the SD card for removable-card boards, or the
// internal-flash FAT (FFat) volume for boards configured with StorageBusKind::InternalFlashFat.
// All library file operations go through this so the rest of the firmware is storage-agnostic.
fs::FS &fs();

// True when the active storage is a removable SD card, false for the internal-flash backend.
bool usesRemovableCard();

// Internal-flash (FFat) lifecycle. No-ops / false on removable-card boards.
bool mountInternalFlash();
void unmount();

// Total / used bytes of the active volume (works for both SD and FFat).
uint64_t totalBytes();
uint64_t usedBytes();

}  // namespace Board::Storage
