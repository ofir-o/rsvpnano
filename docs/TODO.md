# RSVP Nano — AMOLED 1.75 task list

Living backlog for the 1.75 work. Status keys: `IDEA` (needs scoping), `READY` (scoped,
buildable), `IN PROGRESS`, `DONE (verify)` (code done, needs on-device check), `DONE`.

When a piece of work finishes, pick the next item from here.

## Difficulty order (easiest → hardest)
1. **#4 Pastel themes** — easiest. Pure UI palette additions, no hardware/time deps.
2. **#3 Hold-to-read mode** — easy. One localized input-mode change + a settings toggle.
3. **#6 Round chrome cleanup** — easy *if split*: removing the clipped corner labels + edge lines is
   trivial; the **clock** part is medium because it needs a time source (RTC/Wi-Fi/manual).
4. **#7 Multiple bookmarks** — medium. Persistence (sidecar file) + create/list/jump UI, but no
   hardware or time deps.
5. **#1 Gyro auto-level** — medium-hard. Renderer already supports 4 orientations, so it's IMU →
   orientation wiring + debounce + on-device tuning.
6. **#5 Reading Tamagotchi** — hardest. New subsystem: daily-goal tracking, persistence, time/day
   source, pet states/art, a screen, and death/revive logic.
- (#2 button roles — DONE, pending flash verify.)

Shared dependency: #6 (clock) and #5 (Tamagotchi) both need a real time source; wiring the onboard
PCF85063 RTC once unblocks both.

---

## Recently finished (pending a flash to verify)
- Green vertical stripes → CO5300 QSPI clock raised to 40 MHz on the 1.75. **DONE (verify)**
- Auto power-off after ~15s → skip critical-battery shutdown while on USB power. **DONE (verify)**
- Reversed touch axes → fixed (you confirmed). **DONE**
- Pause delay → default changed to "Instant". **DONE (verify)**

---

## Backlog

### 1. Keep text level using the gyro (auto-rotate to gravity)  — IDEA / big
**My understanding:** use the onboard IMU (QMI8658 accelerometer) to detect which way is "down"
and rotate the on-screen text so it always reads horizontally, parallel to the ground, even as you
turn the round watch on your wrist.
**Questions for you:**
- Snap to 4 orientations (0/90/180/270°) like a phone, or smooth/continuous rotation at any angle?
  (Continuous is much harder on this renderer and may be slow; 4-way is the realistic first step.)
- Should it rotate while words are playing, or only re-level when you pause (to avoid distraction)?
- Add a small dead-zone / delay so tiny hand movements don't flip it constantly?

### 2. Button roles: BOOT = power, PWR = play/resume  — DONE (verify)
**Status:** Already implemented via `SWAP_APP_BOOT_AND_POWER_BUTTONS` (on branch, needs flash).
After flashing: small BOOT button hold → the power-off confirm menu (turn off / etc.); large PWR
button → play/pause/resume. Verify it behaves as you described.
**Open nuance:** this interacts with task 3 (hold-to-read on PWR) — see below.

### 3. Hold-to-read mode (deadman switch)  — IDEA, needs a clear decision
**My understanding (please confirm):** add a Settings toggle for a reading mode where the words
only advance **while you hold the play/resume (PWR) button down**, and **stop the instant you let
go**. Like holding a "fast-forward" button — release = pause. This is different from the normal
tap-to-toggle (tap once = play, tap again = pause).
**Why I need to confirm:** holding PWR currently does "enter standby." If hold-to-read is on, a
hold must mean "keep reading," not standby — so the two can't both use PWR-hold. Options:
- Make it a setting: when "Hold to read" is ON, PWR-hold = read-while-held (and standby moves to,
  say, BOOT or is disabled); when OFF, current behavior.
- Or a separate momentary mode you enter from a menu.
Tell me which feels right and I'll spec it precisely.

### 4. Pastel color themes  — READY-ish
**My understanding:** add new selectable color palettes in Settings → Display → Theme, with cute
pastel options (e.g. pink, sage green, beige), keeping text/background contrast high enough to stay
comfortably readable.
**Questions for you:**
- How many palettes to start with? I'd suggest 3 (pink, sage, beige) plus the existing dark/light.
- Each palette = a background color + a text color + the red focus-letter color. Want the focus
  letter to stay red, or tint it to match each palette?
- The display is AMOLED (true black saves power); pastel backgrounds are bright/always-lit — fine
  for you? (Just flagging battery/burn-in, not a blocker.)

### 5. Reading Tamagotchi (virtual pet fed by reading)  — IDEA / big, fun
**My understanding:** a little virtual creature that you must "feed" by reading a daily goal (e.g.
~10 pages/day). Meet the goal and it's happy/healthy; miss days and it gets sad / "dies." A
gamified reading-streak companion.
**Questions for you:**
- What's "a page" here (RSVP has no pages)? Suggest defining the goal in **words/day** or
  **minutes read/day** (e.g. 2,000 words ≈ "10 pages"). Which do you prefer?
- Where does it live — a home-screen widget, a dedicated menu screen, or the standby/screensaver?
- Death = permanent reset, or just "sick" until you read again (gentler)?
- Needs a real-time clock to know "a day." The 1.75 has a PCF85063 RTC on board (not yet wired in
  firmware), or we can use Wi-Fi time / elapsed-time counting. Preference?

### 6. Round-screen reader chrome cleanup  — READY-ish
**My understanding:** on the round 1.75 the reader's corner labels get clipped by the bezel. Wanted:
- **Remove** the labels at **top-left, bottom-left, and bottom-right** (the ones cut off / unreadable).
- **Keep** the **battery percentage at top-right** (currently looks good, not clipped).
- **Add a small clock at the top** of the screen.
- **Remove the small edge lines at the top and bottom** (the paused-reader swipe-handle hints).
**Questions for you:**
- The clock needs the time from somewhere. Options: the onboard PCF85063 RTC (needs wiring in
  firmware), Wi-Fi time sync (needs Wi-Fi at least once), or manual set in Settings. Which do you
  want? (Ties into the Tamagotchi "what is a day" question.)
- 12h or 24h format? Show it always, or only while paused / in menus?
- Removing the top/bottom edge handles is cosmetic; the swipe menus still work — OK to just hide the
  hint lines? (Could make this 1.75-only so other boards keep the hints.)
- After removing three corners, should the remaining bottom-center area show anything (e.g. reading
  progress), or stay clean?

### 7. Multiple bookmarks  — IDEA, medium
**My understanding:** beyond the automatic "resume where I left off," let me save several named/marked
positions in a book and jump back to any of them later.
**Questions for you:**
- Bookmarks per-book (most common) or a global list across all books?
- How do you create one while reading — a menu item, or a gesture/button? (On the round 1.75 the
  inputs are limited: PWR=play/pause, BOOT=power; we'd likely add a "Bookmark here" entry in the
  main menu, plus a "Bookmarks" list to jump from.)
- Auto-label each bookmark with the surrounding words + % progress, or let you type a name? (Typing
  on this device is slow; auto-label is probably nicer.)
- How many max per book (e.g. 10)? Stored as a small sidecar file next to the book in flash.
- Should removing/clearing bookmarks be easy (swipe/Back on the list)?

---

## Notes
- Each non-trivial item should get its own branch/PR and an on-device verification pass.
- The 1.75 is flash-only (no SD); keep features mindful of the ~5.9 MB library budget.
