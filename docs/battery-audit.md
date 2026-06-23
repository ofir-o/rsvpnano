# Battery-life audit (#8)

Read-only audit of the firmware's power behaviour on the AMOLED 1.75. Conclusion up front: the
firmware is **already well optimized**; there are no obvious "free" wins left that are safe to apply
blind (without on-device current measurement). Below is what exists and what to consider.

## Already in place (good)
- **Per-state CPU scaling** — `App::applyStateCpuFrequency()` drops the CPU clock by app state:
  Playing/Paused/Menu/Standby/Scroll each have their own configurable MHz (`cpuMhzPlay_`,
  `cpuMhzPaused_`, `cpuMhzMenu_`, `cpuMhzStandby_`, `cpuMhzScroll_`). Only OTA / sync / USB force
  240 MHz. This is the single biggest CPU saver and it's already done.
- **Auto-dim** — the screen dims to a configurable level after a configurable idle delay
  (`autoDimDelayMs_`, `autoDimBrightnessPercent_`, Settings → Battery). On AMOLED, lower brightness
  is a near-linear power saving.
- **Standby timer** — configurable idle→standby (Settings → Display → Standby timer), with a low
  standby CPU clock.
- **Wi-Fi radio off when idle** — Wi-Fi is only powered for sync/OTA and is returned to `WIFI_OFF`
  afterwards (`WifiConnection`, `CompanionSyncManager`). No always-on radio drain.
- **True-black dark themes** — the dark palettes added this session (Sage, Warm gold, Beige rose,
  plus the default Dark) use a `#000000` background; on AMOLED those pixels are physically off, so
  dark themes draw materially less than the pastel/light ones. This is a real, user-selectable saver.

## Suggestions (need your call / measurement before changing)
1. **Re-enable a gentle idle standby.** Auto-off was disabled earlier (the false "15 s shutdown"),
   but that was the *critical-battery* path, not idle standby. A screen-dim → screen-off → light
   standby after N minutes idle is the biggest remaining saver. Auto-dim already covers the first
   step; consider enabling the standby timer with a few-minutes default.
2. **Lower the paused/menu CPU further.** If reading still feels snappy, `cpuMhzPaused_` /
   `cpuMhzMenu_` could go to 80 MHz. Safe-ish but wants a feel check on hardware.
3. **Poll intervals** — touch (20 ms), battery (180 s), IMU/auto-rotate (120 ms) are already
   reasonable. Auto-rotate sampling could pause while actively Playing to save a little (see #1
   open question).
4. **Default to a dark theme** for new users to bias toward the low-power palettes.

## Not changed
No speculative power code was added: the gains above need a current meter to validate, and the
existing design is sound. The concrete battery win delivered this session is the **true-black dark
theme set**.
