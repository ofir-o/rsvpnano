# Handoff prompt — Pastel color themes (TODO #4)

Paste the prompt below to a fresh agent working on this repo. It is self-contained.

> **IMPORTANT: do NOT push or open a PR until the user has seen color swatches and approved a
> palette set.** Present swatches first, get the choice, then implement.

---

## Prompt

You are adding **pastel color themes** to the RSVP Nano firmware (an ESP32-S3 RSVP text reader).
The active device is the **Waveshare AMOLED 1.75** (round 466×466). Work in this repo.

### Branch / workflow rules
- Create your **own** branch off `main` (e.g. `claude/pastel-themes`). Do **NOT** use
  `claude/blissful-shannon-tgzp2u` — another agent is using it for an unrelated display fix.
- Commit with `git config user.email noreply@anthropic.com && user.name Claude` and `-S`.
- **Do not push or merge until the user approves the palettes** (see deliverable 1).
- To deploy/flash later: merging your branch to `main` triggers the GitHub Pages "Build web flasher"
  workflow, which builds only the 1.75 and redeploys `https://ofir-o.github.io/rsvpnano/`. The user
  flashes from there (hold BOOT while plugging USB to enter download mode).

### What the user wants (3 pastel palettes)
1. **Terracotta + beige** (warm).
2. **Baby pastel pink** (light/soft).
3. **Matcha / sage green**.
All must keep **high, comfortable text/background contrast** (this is a reading device — legibility
first). These are *additions* alongside the existing Dark / Light / Night / Yellow themes.

### Deliverable 1 — palettes for approval FIRST (blocking)
Before writing firmware, present the user **visual swatches** to choose from. Each palette needs:
- background color, body-text color, and the focus-letter (current-letter highlight) color,
- as RGB hex + the RGB565 value the firmware uses.
Give **2 candidate variants per palette** so the user can pick, and check contrast (aim for WCAG-ish
≥ 4.5:1 body text on background). The easiest way to show swatches: write a tiny standalone
`palette-preview.html` (color chips with the sample word "This is" in body + one focus letter) and
send it to the user with the SendUserFile tool, or render hex chips clearly. **Wait for the user's
pick before coding.** Suggested starting points (tune for contrast, user must approve):
- Terracotta+beige: bg `#E8DCC8`, text `#7A3B2E`, focus `#B5532E`.
- Baby pink: bg `#F8D7E3`, text `#5E2A4E`, focus `#C13B7A`.
- Matcha/sage: bg `#CBD8B0`, text `#33422A`, focus `#9C5A2E`.

### Deliverable 2 — implement the chosen palettes
The theme system lives here:
- **Color constants + selectors:** `src/display/DisplayManager.cpp` — see `kTrueBlack`, `kPureWhite`,
  `kDarkWordColor`, `kFocusLetterColor` (0xF800), `kNight*`, `kYellowMode*` near the top, and the
  member functions `backgroundColor()`, `wordColor()/textColor`, `focusColor()`, footer/label color
  helpers (all switch on `darkMode_` / `nightMode_` / yellow). There is an `rgb565(r,g,b)` helper.
- **Theme enum, cycling, label:** `src/app/App.cpp` / `App.h` — `cycleThemeMode()`,
  `themeModeLabel()`, and the theme mode state. Themes are also surfaced in quick settings ("Theme"
  cycles Dark/Light/Night/Yellow) and in Settings → Display.
- Add the new palettes to the theme enum, the color selectors (bg/text/focus/footer), the cycle
  order, the labels, and persistence (the `preferences_` key used for theme). Keep existing themes
  intact and unchanged.
- `panelColor()` applies any panel-specific channel handling — route new colors through the same
  path the existing themes use.
- Note the display is AMOLED: pastel (bright) backgrounds are always-lit (more power than true-black
  dark mode) — acceptable per the user, just don't make them the default.

### Verify
- `node`/compile checks aren't enough for firmware; rely on the CI build. Build only the 1.75 env
  if building locally: `pio run -e waveshare_esp32s3_touch_amoled_175`.
- Confirm contrast on each palette and that the focus letter stays clearly distinct from body text.
- Don't regress other boards (2.16 / 1.8 V2 / 3.49 / 2.41): the new palettes should be additive.

### After approval + implementation
- Commit on your branch, then ask the user before merging to `main` (merge = deploy + they reflash).
