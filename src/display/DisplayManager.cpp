#include "display/DisplayManager.h"

#include <algorithm>
#include <math.h>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>

#include "board/BoardDisplay.h"
#include "board/BoardConfig.h"
#include "display/EmbeddedAtkinsonFont.h"
#include "display/EmbeddedAtkinsonFont70.h"
#include "display/EmbeddedOpenDyslexicFont.h"
#include "display/EmbeddedOpenDyslexicFont70.h"
#include "display/EmbeddedSerifFont.h"
#include "display/EmbeddedSerifFont70.h"
#include "text/LatinText.h"

namespace {
constexpr int kDisplayWidth = Board::Config::DISPLAY_WIDTH;
constexpr int kDisplayHeight = Board::Config::DISPLAY_HEIGHT;
constexpr int kPanelNativeWidth = Board::Config::PANEL_NATIVE_WIDTH;
constexpr int kPanelNativeHeight = Board::Config::PANEL_NATIVE_HEIGHT;

constexpr int kMinTextScale = 1;
constexpr int kMaxTextScale = 1;
constexpr uint8_t kGlyphAlphaThreshold = 16;
constexpr uint16_t kTrueBlack = 0x0000;
constexpr uint16_t kPureWhite = 0xFFFF;
constexpr uint16_t kDarkWordColor = 0xEEB4;  // warm amber-cream (#EDD6A6), easy on the eyes
constexpr uint16_t kLightWordColor = 0x0000;
constexpr uint16_t kFocusLetterColor = 0xFC2E;  // peachy-pink focus (#FF8674)
constexpr uint16_t kYellowModeFocusColor = 0x001F;
constexpr uint16_t kNightWordColor = 0xFCE0;
constexpr uint16_t kNightFocusColor = 0xFA80;
constexpr uint16_t kYellowModeNightFocusColor = 0x7D7F;
constexpr uint16_t kYellowModeBackground = 0xFF44;

// Pastel reading palettes (recommended "A" variants). Each is background /
// body-text / focus-letter, as RGB565. Body-text contrast vs background is well
// above 4.5:1 on all three; the focus letter is a warm accent that sits among
// the body text (same role as the red focus in Dark/Light). See
// docs/palette-preview.html for the candidate variants and exact values.
// Terracotta + beige: bg #E8DCC8, text #494742, focus #A05733.
constexpr uint16_t kTerracottaBackground = 0xEEF9;
constexpr uint16_t kTerracottaWordColor = 0x4A28;
constexpr uint16_t kTerracottaFocusColor = 0xA2A6;
// Peach blush: bg #FEE4D9, text #5C3A2C, focus #AB6622.
constexpr uint16_t kPeachBackground = 0xFF3B;
constexpr uint16_t kPeachWordColor = 0x59C5;
constexpr uint16_t kPeachFocusColor = 0xAB24;
// Beige + olive: bg #E8DCC8, text #3E3F28, focus #888958.
constexpr uint16_t kOliveBackground = 0xEEF9;
constexpr uint16_t kOliveWordColor = 0x39E5;
constexpr uint16_t kOliveFocusColor = 0x8C4B;
// Dark palettes (true-black background = best battery on AMOLED). word / focus as RGB565.
// Sage: text #A8C0A0, focus #C9A86A.
constexpr uint16_t kSageWordColor = 0xAE14;
constexpr uint16_t kSageFocusColor = 0xCD4D;
// Warm gold: text #E4C07A, focus #B5774A.
constexpr uint16_t kWarmGoldWordColor = 0xE60F;
constexpr uint16_t kWarmGoldFocusColor = 0xB3A9;
// Beige rose: text #EAD2CB, focus #D99AA4.
constexpr uint16_t kBeigeRoseWordColor = 0xEE99;
constexpr uint16_t kBeigeRoseFocusColor = 0xDCD4;
constexpr uint16_t kDarkMenuDimColor = 0x8410;
constexpr uint16_t kLightMenuDimColor = 0x6B4D;
constexpr uint16_t kDarkFooterColor = 0x528A;
constexpr uint16_t kLightFooterColor = 0x5ACB;
constexpr uint8_t kNightDimAlpha = 92;
constexpr uint8_t kNightFooterAlpha = 132;
constexpr int kRsvpSideMargin = 12;
constexpr int kRsvpGuideTickHeight = 5;
constexpr int kRsvpGuideTopOffset = 7;
constexpr int kRsvpGuideBottomOffset = 7;
constexpr int kWpmFeedbackBottomMargin = 16;
constexpr int kTinyGlyphWidth = 5;
constexpr int kTinyGlyphHeight = 7;
constexpr int kTinyGlyphSpacing = 1;
constexpr int kTinyScale = 2;
constexpr int kReaderChromeMarginX = Board::Config::READER_CHROME_MARGIN_X;
constexpr int kReaderChromeMarginTop = Board::Config::READER_CHROME_MARGIN_TOP;
constexpr int kReaderChromeMarginBottom = Board::Config::READER_CHROME_MARGIN_BOTTOM;
constexpr int kReaderBatteryMarginX = Board::Config::READER_BATTERY_MARGIN_X;
constexpr int kReaderBatteryMarginTop = Board::Config::READER_BATTERY_MARGIN_TOP;
constexpr int kEdgeMenuHintMaxWidth = 34;
constexpr int kEdgeMenuHintMinWidth = 22;
constexpr int kEdgeMenuHintHeight = 3;
constexpr int kEdgeMenuHintInset = 3;
constexpr uint8_t kEdgeMenuHintAlpha = 72;
constexpr uint8_t kNightEdgeMenuHintAlpha = 88;
constexpr int kCompactMenuRowHeight = 22;
constexpr int kCompactMenuX = 28;
constexpr int kLibraryRowHeight = 38;
constexpr int kLibraryInsetX = 26;
constexpr int kLibraryTitleYOffset = 4;
constexpr int kLibrarySubtitleYOffset = 20;
constexpr int kLibraryScreenPaddingY = 28;
constexpr uint8_t kLibrarySubtitleAlpha = 120;
constexpr int kScrollMarginX = 18;
constexpr int kScrollTop = 8;
constexpr int kScrollLineHeight = 29;
constexpr int kScrollParagraphGap = 8;
constexpr int kScrollParagraphIndent = 22;
constexpr int kScrollSpaceWidth = 10;
constexpr int kScrollSerifDivisor = 2;
constexpr int kWordTickerGapLarge = 16;
constexpr int kWordTickerGapMedium = 12;
constexpr int kWordTickerGapSmall = 9;
constexpr int kWordTickerBandPadding = 10;
constexpr int kPhantomCurrentGapLarge = 30;
constexpr int kPhantomCurrentGapMedium = 24;
constexpr int kPhantomCurrentGapSmall = 20;
constexpr uint8_t kPhantomAlphaLarge = 54;
constexpr uint8_t kPhantomAlphaMedium = 62;
constexpr uint8_t kPhantomAlphaSmall = 72;
constexpr int kTypographyTrackingMin = -2;
constexpr int kTypographyTrackingMax = 3;
constexpr int kTypographyAnchorMin = 30;
constexpr int kTypographyAnchorMax = 60;
constexpr int kTypographyGuideHalfWidthMin = 12;
constexpr int kTypographyGuideHalfWidthMax = 30;
constexpr int kTypographyGuideGapMin = 2;
constexpr int kTypographyGuideGapMax = 8;
constexpr int kOpticalLetterGapPx = 2;

constexpr int kVirtualBufferWidth = kDisplayWidth;
constexpr int kVirtualBufferHeight = kPanelNativeHeight;

// Extra slack (px) added to the circular inset so anti-aliased glyph edges and the bezel curve do
// not shave content on the round panels.
constexpr int kCircularSafetyPaddingPx = 8;

// On the round AMOLED panels the visible glass is a circle inscribed in the square framebuffer, so
// content anchored a fixed distance from a square edge spills past the glass near the corners.
// circularSafeInsetForY(y) returns how far in from each (left/right) edge a row at y must stay to
// remain inside the inscribed circle. Returns 0 on rectangular panels. The canonical maths live in
// DisplayManager::roundScreenEdgeInset so screen layout in App can reuse it.
inline int circularSafeInsetForY(int y) { return DisplayManager::roundScreenEdgeInset(y); }

// Left/right safe x bounds for a horizontal span on the row at y (round-aware).
inline int circularSafeLeftX(int y) { return circularSafeInsetForY(y); }
inline int circularSafeRightX(int y) { return kVirtualBufferWidth - circularSafeInsetForY(y); }

constexpr size_t kBytesPerPixel = sizeof(uint16_t);
constexpr size_t kMaxChunkBytes = Board::Config::DISPLAY_TX_CHUNK_BYTES;
constexpr int kTxBufferWidth = kDisplayWidth > kPanelNativeWidth ? kDisplayWidth : kPanelNativeWidth;
// Boards that need a seam-free panel flush the whole frame in one address window, so the tx buffer
// spans the full height (allocated in PSRAM). Others keep the small DMA-internal strip buffer.
constexpr bool kFlushWholeFrame = Board::Config::DISPLAY_FLUSH_WHOLE_FRAME;
constexpr int kMaxChunkPhysicalRows =
    kFlushWholeFrame ? kPanelNativeHeight
                     : static_cast<int>(kMaxChunkBytes / (kTxBufferWidth * kBytesPerPixel));
static_assert(kMaxChunkPhysicalRows > 0, "Display chunk buffer must hold at least one row");

constexpr size_t kTxBufferPixels = static_cast<size_t>(kTxBufferWidth) * kMaxChunkPhysicalRows;

int logicalWidthForOrientation(Board::Config::UiOrientation orientation) {
  switch (orientation) {
    case Board::Config::UiOrientation::Portrait:
    case Board::Config::UiOrientation::PortraitFlipped:
      return kPanelNativeWidth;
    case Board::Config::UiOrientation::Landscape:
    case Board::Config::UiOrientation::LandscapeFlipped:
    default:
      return kDisplayWidth;
  }
}

int logicalHeightForOrientation(Board::Config::UiOrientation orientation) {
  switch (orientation) {
    case Board::Config::UiOrientation::Portrait:
    case Board::Config::UiOrientation::PortraitFlipped:
      return kPanelNativeHeight;
    case Board::Config::UiOrientation::Landscape:
    case Board::Config::UiOrientation::LandscapeFlipped:
    default:
      return kDisplayHeight;
  }
}

bool isPortraitOrientation(Board::Config::UiOrientation orientation) {
  return orientation == Board::Config::UiOrientation::Portrait ||
         orientation == Board::Config::UiOrientation::PortraitFlipped;
}

void mapPhysicalToLogical(Board::Config::UiOrientation orientation, int physicalX, int physicalY,
                          int &logicalX, int &logicalY) {
  switch (orientation) {
    case Board::Config::UiOrientation::Portrait:
      logicalX = physicalX;
      logicalY = physicalY;
      break;
    case Board::Config::UiOrientation::PortraitFlipped:
      logicalX = kPanelNativeWidth - 1 - physicalX;
      logicalY = kPanelNativeHeight - 1 - physicalY;
      break;
    case Board::Config::UiOrientation::Landscape:
      logicalX = kDisplayWidth - 1 - physicalY;
      logicalY = physicalX;
      break;
    case Board::Config::UiOrientation::LandscapeFlipped:
    default:
      logicalX = physicalY;
      logicalY = kDisplayHeight - 1 - physicalX;
      break;
  }
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
}

struct TinyGlyph {
  char c;
  uint8_t rows[kTinyGlyphHeight];
};

struct ReaderGlyph {
  ReaderGlyph() = default;
  ReaderGlyph(const uint8_t *bitmapPtr, int xOffsetValue, int widthValue, int xAdvanceValue,
              int heightValue)
      : bitmap(bitmapPtr),
        xOffset(xOffsetValue),
        width(widthValue),
        xAdvance(xAdvanceValue),
        height(heightValue) {}

  const uint8_t *bitmap = nullptr;
  int xOffset = 0;
  int width = 0;
  int xAdvance = 0;
  int height = 0;
};

DisplayManager::TypographyConfig &activeTypographyConfig() {
  static DisplayManager::TypographyConfig config;
  return config;
}

DisplayManager::ReaderTypeface sanitizeReaderTypeface(DisplayManager::ReaderTypeface typeface) {
  switch (typeface) {
    case DisplayManager::ReaderTypeface::Standard:
    case DisplayManager::ReaderTypeface::OpenDyslexic:
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return typeface;
  }
  return DisplayManager::ReaderTypeface::Standard;
}

int clampTypographyTracking(int value) {
  return std::max(kTypographyTrackingMin, std::min(kTypographyTrackingMax, value));
}

int clampTypographyAnchorPercent(int value) {
  return std::max(kTypographyAnchorMin, std::min(kTypographyAnchorMax, value));
}

int clampTypographyGuideHalfWidth(int value) {
  return std::max(kTypographyGuideHalfWidthMin, std::min(kTypographyGuideHalfWidthMax, value));
}

int clampTypographyGuideGap(int value) {
  return std::max(kTypographyGuideGapMin, std::min(kTypographyGuideGapMax, value));
}

int currentTypographyTrackingPx() {
  return clampTypographyTracking(activeTypographyConfig().trackingPx);
}

bool currentFocusHighlightEnabled() {
  return activeTypographyConfig().focusHighlight;
}

int currentAnchorPercent() {
  return clampTypographyAnchorPercent(activeTypographyConfig().anchorPercent);
}

int currentGuideHalfWidth() {
  return clampTypographyGuideHalfWidth(activeTypographyConfig().guideHalfWidth);
}

int currentGuideGap() {
  return clampTypographyGuideGap(activeTypographyConfig().guideGap);
}

DisplayManager::ReaderTypeface currentReaderTypeface() {
  return sanitizeReaderTypeface(activeTypographyConfig().typeface);
}

DisplayManager::ReaderTypeface effectiveReaderTypefaceForText(const String &) {
  return currentReaderTypeface();
}

bool invertedPunctuationBaseByte(uint8_t value, uint8_t &base) {
  switch (value) {
    case 0x16:
      base = static_cast<uint8_t>('!');
      return true;
    case 0x17:
      base = static_cast<uint8_t>('?');
      return true;
    default:
      return false;
  }
}

bool shouldDrawInvertedGlyph(char c) {
  uint8_t base = 0;
  return invertedPunctuationBaseByte(LatinText::byteValue(c), base);
}

String readerChromeKey(const DisplayManager::ReaderChrome &chrome) {
  return String(chrome.showBattery ? 1 : 0) + String(chrome.showChapter ? 1 : 0) +
         String(chrome.showProgress ? 1 : 0) +
         String(chrome.showPreviousSentenceHint ? 1 : 0) +
         String(chrome.showEdgeMenuHints ? 1 : 0) +
         String(chrome.swapPreviousSentenceAndBattery ? 1 : 0);
}

int baseGlyphHeightForTypeface(DisplayManager::ReaderTypeface typeface) {
  switch (typeface) {
    case DisplayManager::ReaderTypeface::OpenDyslexic:
      return kEmbeddedOpenDyslexicHeight;
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return kEmbeddedAtkinsonHeight;
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return kEmbeddedSerifHeight;
  }
}

int baseGlyphHeight() {
  return baseGlyphHeightForTypeface(currentReaderTypeface());
}

int mediumGlyphHeightForTypeface(DisplayManager::ReaderTypeface typeface) {
  switch (typeface) {
    case DisplayManager::ReaderTypeface::OpenDyslexic:
      return kEmbeddedOpenDyslexic70Height;
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible:
      return kEmbeddedAtkinson70Height;
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return kEmbeddedSerif70Height;
  }
}

int mediumGlyphHeight() {
  return mediumGlyphHeightForTypeface(currentReaderTypeface());
}

struct ReaderTextStyle {
  uint8_t scalePercent;
  int currentGap;
  uint8_t alpha;
};

constexpr TinyGlyph kTinyGlyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {'"', {0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00}},
    {'#', {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}},
    {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
    {'&', {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D}},
    {'\'', {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00}},
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    {'*', {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
    {'6', {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {';', {0x00, 0x0C, 0x0C, 0x00, 0x06, 0x04, 0x08}},
    {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
    {'<', {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01}},
    {'>', {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}},
    {'X', {0x11, 0x0A, 0x04, 0x04, 0x04, 0x0A, 0x11}},
    {'Y', {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}},
};

ReaderGlyph serifGlyphForByte(uint8_t value) {
  if (value < kEmbeddedSerifFirstChar || value > kEmbeddedSerifLastChar) {
    value = static_cast<uint8_t>('?');
  }
  const EmbeddedSerifGlyph &glyph = kEmbeddedSerifGlyphs[value - kEmbeddedSerifFirstChar];
  return {kEmbeddedSerifBitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width, glyph.xAdvance,
          kEmbeddedSerifHeight};
}

ReaderGlyph serif70GlyphForByte(uint8_t value) {
  if (value < kEmbeddedSerif70FirstChar || value > kEmbeddedSerif70LastChar) {
    value = static_cast<uint8_t>('?');
  }
  const EmbeddedSerif70Glyph &glyph = kEmbeddedSerif70Glyphs[value - kEmbeddedSerif70FirstChar];
  return {kEmbeddedSerif70Bitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
          glyph.xAdvance, kEmbeddedSerif70Height};
}

ReaderGlyph glyphFor(char c, DisplayManager::ReaderTypeface typeface) {
  const uint8_t value = LatinText::byteValue(c);
  uint8_t baseValue = 0;
  const uint8_t lookupValue = invertedPunctuationBaseByte(value, baseValue) ? baseValue : value;

  switch (typeface) {
    case DisplayManager::ReaderTypeface::OpenDyslexic: {
      const uint8_t glyphValue =
          (lookupValue >= kEmbeddedOpenDyslexicFirstChar &&
           lookupValue <= kEmbeddedOpenDyslexicLastChar)
              ? lookupValue
              : LatinText::fallbackAsciiByte(lookupValue);
      const EmbeddedOpenDyslexicGlyph &glyph =
          kEmbeddedOpenDyslexicGlyphs[glyphValue - kEmbeddedOpenDyslexicFirstChar];
      return {kEmbeddedOpenDyslexicBitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
              glyph.xAdvance, kEmbeddedOpenDyslexicHeight};
    }
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible: {
      const uint8_t glyphValue =
          (lookupValue >= kEmbeddedAtkinsonFirstChar && lookupValue <= kEmbeddedAtkinsonLastChar)
              ? lookupValue
              : LatinText::fallbackAsciiByte(lookupValue);
      const EmbeddedAtkinsonGlyph &glyph =
          kEmbeddedAtkinsonGlyphs[glyphValue - kEmbeddedAtkinsonFirstChar];
      return {kEmbeddedAtkinsonBitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
              glyph.xAdvance, kEmbeddedAtkinsonHeight};
    }
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return serifGlyphForByte(lookupValue);
  }
}

ReaderGlyph glyphFor(char c) { return glyphFor(c, currentReaderTypeface()); }

ReaderGlyph glyph70For(char c, DisplayManager::ReaderTypeface typeface) {
  const uint8_t value = LatinText::byteValue(c);
  uint8_t baseValue = 0;
  const uint8_t lookupValue = invertedPunctuationBaseByte(value, baseValue) ? baseValue : value;

  switch (typeface) {
    case DisplayManager::ReaderTypeface::OpenDyslexic: {
      const uint8_t glyphValue =
          (lookupValue >= kEmbeddedOpenDyslexic70FirstChar &&
           lookupValue <= kEmbeddedOpenDyslexic70LastChar)
              ? lookupValue
              : LatinText::fallbackAsciiByte(lookupValue);
      const EmbeddedOpenDyslexic70Glyph &glyph =
          kEmbeddedOpenDyslexic70Glyphs[glyphValue - kEmbeddedOpenDyslexic70FirstChar];
      return {kEmbeddedOpenDyslexic70Bitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
              glyph.xAdvance, kEmbeddedOpenDyslexic70Height};
    }
    case DisplayManager::ReaderTypeface::AtkinsonHyperlegible: {
      const uint8_t glyphValue =
          (lookupValue >= kEmbeddedAtkinson70FirstChar &&
           lookupValue <= kEmbeddedAtkinson70LastChar)
              ? lookupValue
              : LatinText::fallbackAsciiByte(lookupValue);
      const EmbeddedAtkinson70Glyph &glyph =
          kEmbeddedAtkinson70Glyphs[glyphValue - kEmbeddedAtkinson70FirstChar];
      return {kEmbeddedAtkinson70Bitmaps + glyph.bitmapOffset, glyph.xOffset, glyph.width,
              glyph.xAdvance, kEmbeddedAtkinson70Height};
    }
    case DisplayManager::ReaderTypeface::Standard:
    default:
      return serif70GlyphForByte(lookupValue);
  }
}

ReaderGlyph glyph70For(char c) { return glyph70For(c, currentReaderTypeface()); }

const uint8_t *tinyRowsFor(char c) {
  uint8_t value = LatinText::byteValue(c);
  for (int pass = 0; pass < 2; ++pass) {
    char lookup = static_cast<char>(value);
    if (lookup >= 'a' && lookup <= 'z') {
      lookup = static_cast<char>(lookup - 'a' + 'A');
    }

    for (const TinyGlyph &glyph : kTinyGlyphs) {
      if (glyph.c == lookup) {
        return glyph.rows;
      }
    }

    const uint8_t fallback = LatinText::fallbackAsciiByte(value);
    if (fallback == value) {
      break;
    }
    value = fallback;
  }

  for (const TinyGlyph &glyph : kTinyGlyphs) {
    if (glyph.c == '?') {
      return glyph.rows;
    }
  }

  return kTinyGlyphs[0].rows;
}

uint16_t panelColor(uint16_t rgb565) {
  return static_cast<uint16_t>((rgb565 << 8) | (rgb565 >> 8));
}

bool packedLifeCellAlive(const std::vector<uint32_t> &cells, size_t index) {
  const size_t word = index / 32U;
  if (word >= cells.size()) {
    return false;
  }
  return (cells[word] & (1UL << (index % 32U))) != 0;
}

bool isWordCharacter(char c) { return LatinText::isWordCharacter(LatinText::byteValue(c)); }

int scaledAdvance(int value, int divisor) {
  divisor = std::max(1, divisor);
  return std::max(1, (value + divisor - 1) / divisor);
}

int scaledSignedAdvance(int value, int divisor) {
  divisor = std::max(1, divisor);
  if (value >= 0) {
    return value / divisor;
  }
  return -(((-value) + divisor - 1) / divisor);
}

int scaledPercentDimension(int value, uint8_t scalePercent) {
  if (scalePercent == 0) {
    scalePercent = 1;
  }
  return std::max(1, (value * static_cast<int>(scalePercent) + 99) / 100);
}

int scaledSignedPercent(int value, uint8_t scalePercent) {
  if (scalePercent == 0) {
    scalePercent = 1;
  }
  if (value >= 0) {
    return (value * static_cast<int>(scalePercent) + 50) / 100;
  }
  return -(((-value) * static_cast<int>(scalePercent) + 50) / 100);
}

int trackedAdvance(int advance, size_t index, size_t length) {
  if (index + 1 >= length) {
    return advance;
  }
  return std::max(1, advance + currentTypographyTrackingPx());
}

int trackedAdvanceScaled(int advance, int divisor, size_t index, size_t length) {
  const int scaled = scaledAdvance(advance, divisor);
  if (index + 1 >= length) {
    return scaled;
  }
  return std::max(1, scaled + scaledSignedAdvance(currentTypographyTrackingPx(), divisor));
}

int trackedAdvanceScaledPercent(int advance, uint8_t scalePercent, size_t index, size_t length) {
  const int scaled = scaledPercentDimension(advance, scalePercent);
  if (index + 1 >= length) {
    return scaled;
  }
  return std::max(1, scaled + scaledSignedPercent(currentTypographyTrackingPx(), scalePercent));
}

int opticalKerningAdjustment(char currentChar, char nextChar, int currentXOffset, int currentWidth,
                             int trackedAdvanceValue, int nextXOffset, int desiredGap) {
  if (!isWordCharacter(currentChar) || !isWordCharacter(nextChar) || currentWidth <= 0) {
    return 0;
  }

  desiredGap = std::max(1, desiredGap);
  const int visibleGap =
      trackedAdvanceValue + nextXOffset - (currentXOffset + currentWidth);
  if (visibleGap <= desiredGap) {
    return 0;
  }

  return std::min(visibleGap - desiredGap, std::max(0, trackedAdvanceValue - 1));
}

int regularDesiredGap() { return std::max(1, kOpticalLetterGapPx + currentTypographyTrackingPx()); }

int scaledDesiredGap(int divisor) {
  return std::max(1, scaledAdvance(kOpticalLetterGapPx, divisor) +
                         scaledSignedAdvance(currentTypographyTrackingPx(), divisor));
}

int scaledPercentDesiredGap(uint8_t scalePercent) {
  return std::max(1, scaledPercentDimension(kOpticalLetterGapPx, scalePercent) +
                         scaledSignedPercent(currentTypographyTrackingPx(), scalePercent));
}

struct TextLayoutMetrics {
  int minX = 0;
  int maxX = 0;
  int focusCenterX = 0;
  bool hasPixels = false;
};

void updateTextLayoutBounds(TextLayoutMetrics &layout, int left, int width) {
  if (width <= 0) {
    return;
  }

  const int right = left + width;
  if (!layout.hasPixels) {
    layout.minX = left;
    layout.maxX = right;
    layout.hasPixels = true;
    return;
  }

  layout.minX = std::min(layout.minX, left);
  layout.maxX = std::max(layout.maxX, right);
}

int textLayoutWidth(const TextLayoutMetrics &layout) {
  if (!layout.hasPixels) {
    return 0;
  }
  return std::max(0, layout.maxX - layout.minX);
}

TextLayoutMetrics serifWordLayout(const String &word, int focusIndex, int divisor = 1) {
  TextLayoutMetrics layout;
  int cursorX = 0;
  const bool trackFocus = focusIndex >= 0;
  const DisplayManager::ReaderTypeface typeface = effectiveReaderTypefaceForText(word);

  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    const int xOffset = scaledSignedAdvance(glyph.xOffset, divisor);
    const int width = glyph.width == 0 ? 0 : scaledAdvance(glyph.width, divisor);
    const int advance = scaledAdvance(glyph.xAdvance, divisor);
    const int left = cursorX + xOffset;
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked = trackedAdvanceScaled(glyph.xAdvance, divisor, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(nextGlyph.xOffset, divisor), scaledDesiredGap(divisor));
    }
    cursorX += std::max(1, tracked);
  }

  if (!trackFocus && layout.hasPixels) {
    layout.focusCenterX = layout.minX + (textLayoutWidth(layout) / 2);
  }

  return layout;
}

TextLayoutMetrics serifWordLayoutScaledPercent(const String &word, int focusIndex,
                                               uint8_t scalePercent) {
  TextLayoutMetrics layout;
  int cursorX = 0;
  const bool trackFocus = focusIndex >= 0;
  const DisplayManager::ReaderTypeface typeface = effectiveReaderTypefaceForText(word);

  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    const int xOffset = scaledSignedPercent(glyph.xOffset, scalePercent);
    const int width = glyph.width == 0 ? 0 : scaledPercentDimension(glyph.width, scalePercent);
    const int advance = scaledPercentDimension(glyph.xAdvance, scalePercent);
    const int left = cursorX + xOffset;
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked = trackedAdvanceScaledPercent(glyph.xAdvance, scalePercent, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedPercent(nextGlyph.xOffset, scalePercent),
          scaledPercentDesiredGap(scalePercent));
    }
    cursorX += std::max(1, tracked);
  }

  if (!trackFocus && layout.hasPixels) {
    layout.focusCenterX = layout.minX + (textLayoutWidth(layout) / 2);
  }

  return layout;
}

TextLayoutMetrics serif70WordLayout(const String &word, int focusIndex) {
  TextLayoutMetrics layout;
  int cursorX = 0;
  const bool trackFocus = focusIndex >= 0;
  const DisplayManager::ReaderTypeface typeface = effectiveReaderTypefaceForText(word);

  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyph70For(word[i], typeface);
    const int left = cursorX + glyph.xOffset;
    const int width = glyph.width;
    const int advance = glyph.xAdvance;
    updateTextLayoutBounds(layout, left, width);

    if (trackFocus && static_cast<int>(i) == focusIndex) {
      layout.focusCenterX = width > 0 ? left + (width / 2) : cursorX + (advance / 2);
    }

    int tracked = trackedAdvance(advance, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyph70For(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], glyph.xOffset, width, tracked,
                                          nextGlyph.xOffset,
                                          regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }

  if (!trackFocus && layout.hasPixels) {
    layout.focusCenterX = layout.minX + (textLayoutWidth(layout) / 2);
  }

  return layout;
}

int serifWordWidth(const String &word) { return textLayoutWidth(serifWordLayout(word, -1)); }

int scaledWordWidth(const String &word, int divisor) {
  return textLayoutWidth(serifWordLayout(word, -1, divisor));
}

int scaledWordWidthPercent(const String &word, uint8_t scalePercent) {
  return textLayoutWidth(serifWordLayoutScaledPercent(word, -1, scalePercent));
}

ReaderTextStyle readerTextStyle(uint8_t fontSizeLevel) {
  static constexpr ReaderTextStyle kStyles[] = {
      {100, kPhantomCurrentGapLarge, kPhantomAlphaLarge},
      {70, kPhantomCurrentGapMedium, kPhantomAlphaMedium},
      {50, kPhantomCurrentGapSmall, kPhantomAlphaSmall},
      // Index 3: "Extra Large" -- bigger than the previous max. Routed through the scale-aware
      // default render path (it is not index 1 or 2), so it renders at 120% with the large layout.
      {120, kPhantomCurrentGapLarge, kPhantomAlphaLarge},
  };

  const size_t styleCount = sizeof(kStyles) / sizeof(kStyles[0]);
  if (fontSizeLevel >= styleCount) {
    fontSizeLevel = 0;
  }
  return kStyles[fontSizeLevel];
}

int orpOrdinalForLength(int length) {
  if (length <= 1) {
    return 0;
  }
  if (length <= 5) {
    return 1;
  }
  if (length <= 9) {
    return 2;
  }
  if (length <= 13) {
    return 3;
  }
  return 4;
}

int findFocusLetterIndex(const String &word) {
  int wordCharacterCount = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (isWordCharacter(word[i])) {
      ++wordCharacterCount;
    }
  }

  if (wordCharacterCount == 0) {
    return word.length() > 0 ? 0 : -1;
  }

  const int targetOrdinal = std::min(orpOrdinalForLength(wordCharacterCount), wordCharacterCount - 1);
  int currentOrdinal = 0;
  for (size_t i = 0; i < word.length(); ++i) {
    if (!isWordCharacter(word[i])) {
      continue;
    }
    if (currentOrdinal == targetOrdinal) {
      return static_cast<int>(i);
    }
    ++currentOrdinal;
  }

  return 0;
}

int rsvpStartX(const String &word, int focusIndex, int virtualWidth, int divisor = 1,
               bool clampToMargins = true) {
  const TextLayoutMetrics layout = serifWordLayout(word, focusIndex, divisor);
  const int wordWidth = textLayoutWidth(layout);
  if (focusIndex < 0) {
    return ((virtualWidth - wordWidth) / 2) - layout.minX;
  }

  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const int x = anchorX - layout.focusCenterX;
  if (!clampToMargins) {
    return x;
  }
  const int minX = kRsvpSideMargin - layout.minX;
  const int maxX = virtualWidth - kRsvpSideMargin - layout.maxX;

  if (maxX < minX) {
    return x;
  }

  return std::max(minX, std::min(maxX, x));
}

int rsvpStartXScaledPercent(const String &word, int focusIndex, int virtualWidth,
                            uint8_t scalePercent, bool clampToMargins = true) {
  const TextLayoutMetrics layout = serifWordLayoutScaledPercent(word, focusIndex, scalePercent);
  const int wordWidth = textLayoutWidth(layout);
  if (focusIndex < 0) {
    return ((virtualWidth - wordWidth) / 2) - layout.minX;
  }

  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const int x = anchorX - layout.focusCenterX;
  if (!clampToMargins) {
    return x;
  }
  const int minX = kRsvpSideMargin - layout.minX;
  const int maxX = virtualWidth - kRsvpSideMargin - layout.maxX;

  if (maxX < minX) {
    return x;
  }

  return std::max(minX, std::min(maxX, x));
}

int serif70WordWidth(const String &word) { return textLayoutWidth(serif70WordLayout(word, -1)); }

int rsvpStartX70(const String &word, int focusIndex, int virtualWidth, bool clampToMargins = true) {
  const TextLayoutMetrics layout = serif70WordLayout(word, focusIndex);
  const int wordWidth = textLayoutWidth(layout);
  if (focusIndex < 0) {
    return ((virtualWidth - wordWidth) / 2) - layout.minX;
  }

  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const int x = anchorX - layout.focusCenterX;
  if (!clampToMargins) {
    return x;
  }

  const int minX = kRsvpSideMargin - layout.minX;
  const int maxX = virtualWidth - kRsvpSideMargin - layout.maxX;
  if (maxX < minX) {
    return x;
  }

  return std::max(minX, std::min(maxX, x));
}

}  // namespace

int DisplayManager::roundScreenEdgeInset(int y) {
  if (!Board::Config::DISPLAY_IS_ROUND) {
    return 0;
  }
  const float radius = static_cast<float>(kVirtualBufferWidth) * 0.5f;
  const float center = static_cast<float>(kVirtualBufferHeight) * 0.5f;
  const float dy = static_cast<float>(y) - center;
  if (dy <= -radius || dy >= radius) {
    return static_cast<int>(radius);
  }
  const float halfChord = sqrtf(radius * radius - dy * dy);
  const int inset = static_cast<int>(radius - halfChord) + kCircularSafetyPaddingPx;
  return inset < 0 ? 0 : inset;
}

static const char *kDisplayTag = "display";

DisplayManager::~DisplayManager() {
  if (virtualFrame_ != nullptr) {
    heap_caps_free(virtualFrame_);
    virtualFrame_ = nullptr;
  }

  if (txBuffer_ != nullptr) {
    heap_caps_free(txBuffer_);
    txBuffer_ = nullptr;
  }
}

void DisplayManager::setBatteryLabel(const String &label) {
  if (batteryLabel_ == label) {
    return;
  }

  batteryLabel_ = label;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setBatteryCharging(bool charging) {
  if (batteryCharging_ == charging) {
    return;
  }

  batteryCharging_ = charging;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setClockLabel(const String &label) {
  if (clockLabel_ == label) {
    return;
  }

  clockLabel_ = label;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setBrightnessOverlay(const String &text) {
  if (brightnessOverlayText_ == text) {
    return;
  }

  brightnessOverlayText_ = text;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setBrightnessPercent(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  brightnessPercent_ = percent;
  if (initialized_) {
    applyBrightness();
  }
}

void DisplayManager::setDarkMode(bool darkMode) {
  if (darkMode_ == darkMode) {
    return;
  }

  darkMode_ = darkMode;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setNightMode(bool nightMode) {
  if (nightMode_ == nightMode) {
    return;
  }

  nightMode_ = nightMode;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setYellowMode(bool enabled) {
  if (yellowMode_ == enabled) {
    return;
  }

  yellowMode_ = enabled;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setThemePalette(ThemePalette palette) {
  if (themePalette_ == palette) {
    return;
  }

  themePalette_ = palette;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

DisplayManager::ThemePalette DisplayManager::themePalette() const { return themePalette_; }

void DisplayManager::setUiOrientation(Board::Config::UiOrientation orientation) {
  if (uiOrientation_ == orientation) {
    return;
  }

  uiOrientation_ = orientation;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

void DisplayManager::setUiRotated180(bool rotated180) {
  setUiOrientation(rotated180 ? Board::Config::ROTATED_UI_ORIENTATION
                              : Board::Config::DEFAULT_UI_ORIENTATION);
}

void DisplayManager::setTypographyConfig(const TypographyConfig &config) {
  TypographyConfig next;
  next.typeface = sanitizeReaderTypeface(config.typeface);
  next.focusHighlight = config.focusHighlight;
  next.trackingPx = static_cast<int8_t>(clampTypographyTracking(config.trackingPx));
  next.anchorPercent = static_cast<uint8_t>(clampTypographyAnchorPercent(config.anchorPercent));
  next.guideHalfWidth =
      static_cast<uint8_t>(clampTypographyGuideHalfWidth(config.guideHalfWidth));
  next.guideGap = static_cast<uint8_t>(clampTypographyGuideGap(config.guideGap));

  TypographyConfig &current = activeTypographyConfig();
  if (current.typeface == next.typeface && current.focusHighlight == next.focusHighlight &&
      current.trackingPx == next.trackingPx &&
      current.anchorPercent == next.anchorPercent &&
      current.guideHalfWidth == next.guideHalfWidth && current.guideGap == next.guideGap) {
    return;
  }

  current = next;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

DisplayManager::TypographyConfig DisplayManager::typographyConfig() const {
  return activeTypographyConfig();
}

bool DisplayManager::darkMode() const { return darkMode_; }

bool DisplayManager::nightMode() const { return nightMode_; }

bool DisplayManager::begin() {
  ESP_LOGI(kDisplayTag, "Begin");

  if (!allocateBuffers()) {
    ESP_LOGE(kDisplayTag, "Buffer allocation failed");
    return false;
  }
  ESP_LOGI(kDisplayTag, "Buffers ready");

  if (!initPanel()) {
    ESP_LOGE(kDisplayTag, "Panel init failed");
    return false;
  }

  initialized_ = true;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
  fillScreen(backgroundColor());
  applyBrightness();
  ESP_LOGI(kDisplayTag, "Display initialized for %s", Board::Config::BOARD_LABEL);
  return true;
}

void DisplayManager::prepareForSleep() {
  if (!initialized_) {
    return;
  }

  fillScreen(kTrueBlack);
  Board::Display::sleep();
  initialized_ = false;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
}

bool DisplayManager::wakeFromSleep() {
  if (!allocateBuffers()) {
    ESP_LOGE(kDisplayTag, "Buffer allocation failed after wake");
    return false;
  }

  Board::Display::wake();
  initialized_ = true;
  tickerPlaybackFrameActive_ = false;
  lastRenderKey_ = "";
  applyBrightness();
  return true;
}

bool DisplayManager::allocateBuffers() {
  if (virtualFrame_ == nullptr) {
    virtualFrame_ = static_cast<uint16_t *>(
        heap_caps_malloc(kVirtualBufferWidth * kVirtualBufferHeight * sizeof(uint16_t),
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (virtualFrame_ == nullptr) {
      virtualFrame_ = static_cast<uint16_t *>(
          heap_caps_malloc(kVirtualBufferWidth * kVirtualBufferHeight * sizeof(uint16_t),
                           MALLOC_CAP_8BIT));
    }
  }

  if (txBuffer_ == nullptr) {
    txBufferBytes_ = kTxBufferPixels * sizeof(uint16_t);
    // A full-frame buffer is too large for internal DMA RAM, so place it in PSRAM; the CO5300
    // driver bounces each DMA chunk through an internal buffer when the source is PSRAM. Strip
    // buffers stay in internal DMA RAM as before.
    if (kFlushWholeFrame) {
      txBuffer_ = static_cast<uint16_t *>(
          heap_caps_malloc(txBufferBytes_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (txBuffer_ == nullptr) {
      txBuffer_ = static_cast<uint16_t *>(
          heap_caps_malloc(txBufferBytes_, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    }
  }

  return virtualFrame_ != nullptr && txBuffer_ != nullptr;
}

bool DisplayManager::initPanel() {
  if (!Board::Display::begin()) {
    return false;
  }
  ESP_LOGI(kDisplayTag, "Panel init sequence complete");
  return true;
}

bool DisplayManager::drawBitmap(int xStart, int yStart, int xEnd, int yEnd, const void *colorData) {
  if (colorData == nullptr || xEnd <= xStart || yEnd <= yStart) {
    return false;
  }

  return Board::Display::pushColors(static_cast<uint16_t>(xStart),
                                    static_cast<uint16_t>(yStart),
                                    static_cast<uint16_t>(xEnd - xStart),
                                    static_cast<uint16_t>(yEnd - yStart),
                                    static_cast<const uint16_t *>(colorData));
}

void DisplayManager::fillScreen(uint16_t color) {
  if (txBuffer_ == nullptr) {
    return;
  }

  const size_t pixelsPerChunk = static_cast<size_t>(kPanelNativeWidth) * kMaxChunkPhysicalRows;
  for (size_t i = 0; i < pixelsPerChunk; ++i) {
    txBuffer_[i] = panelColor(color);
  }

  for (int yStart = 0; yStart < kPanelNativeHeight; yStart += kMaxChunkPhysicalRows) {
    const int rows = std::min(kMaxChunkPhysicalRows, kPanelNativeHeight - yStart);
    if (!drawBitmap(0, yStart, kPanelNativeWidth, yStart + rows, txBuffer_)) {
      return;
    }
  }
}

void DisplayManager::clearVirtualBuffer(int width, int height) {
  const uint16_t background = panelColor(backgroundColor());
  for (int row = 0; row < height; ++row) {
    std::fill_n(virtualFrame_ + row * kVirtualBufferWidth, width, background);
  }
}

uint16_t DisplayManager::backgroundColor() const {
  switch (themePalette_) {
    case ThemePalette::Terracotta:
      return kTerracottaBackground;
    case ThemePalette::Peach:
      return kPeachBackground;
    case ThemePalette::Olive:
      return kOliveBackground;
    case ThemePalette::Sage:
    case ThemePalette::WarmGold:
    case ThemePalette::BeigeRose:
      return kTrueBlack;
    case ThemePalette::None:
      break;
  }
  if (nightMode_) {
    return kTrueBlack;
  }
  if (yellowMode_) {
    return kYellowModeBackground;
  }
  return darkMode_ ? kTrueBlack : kPureWhite;
}

uint16_t DisplayManager::wordColor() const {
  switch (themePalette_) {
    case ThemePalette::Terracotta:
      return kTerracottaWordColor;
    case ThemePalette::Peach:
      return kPeachWordColor;
    case ThemePalette::Olive:
      return kOliveWordColor;
    case ThemePalette::Sage:
      return kSageWordColor;
    case ThemePalette::WarmGold:
      return kWarmGoldWordColor;
    case ThemePalette::BeigeRose:
      return kBeigeRoseWordColor;
    case ThemePalette::None:
      break;
  }
  if (nightMode_) {
    return kNightWordColor;
  }
  if (yellowMode_) {
    return kLightWordColor;
  }
  return darkMode_ ? kDarkWordColor : kLightWordColor;
}

uint16_t DisplayManager::focusColor() const {
  switch (themePalette_) {
    case ThemePalette::Terracotta:
      return kTerracottaFocusColor;
    case ThemePalette::Peach:
      return kPeachFocusColor;
    case ThemePalette::Olive:
      return kOliveFocusColor;
    case ThemePalette::Sage:
      return kSageFocusColor;
    case ThemePalette::WarmGold:
      return kWarmGoldFocusColor;
    case ThemePalette::BeigeRose:
      return kBeigeRoseFocusColor;
    case ThemePalette::None:
      break;
  }
  if (yellowMode_) {
    return nightMode_ ? kYellowModeNightFocusColor : kYellowModeFocusColor;
  }
  if (nightMode_) {
    return kNightFocusColor;
  }
  return kFocusLetterColor;
}

uint16_t DisplayManager::dimColor() const {
  if (themePalette_ != ThemePalette::None) {
    // Derive a muted label color by blending the body text into the pastel bg.
    return blendOverBackground(wordColor(), 150);
  }
  if (nightMode_) {
    return blendOverBackground(wordColor(), kNightDimAlpha);
  }
  if (yellowMode_) {
    return kLightMenuDimColor;
  }
  return darkMode_ ? kDarkMenuDimColor : kLightMenuDimColor;
}

uint16_t DisplayManager::footerColor() const {
  if (themePalette_ != ThemePalette::None) {
    return blendOverBackground(wordColor(), 120);
  }
  if (nightMode_) {
    return blendOverBackground(wordColor(), kNightFooterAlpha);
  }
  if (yellowMode_) {
    return kLightFooterColor;
  }
  return darkMode_ ? kDarkFooterColor : kLightFooterColor;
}

uint16_t DisplayManager::selectedBarColor() const {
  if (themePalette_ != ThemePalette::None) {
    return focusColor();
  }
  return nightMode_ ? focusColor() : kFocusLetterColor;
}

uint16_t DisplayManager::focusTimerBreakColor() const {
  return nightMode_ ? rgb565(112, 176, 126) : rgb565(68, 132, 88);
}

uint16_t DisplayManager::blendOverBackground(uint16_t rgb565, uint8_t alpha) const {
  if (alpha >= 250) {
    return rgb565;
  }

  const uint16_t bg = backgroundColor();
  const uint32_t inverseAlpha = 255U - alpha;
  const uint32_t r =
      ((((rgb565 >> 11) & 0x1F) * alpha) + (((bg >> 11) & 0x1F) * inverseAlpha)) / 255U;
  const uint32_t g =
      ((((rgb565 >> 5) & 0x3F) * alpha) + (((bg >> 5) & 0x3F) * inverseAlpha)) / 255U;
  const uint32_t b = (((rgb565 & 0x1F) * alpha) + ((bg & 0x1F) * inverseAlpha)) / 255U;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

int DisplayManager::logicalWidth() const { return logicalWidthForOrientation(uiOrientation_); }

int DisplayManager::logicalHeight() const { return logicalHeightForOrientation(uiOrientation_); }

int DisplayManager::chooseTextScale(const String &word) const {
  const int usableWidth = std::max(1, measureTextWidth(word));
  const int maxScaleX = kDisplayWidth / usableWidth;
  const int maxScaleY = kDisplayHeight / baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
  const int maxScale = std::min(kMaxTextScale, std::min(maxScaleX, maxScaleY));
  return std::max(1, maxScale);
}

int DisplayManager::measureTextWidth(const String &word) const {
  return textLayoutWidth(serifWordLayout(word, -1));
}

int DisplayManager::measureSerifTextWidth(const String &text, int divisor) const {
  return textLayoutWidth(serifWordLayout(text, -1, divisor));
}

int DisplayManager::measureSerif70TextWidth(const String &text) const {
  return textLayoutWidth(serif70WordLayout(text, -1));
}

int DisplayManager::measureSerifTextWidthScaled(const String &text, uint8_t scalePercent) const {
  return textLayoutWidth(serifWordLayoutScaledPercent(text, -1, scalePercent));
}

int DisplayManager::measureTinyTextWidth(const String &text, int scale) const {
  if (text.isEmpty()) {
    return 0;
  }
  return static_cast<int>(text.length()) * (kTinyGlyphWidth + kTinyGlyphSpacing) * scale -
         kTinyGlyphSpacing * scale;
}

String DisplayManager::fitSerifText(const String &text, int maxWidth, int divisor) const {
  if (measureSerifTextWidth(text, divisor) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() && measureSerifTextWidth(fitted + ellipsis, divisor) > maxWidth) {
    fitted.remove(fitted.length() - 1);
  }
  while (!fitted.isEmpty() && fitted[fitted.length() - 1] == ' ') {
    fitted.remove(fitted.length() - 1, 1);
  }
  return fitted.isEmpty() ? ellipsis : fitted + ellipsis;
}

String DisplayManager::fitSerifTextScaled(const String &text, int maxWidth,
                                          uint8_t scalePercent) const {
  if (measureSerifTextWidthScaled(text, scalePercent) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() &&
         measureSerifTextWidthScaled(fitted + ellipsis, scalePercent) > maxWidth) {
    fitted.remove(fitted.length() - 1);
  }
  while (!fitted.isEmpty() && fitted[fitted.length() - 1] == ' ') {
    fitted.remove(fitted.length() - 1, 1);
  }
  return fitted.isEmpty() ? ellipsis : fitted + ellipsis;
}

String DisplayManager::fitSerifTextTrailingScaled(const String &text, int maxWidth,
                                                  uint8_t scalePercent) const {
  if (measureSerifTextWidthScaled(text, scalePercent) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() &&
         measureSerifTextWidthScaled(ellipsis + fitted, scalePercent) > maxWidth) {
    fitted.remove(0, 1);
  }
  while (!fitted.isEmpty() && fitted[0] == ' ') {
    fitted.remove(0, 1);
  }
  return fitted.isEmpty() ? ellipsis : ellipsis + fitted;
}

String DisplayManager::fitTinyText(const String &text, int maxWidth, int scale) const {
  if (measureTinyTextWidth(text, scale) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() && measureTinyTextWidth(fitted + ellipsis, scale) > maxWidth) {
    fitted.remove(fitted.length() - 1);
  }
  while (!fitted.isEmpty() && fitted[fitted.length() - 1] == ' ') {
    fitted.remove(fitted.length() - 1, 1);
  }
  return fitted.isEmpty() ? ellipsis : fitted + ellipsis;
}

String DisplayManager::fitTinyTextTrailing(const String &text, int maxWidth, int scale) const {
  if (measureTinyTextWidth(text, scale) <= maxWidth) {
    return text;
  }

  String fitted = text;
  const String ellipsis = "...";
  while (!fitted.isEmpty() && measureTinyTextWidth(ellipsis + fitted, scale) > maxWidth) {
    fitted.remove(0, 1);
  }
  while (!fitted.isEmpty() && fitted[0] == ' ') {
    fitted.remove(0, 1);
  }
  return fitted.isEmpty() ? ellipsis : ellipsis + fitted;
}

void DisplayManager::drawGlyph(int x, int y, char c, uint16_t color) {
  drawGlyph(x, y, c, color, currentReaderTypeface());
}

void DisplayManager::drawGlyph(int x, int y, char c, uint16_t color, ReaderTypeface typeface) {
  const ReaderGlyph glyph = glyphFor(c, typeface);
  if (glyph.width == 0) {
    return;
  }
  const bool invert = shouldDrawInvertedGlyph(c);

  for (int row = 0; row < glyph.height; ++row) {
    const int dstY = y + row;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    for (int col = 0; col < glyph.width; ++col) {
      const int dstX = x + col;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const int sourceRow = invert ? glyph.height - 1 - row : row;
      const int sourceCol = invert ? glyph.width - 1 - col : col;
      const uint8_t alpha = glyph.bitmap[sourceRow * glyph.width + sourceCol];
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::drawSerifGlyphScaled(int x, int y, char c, uint16_t color, int divisor) {
  drawSerifGlyphScaled(x, y, c, color, divisor, currentReaderTypeface());
}

void DisplayManager::drawSerifGlyphScaled(int x, int y, char c, uint16_t color, int divisor,
                                          ReaderTypeface typeface) {
  divisor = std::max(1, divisor);
  const ReaderGlyph glyph = glyphFor(c, typeface);
  if (glyph.width == 0) {
    return;
  }
  const bool invert = shouldDrawInvertedGlyph(c);

  const int glyphHeight = glyph.height;
  const int scaledWidth = std::max(1, (glyph.width + divisor - 1) / divisor);
  const int scaledHeight = std::max(1, (glyphHeight + divisor - 1) / divisor);

  for (int dstRow = 0; dstRow < scaledHeight; ++dstRow) {
    const int dstY = y + dstRow;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    const int sourceYStart = dstRow * divisor;
    const int sourceYEnd = std::min(glyphHeight, sourceYStart + divisor);
    for (int dstCol = 0; dstCol < scaledWidth; ++dstCol) {
      const int dstX = x + dstCol;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const int sourceXStart = dstCol * divisor;
      const int sourceXEnd = std::min(static_cast<int>(glyph.width), sourceXStart + divisor);
      uint32_t alphaSum = 0;
      uint32_t sampleCount = 0;
      for (int sourceY = sourceYStart; sourceY < sourceYEnd; ++sourceY) {
        for (int sourceX = sourceXStart; sourceX < sourceXEnd; ++sourceX) {
          const int lookupY = invert ? glyphHeight - 1 - sourceY : sourceY;
          const int lookupX = invert ? glyph.width - 1 - sourceX : sourceX;
          alphaSum += glyph.bitmap[lookupY * glyph.width + lookupX];
          ++sampleCount;
        }
      }

      const uint8_t alpha =
          sampleCount == 0 ? 0 : static_cast<uint8_t>(alphaSum / sampleCount);
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::drawSerif70Glyph(int x, int y, char c, uint16_t color) {
  drawSerif70Glyph(x, y, c, color, currentReaderTypeface());
}

void DisplayManager::drawSerif70Glyph(int x, int y, char c, uint16_t color, ReaderTypeface typeface) {
  const ReaderGlyph glyph = glyph70For(c, typeface);
  if (glyph.width == 0) {
    return;
  }
  const bool invert = shouldDrawInvertedGlyph(c);

  for (int row = 0; row < glyph.height; ++row) {
    const int dstY = y + row;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    for (int col = 0; col < glyph.width; ++col) {
      const int dstX = x + col;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const int sourceRow = invert ? glyph.height - 1 - row : row;
      const int sourceCol = invert ? glyph.width - 1 - col : col;
      const uint8_t alpha = glyph.bitmap[sourceRow * glyph.width + sourceCol];
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::drawSerifGlyphScaledPercent(int x, int y, char c, uint16_t color,
                                                 uint8_t scalePercent) {
  drawSerifGlyphScaledPercent(x, y, c, color, scalePercent, currentReaderTypeface());
}

void DisplayManager::drawSerifGlyphScaledPercent(int x, int y, char c, uint16_t color,
                                                 uint8_t scalePercent,
                                                 ReaderTypeface typeface) {
  if (scalePercent >= 100) {
    drawGlyph(x, y, c, color, typeface);
    return;
  }

  const ReaderGlyph glyph = glyphFor(c, typeface);
  if (glyph.width == 0) {
    return;
  }
  const bool invert = shouldDrawInvertedGlyph(c);

  const int glyphHeight = glyph.height;
  const int scaledWidth = scaledPercentDimension(glyph.width, scalePercent);
  const int scaledHeight = scaledPercentDimension(glyphHeight, scalePercent);

  for (int dstRow = 0; dstRow < scaledHeight; ++dstRow) {
    const int dstY = y + dstRow;
    if (dstY < 0 || dstY >= kVirtualBufferHeight) {
      continue;
    }

    const int sourceYStart = (dstRow * glyphHeight) / scaledHeight;
    const int sourceYEnd =
        std::min(glyphHeight, ((dstRow + 1) * glyphHeight + scaledHeight - 1) / scaledHeight);
    for (int dstCol = 0; dstCol < scaledWidth; ++dstCol) {
      const int dstX = x + dstCol;
      if (dstX < 0 || dstX >= kVirtualBufferWidth) {
        continue;
      }

      const int sourceXStart = (dstCol * glyph.width) / scaledWidth;
      const int sourceXEnd =
          std::min(static_cast<int>(glyph.width),
                   ((dstCol + 1) * glyph.width + scaledWidth - 1) / scaledWidth);
      uint32_t alphaSum = 0;
      uint32_t sampleCount = 0;
      for (int sourceY = sourceYStart; sourceY < sourceYEnd; ++sourceY) {
        for (int sourceX = sourceXStart; sourceX < sourceXEnd; ++sourceX) {
          const int lookupY = invert ? glyphHeight - 1 - sourceY : sourceY;
          const int lookupX = invert ? glyph.width - 1 - sourceX : sourceX;
          alphaSum += glyph.bitmap[lookupY * glyph.width + lookupX];
          ++sampleCount;
        }
      }

      const uint8_t alpha =
          sampleCount == 0 ? 0 : static_cast<uint8_t>(alphaSum / sampleCount);
      if (alpha < kGlyphAlphaThreshold) {
        continue;
      }

      virtualFrame_[dstY * kVirtualBufferWidth + dstX] =
          panelColor(blendOverBackground(color, alpha));
    }
  }
}

void DisplayManager::fillVirtualRect(int x, int y, int width, int height, uint16_t color) {
  const uint16_t panel = panelColor(color);
  const int xEnd = std::min(kVirtualBufferWidth, x + width);
  const int yEnd = std::min(kVirtualBufferHeight, y + height);
  x = std::max(0, x);
  y = std::max(0, y);

  for (int row = y; row < yEnd; ++row) {
    for (int col = x; col < xEnd; ++col) {
      virtualFrame_[row * kVirtualBufferWidth + col] = panel;
    }
  }
}

void DisplayManager::drawSerifTextAt(const String &text, int x, int y, uint16_t color,
                                     int divisor) {
  divisor = std::max(1, divisor);
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(text);
  for (size_t i = 0; i < text.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(text[i], typeface);
    const int xOffset = scaledSignedAdvance(glyph.xOffset, divisor);
    const int width = glyph.width == 0 ? 0 : scaledAdvance(glyph.width, divisor);
    drawSerifGlyphScaled(cursorX + xOffset, y, text[i], color, divisor, typeface);
    int tracked = trackedAdvanceScaled(glyph.xAdvance, divisor, i, text.length());
    if (i + 1 < text.length()) {
      const ReaderGlyph nextGlyph = glyphFor(text[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          text[i], text[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(nextGlyph.xOffset, divisor), scaledDesiredGap(divisor));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawSerif70TextAt(const String &text, int x, int y, uint16_t color) {
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(text);
  for (size_t i = 0; i < text.length(); ++i) {
    const ReaderGlyph glyph = glyph70For(text[i], typeface);
    drawSerif70Glyph(cursorX + glyph.xOffset, y, text[i], color, typeface);
    int tracked = trackedAdvance(glyph.xAdvance, i, text.length());
    if (i + 1 < text.length()) {
      const ReaderGlyph nextGlyph = glyph70For(text[i + 1], typeface);
      tracked -= opticalKerningAdjustment(text[i], text[i + 1], glyph.xOffset, glyph.width, tracked,
                                          nextGlyph.xOffset, regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawSerifTextScaledAt(const String &text, int x, int y, uint16_t color,
                                           uint8_t scalePercent) {
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(text);
  for (size_t i = 0; i < text.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(text[i], typeface);
    const int xOffset = scaledSignedPercent(glyph.xOffset, scalePercent);
    const int width =
        glyph.width == 0 ? 0 : scaledPercentDimension(glyph.width, scalePercent);
    drawSerifGlyphScaledPercent(cursorX + xOffset, y, text[i], color, scalePercent, typeface);
    int tracked = trackedAdvanceScaledPercent(glyph.xAdvance, scalePercent, i, text.length());
    if (i + 1 < text.length()) {
      const ReaderGlyph nextGlyph = glyphFor(text[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          text[i], text[i + 1], xOffset, width, tracked,
          scaledSignedPercent(nextGlyph.xOffset, scalePercent),
          scaledPercentDesiredGap(scalePercent));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawTinyGlyph(int x, int y, char c, uint16_t color, int scale) {
  const uint8_t *rows = tinyRowsFor(c);
  const uint16_t panel = panelColor(color);

  for (int row = 0; row < kTinyGlyphHeight; ++row) {
    for (int col = 0; col < kTinyGlyphWidth; ++col) {
      if ((rows[row] & (1 << (kTinyGlyphWidth - 1 - col))) == 0) {
        continue;
      }

      for (int yy = 0; yy < scale; ++yy) {
        const int dstY = y + row * scale + yy;
        if (dstY < 0 || dstY >= kVirtualBufferHeight) {
          continue;
        }

        for (int xx = 0; xx < scale; ++xx) {
          const int dstX = x + col * scale + xx;
          if (dstX < 0 || dstX >= kVirtualBufferWidth) {
            continue;
          }
          virtualFrame_[dstY * kVirtualBufferWidth + dstX] = panel;
        }
      }
    }
  }
}

void DisplayManager::drawTinyTextAt180(const String &text, int x, int y, uint16_t color,
                                       int scale) {
  const uint16_t panel = panelColor(color);
  const int count = static_cast<int>(text.length());
  for (int charIndex = 0; charIndex < count; ++charIndex) {
    const int charBaseX =
        x + (count - 1 - charIndex) * (kTinyGlyphWidth + kTinyGlyphSpacing) * scale;
    const uint8_t *rows = tinyRowsFor(text[charIndex]);
    for (int row = 0; row < kTinyGlyphHeight; ++row) {
      const int dstRow = kTinyGlyphHeight - 1 - row;
      for (int col = 0; col < kTinyGlyphWidth; ++col) {
        if ((rows[row] & (1 << (kTinyGlyphWidth - 1 - col))) == 0) {
          continue;
        }
        const int dstCol = kTinyGlyphWidth - 1 - col;
        for (int yy = 0; yy < scale; ++yy) {
          const int dstY = y + dstRow * scale + yy;
          if (dstY < 0 || dstY >= kVirtualBufferHeight) {
            continue;
          }
          for (int xx = 0; xx < scale; ++xx) {
            const int dstX = charBaseX + dstCol * scale + xx;
            if (dstX < 0 || dstX >= kVirtualBufferWidth) {
              continue;
            }
            virtualFrame_[dstY * kVirtualBufferWidth + dstX] = panel;
          }
        }
      }
    }
  }
}

void DisplayManager::drawTinyTextAt(const String &text, int x, int y, uint16_t color, int scale) {
  int cursorX = x;
  for (size_t i = 0; i < text.length(); ++i) {
    drawTinyGlyph(cursorX, y, text[i], color, scale);
    cursorX += (kTinyGlyphWidth + kTinyGlyphSpacing) * scale;
  }
}

void DisplayManager::drawTinyTextCentered(const String &text, int y, uint16_t color, int scale) {
  const int textWidth = measureTinyTextWidth(text, scale);
  drawTinyTextAt(text, std::max(0, (kVirtualBufferWidth - textWidth) / 2), y, color, scale);
}

void DisplayManager::drawTinyTextCentered(const String &text, int y, uint16_t color, int scale,
                                          int width, int xOffset) {
  const int textWidth = measureTinyTextWidth(text, scale);
  drawTinyTextAt(text, std::max(xOffset, xOffset + ((width - textWidth) / 2)), y, color, scale);
}

void DisplayManager::drawSerif70TextCentered(const String &text, int y, uint16_t color, int width,
                                             int xOffset) {
  const int textWidth = measureSerif70TextWidth(text);
  drawSerif70TextAt(text, std::max(xOffset, xOffset + ((width - textWidth) / 2)), y, color);
}

void DisplayManager::drawSerifTextScaledCentered(const String &text, int y, uint16_t color,
                                                 uint8_t scalePercent, int width, int xOffset) {
  const int textWidth = measureSerifTextWidthScaled(text, scalePercent);
  drawSerifTextScaledAt(text, std::max(xOffset, xOffset + ((width - textWidth) / 2)), y, color,
                        scalePercent);
}

void DisplayManager::drawBatteryBadge() {
  drawBatteryBadge(kDisplayWidth, kDisplayHeight);
}

void DisplayManager::drawBatteryBadge(int logicalWidth, int logicalHeight) {
  drawBatteryBadge(logicalWidth, logicalHeight, ReaderChrome());
}

void DisplayManager::drawBatteryBadge(const ReaderChrome &chrome) {
  drawBatteryBadge(kDisplayWidth, kDisplayHeight, chrome);
}

void DisplayManager::drawBatteryBadge(int logicalWidth, int logicalHeight,
                                      const ReaderChrome &chrome) {
  if (batteryLabel_.isEmpty()) {
    return;
  }

  // Battery stays pinned to the top-right corner (progress occupies the top-left): a percentage
  // followed by a small phone-style battery icon whose fill tracks the charge level.
  const uint16_t color = footerColor();
  const int textWidth = measureTinyTextWidth(batteryLabel_, kTinyScale);
  const int textHeight = kTinyGlyphHeight * kTinyScale;

  const int bodyW = 22;
  const int bodyH = 11;
  const int nubW = 2;
  const int nubH = 5;
  const int gap = 5;
  const int badgeWidth = textWidth + gap + bodyW + nubW;

  const int x = std::max(kReaderBatteryMarginX, logicalWidth - kReaderBatteryMarginX - badgeWidth);
  const int y = logicalHeight > (kDisplayHeight * 2) ? kReaderBatteryMarginTop + 8
                                                      : kReaderBatteryMarginTop;

  drawTinyTextAt(batteryLabel_, x, y, color, kTinyScale);

  // Battery outline (vertically centred on the text) with a positive-terminal nub.
  const int bx = x + textWidth + gap;
  const int by = y + std::max(0, (textHeight - bodyH) / 2);
  fillVirtualRect(bx, by, bodyW, 1, color);
  fillVirtualRect(bx, by + bodyH - 1, bodyW, 1, color);
  fillVirtualRect(bx, by, 1, bodyH, color);
  fillVirtualRect(bx + bodyW - 1, by, 1, bodyH, color);
  fillVirtualRect(bx + bodyW, by + (bodyH - nubH) / 2, nubW, nubH, color);

  if (batteryCharging_) {
    // Charging: draw a small lightning bolt inside the battery instead of the level bar.
    static const uint8_t kBolt[7] = {0b00110, 0b01100, 0b11000, 0b11110,
                                     0b00110, 0b01100, 0b11000};
    const int boltX = bx + (bodyW - 5) / 2;
    const int boltY = by + (bodyH - 7) / 2;
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if (kBolt[row] & (1 << (4 - col))) {
          fillVirtualRect(boltX + col, boltY + row, 1, 1, color);
        }
      }
    }
  } else {
    // Not charging: fill bar proportional to the reported percentage.
    int percent = batteryLabel_.toInt();
    percent = std::max(0, std::min(100, percent));
    const int fillW = ((bodyW - 4) * percent) / 100;
    if (fillW > 0) {
      fillVirtualRect(bx + 2, by + 2, fillW, bodyH - 4, color);
    }
  }
}

void DisplayManager::drawClockBadge(int logicalWidth, int logicalHeight) {
  if (!Board::Config::READER_SHOW_CLOCK || clockLabel_.isEmpty()) {
    return;
  }

  const int width = measureTinyTextWidth(clockLabel_, kTinyScale);
  const int x = std::max(0, (logicalWidth - width) / 2);
  const int y = logicalHeight > (kDisplayHeight * 2) ? kReaderBatteryMarginTop + 8
                                                     : kReaderBatteryMarginTop;
  drawTinyTextAt(clockLabel_, x, y, footerColor(), kTinyScale);
}

void DisplayManager::drawBrightnessToastBadge(int logicalWidth, int logicalHeight) {
  if (brightnessOverlayText_.isEmpty()) {
    return;
  }

  const int iconSize = 11;
  const int iconGap = 8;
  const int textWidth = measureTinyTextWidth(brightnessOverlayText_, kTinyScale);
  const int badgeWidth = iconSize + iconGap + textWidth;
  const int x = std::max(kReaderBatteryMarginX, logicalWidth - kReaderBatteryMarginX - badgeWidth);
  const int y = std::min(std::max(kReaderBatteryMarginTop + 22, kReaderBatteryMarginTop),
                         std::max(kReaderBatteryMarginTop, logicalHeight - iconSize - 2));
  const uint16_t color = footerColor();
  const int iconX = x;
  const int iconY = y + 2;
  const int centerX = iconX + iconSize / 2;
  const int centerY = iconY + iconSize / 2;

  fillVirtualRect(centerX - 2, centerY - 2, 5, 5, color);
  fillVirtualRect(centerX, iconY, 1, 2, color);
  fillVirtualRect(centerX, iconY + iconSize - 2, 1, 2, color);
  fillVirtualRect(iconX, centerY, 2, 1, color);
  fillVirtualRect(iconX + iconSize - 2, centerY, 2, 1, color);
  fillVirtualRect(iconX + 2, iconY + 2, 1, 1, color);
  fillVirtualRect(iconX + iconSize - 3, iconY + 2, 1, 1, color);
  fillVirtualRect(iconX + 2, iconY + iconSize - 3, 1, 1, color);
  fillVirtualRect(iconX + iconSize - 3, iconY + iconSize - 3, 1, 1, color);
  drawTinyTextAt(brightnessOverlayText_, x + iconSize + iconGap, y, color, kTinyScale);
}

void DisplayManager::drawPreviousSentenceHint(int logicalWidth, const ReaderChrome &chrome) {
  // The top corners now hold progress (left) and battery (right), so the rewind hint moves to the
  // bottom-right corner, opposite the chapter label.
  (void)chrome;
  const String hint = "<<";
  const int width = measureTinyTextWidth(hint, kTinyScale);
  const int x = std::max(kReaderChromeMarginX, logicalWidth - kReaderChromeMarginX - width);
  const int y = kDisplayHeight - kTinyGlyphHeight * kTinyScale - kReaderChromeMarginBottom;
  drawTinyTextAt(hint, x, y, footerColor(), kTinyScale);
}

void DisplayManager::drawEdgeMenuHints(int logicalWidth, int logicalHeight,
                                       const ReaderChrome &chrome) {
  if (!chrome.showEdgeMenuHints) {
    return;
  }

  const int handleWidth =
      std::min(kEdgeMenuHintMaxWidth, std::max(kEdgeMenuHintMinWidth, logicalWidth / 7));
  const int x = std::max(0, (logicalWidth - handleWidth) / 2);
  const uint16_t color =
      blendOverBackground(wordColor(), nightMode_ ? kNightEdgeMenuHintAlpha : kEdgeMenuHintAlpha);

  auto drawHandle = [&](int y) {
    fillVirtualRect(x + 1, y, handleWidth - 2, 1, color);
    fillVirtualRect(x, y + 1, handleWidth, kEdgeMenuHintHeight - 2, color);
    fillVirtualRect(x + 1, y + kEdgeMenuHintHeight - 1, handleWidth - 2, 1, color);
  };

  drawHandle(kEdgeMenuHintInset);
  drawHandle(std::max(kEdgeMenuHintInset,
                      logicalHeight - kEdgeMenuHintInset - kEdgeMenuHintHeight));
}

void DisplayManager::drawFooter(const String &chapterLabel, const String &statusLabel,
                                const ReaderChrome &chrome) {
  if (!chrome.showChapter && !chrome.showProgress) {
    return;
  }

  // Book progress lives in the top-left corner, mirroring the battery badge in the top-right.
  if (chrome.showProgress) {
    const String status = statusLabel.isEmpty() ? "0%" : statusLabel;
    drawTinyTextAt(status, kReaderBatteryMarginX, kReaderBatteryMarginTop, footerColor(),
                   kTinyScale);
  }

  // Chapter/section name stays along the bottom edge and may use the full width.
  if (chrome.showChapter) {
    const int y = kDisplayHeight - kTinyGlyphHeight * kTinyScale - kReaderChromeMarginBottom;
    const int maxChapterWidth = kDisplayWidth - (kReaderChromeMarginX * 2);
    const String chapter = fitTinyText(chapterLabel.isEmpty() ? "START" : chapterLabel,
                                      maxChapterWidth, kTinyScale);
    drawTinyTextAt(chapter, kReaderChromeMarginX, y, footerColor(), kTinyScale);
  }
}

void DisplayManager::drawRsvpAnchorGuide(int anchorX, int textY, int textHeight) {
  const int topY = std::max(2, textY - kRsvpGuideTopOffset);
  const int bottomY = std::min(kVirtualBufferHeight - 3, textY + textHeight + kRsvpGuideBottomOffset);
  const int guideHalfWidth = currentGuideHalfWidth();
  const int guideGap = currentGuideGap();
  const int leftX = std::max(0, anchorX - guideHalfWidth);
  const int rightX = std::min(kVirtualBufferWidth - 1, anchorX + guideHalfWidth);
  const int leftWidth = std::max(0, (anchorX - guideGap) - leftX);
  const int rightWidth = std::max(0, rightX - (anchorX + guideGap) + 1);
  const uint16_t guideColor = blendOverBackground(wordColor(), nightMode_ ? 136 : 96);
  const uint16_t guideTickColor = currentFocusHighlightEnabled() ? focusColor() : guideColor;

  fillVirtualRect(leftX, topY, leftWidth, 1, guideColor);
  fillVirtualRect(anchorX + guideGap, topY, rightWidth, 1, guideColor);
  fillVirtualRect(leftX, bottomY, leftWidth, 1, guideColor);
  fillVirtualRect(anchorX + guideGap, bottomY, rightWidth, 1, guideColor);
  fillVirtualRect(anchorX, topY, 1, kRsvpGuideTickHeight, guideTickColor);
  fillVirtualRect(anchorX, bottomY - kRsvpGuideTickHeight + 1, 1, kRsvpGuideTickHeight,
                  guideTickColor);
}

void DisplayManager::drawWordAt(const String &word, int x, int y, uint16_t color) {
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(word);
  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    drawGlyph(cursorX + glyph.xOffset, y, word[i], color, typeface);
    int tracked = trackedAdvance(glyph.xAdvance, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], glyph.xOffset, glyph.width, tracked,
                                          nextGlyph.xOffset, regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvpWordScaledAt(const String &word, int x, int y, int focusIndex,
                                          int divisor) {
  divisor = std::max(1, divisor);
  const bool highlightFocus = currentFocusHighlightEnabled();
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(word);
  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    const uint16_t color =
        (highlightFocus && static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    const int xOffset = scaledSignedAdvance(glyph.xOffset, divisor);
    const int width = glyph.width == 0 ? 0 : scaledAdvance(glyph.width, divisor);
    drawSerifGlyphScaled(cursorX + xOffset, y, word[i], color, divisor, typeface);
    int tracked = trackedAdvanceScaled(glyph.xAdvance, divisor, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedAdvance(nextGlyph.xOffset, divisor), scaledDesiredGap(divisor));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvp70WordAt(const String &word, int x, int y, int focusIndex) {
  const bool highlightFocus = currentFocusHighlightEnabled();
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(word);
  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyph70For(word[i], typeface);
    const uint16_t color =
        (highlightFocus && static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    drawSerif70Glyph(cursorX + glyph.xOffset, y, word[i], color, typeface);
    int tracked = trackedAdvance(glyph.xAdvance, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyph70For(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(word[i], word[i + 1], glyph.xOffset, glyph.width, tracked,
                                          nextGlyph.xOffset, regularDesiredGap());
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvpWordScaledPercentAt(const String &word, int x, int y, int focusIndex,
                                                 uint8_t scalePercent) {
  const bool highlightFocus = currentFocusHighlightEnabled();
  int cursorX = x;
  const ReaderTypeface typeface = effectiveReaderTypefaceForText(word);
  for (size_t i = 0; i < word.length(); ++i) {
    const ReaderGlyph glyph = glyphFor(word[i], typeface);
    const uint16_t color =
        (highlightFocus && static_cast<int>(i) == focusIndex) ? focusColor() : wordColor();
    const int xOffset = scaledSignedPercent(glyph.xOffset, scalePercent);
    const int width =
        glyph.width == 0 ? 0 : scaledPercentDimension(glyph.width, scalePercent);
    drawSerifGlyphScaledPercent(cursorX + xOffset, y, word[i], color, scalePercent, typeface);
    int tracked = trackedAdvanceScaledPercent(glyph.xAdvance, scalePercent, i, word.length());
    if (i + 1 < word.length()) {
      const ReaderGlyph nextGlyph = glyphFor(word[i + 1], typeface);
      tracked -= opticalKerningAdjustment(
          word[i], word[i + 1], xOffset, width, tracked,
          scaledSignedPercent(nextGlyph.xOffset, scalePercent),
          scaledPercentDesiredGap(scalePercent));
    }
    cursorX += std::max(1, tracked);
  }
}

void DisplayManager::drawRsvpWordAt(const String &word, int x, int y, int focusIndex) {
  drawRsvpWordScaledAt(word, x, y, focusIndex, 1);
}

void DisplayManager::drawWordLine(const String &word, int y, uint16_t color) {
  const TextLayoutMetrics layout = serifWordLayout(word, -1);
  const int textWidth = textLayoutWidth(layout);
  const int x = std::max(0, ((kVirtualBufferWidth - textWidth) / 2) - layout.minX);
  drawWordAt(word, x, y, color);
}

void DisplayManager::drawMenuItem(const String &item, int y, bool selected) {
  drawWordLine(item, y, selected ? focusColor() : dimColor());
}

void DisplayManager::applyBrightness() {
  Board::Display::setBrightness(brightnessPercent_);
  Board::Display::setBacklight(true);
}

void DisplayManager::flushScaledFrame(int scale, int virtualWidth, int virtualHeight) {
  tickerPlaybackFrameActive_ = false;
  drawBrightnessToastBadge(virtualWidth, virtualHeight);
  for (int nativeYStart = 0; nativeYStart < kPanelNativeHeight;
       nativeYStart += kMaxChunkPhysicalRows) {
    const int nativeRows = std::min(kMaxChunkPhysicalRows, kPanelNativeHeight - nativeYStart);
    std::memset(txBuffer_, 0, txBufferBytes_);

    for (int localNativeY = 0; localNativeY < nativeRows; ++localNativeY) {
      const int nativeY = nativeYStart + localNativeY;
      uint16_t *dstRow = txBuffer_ + localNativeY * kPanelNativeWidth;

      for (int nativeX = 0; nativeX < kPanelNativeWidth; ++nativeX) {
        int logicalX = 0;
        int logicalY = 0;
        mapPhysicalToLogical(uiOrientation_, nativeX, nativeY, logicalX, logicalY);
        const int sourceX = logicalX / scale;
        const int sourceY = logicalY / scale;

        if (sourceX >= 0 && sourceX < virtualWidth && sourceY >= 0 && sourceY < virtualHeight) {
          dstRow[nativeX] = virtualFrame_[sourceY * kVirtualBufferWidth + sourceX];
        }
      }
    }

    if (!drawBitmap(0, nativeYStart, kPanelNativeWidth, nativeYStart + nativeRows, txBuffer_)) {
      return;
    }
  }
}

void DisplayManager::flushFullWidthLogicalBand(int yStart, int yEnd) {
  if (!initialized_) {
    return;
  }

  if (isPortraitOrientation(uiOrientation_)) {
    flushScaledFrame(1, logicalWidth(), logicalHeight());
    return;
  }

  yStart = std::max(0, std::min(kDisplayHeight, yStart));
  yEnd = std::max(0, std::min(kDisplayHeight, yEnd));
  if (yEnd <= yStart) {
    return;
  }

  const bool flipped = uiOrientation_ == Board::Config::UiOrientation::LandscapeFlipped;
  const int physicalXStart = flipped ? (kDisplayHeight - yEnd) : yStart;
  const int physicalXEnd = flipped ? (kDisplayHeight - yStart) : yEnd;
  const int physicalWidth = physicalXEnd - physicalXStart;
  if (physicalWidth <= 0 || txBuffer_ == nullptr) {
    return;
  }

  for (int nativeYStart = 0; nativeYStart < kPanelNativeHeight;
       nativeYStart += kMaxChunkPhysicalRows) {
    const int nativeRows = std::min(kMaxChunkPhysicalRows, kPanelNativeHeight - nativeYStart);

    for (int localNativeY = 0; localNativeY < nativeRows; ++localNativeY) {
      const int nativeY = nativeYStart + localNativeY;
      uint16_t *dstRow = txBuffer_ + (localNativeY * physicalWidth);

      for (int localNativeX = 0; localNativeX < physicalWidth; ++localNativeX) {
        const int nativeX = physicalXStart + localNativeX;
        int logicalX = 0;
        int logicalY = 0;
        mapPhysicalToLogical(uiOrientation_, nativeX, nativeY, logicalX, logicalY);
        dstRow[localNativeX] = virtualFrame_[logicalY * kVirtualBufferWidth + logicalX];
      }
    }

    if (!drawBitmap(physicalXStart, nativeYStart, physicalXEnd, nativeYStart + nativeRows,
                    txBuffer_)) {
      return;
    }
  }

  tickerPlaybackFrameActive_ = true;
}

void DisplayManager::renderCenteredWord(const String &word, uint16_t color) {
  String normalized = word;
  const uint16_t renderColor = (color == kPureWhite) ? wordColor() : color;
  const String renderKey = "center|" + normalized + "|" + String(renderColor) + "|b:" +
                           batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
                           String(nightMode_ ? 1 : 0);

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = chooseTextScale(normalized);
  const int virtualWidth = (kDisplayWidth + scale - 1) / scale;
  const int virtualHeight = (kDisplayHeight + scale - 1) / scale;
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(normalized));

  clearVirtualBuffer(virtualWidth, virtualHeight);
  const int y = std::max(0, (virtualHeight - glyphHeight) / 2);
  drawWordLine(normalized, y, renderColor);
  drawBatteryBadge();

  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderRsvpWord(const String &word, const String &chapterLabel,
                                    uint8_t progressPercent, bool showFooter,
                                    const String &footerStatusLabel, ReaderChrome chrome) {
  const String renderKey =
      "rsvp|" + word + "|" + chapterLabel + "|" + String(progressPercent) + "|" +
      String(showFooter ? 1 : 0) + "|f:" + footerStatusLabel + "|b:" + batteryLabel_ +
      "|rc:" + readerChromeKey(chrome) + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
      String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = logicalWidth();
  const int virtualHeight = logicalHeight();
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
  const int y = std::max(0, (virtualHeight - glyphHeight) / 2);
  const int focusIndex = findFocusLetterIndex(word);
  const int x = rsvpStartX(word, focusIndex, virtualWidth, 1, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, y, glyphHeight);
  drawRsvpWordAt(word, x, y, focusIndex);
  if (showFooter) {
    drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                         : footerStatusLabel,
               chrome);
  }
  if (chrome.showPreviousSentenceHint) {
    drawPreviousSentenceHint(virtualWidth, chrome);
  }
  drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
  drawClockBadge(virtualWidth, virtualHeight);
  if (chrome.showBattery) {
    drawBatteryBadge(virtualWidth, virtualHeight, chrome);
  }
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderRsvpWordWithWpm(const String &word, uint16_t wpm,
                                           const String &chapterLabel, uint8_t progressPercent,
                                           bool showFooter, const String &footerStatusLabel,
                                           ReaderChrome chrome) {
  const String wpmText = String(wpm) + " WPM";
  const String renderKey =
      "rsvp_wpm|" + word + "|" + wpmText + "|" + chapterLabel + "|" +
      String(progressPercent) + "|" + String(showFooter ? 1 : 0) + "|f:" + footerStatusLabel +
      "|b:" + batteryLabel_ + "|rc:" + readerChromeKey(chrome) + "|d:" +
      String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
  const int wordY = std::max(0, (virtualHeight - glyphHeight) / 2);
  const int wpmY =
      std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
  const int focusIndex = findFocusLetterIndex(word);
  const int x = rsvpStartX(word, focusIndex, virtualWidth, 1, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, wordY, glyphHeight);
  drawRsvpWordAt(word, x, wordY, focusIndex);
  drawTinyTextCentered(wpmText, wpmY, focusColor(), kTinyScale);
  if (showFooter) {
    drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                         : footerStatusLabel,
               chrome);
  }
  if (chrome.showPreviousSentenceHint) {
    drawPreviousSentenceHint(virtualWidth, chrome);
  }
  drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
  drawClockBadge(virtualWidth, virtualHeight);
  if (chrome.showBattery) {
    drawBatteryBadge(virtualWidth, virtualHeight, chrome);
  }
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderPhantomRsvpWord(const String &beforeText, const String &word,
                                           const String &afterText, uint8_t fontSizeLevel,
                                           const String &chapterLabel, uint8_t progressPercent,
                                           bool showFooter, const String &footerStatusLabel,
                                           ReaderChrome chrome) {
  const String renderKey =
      "rsvp_phantom|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + chapterLabel + "|" + String(progressPercent) + "|" +
      String(showFooter ? 1 : 0) + "|f:" + footerStatusLabel + "|b:" + batteryLabel_ +
      "|rc:" + readerChromeKey(chrome) + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
      String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  if (fontSizeLevel == 1) {
    const int scale = 1;
    const int virtualWidth = kDisplayWidth;
    const int virtualHeight = kDisplayHeight;
    const int mediumHeight = mediumGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
    const int textY = std::max(0, (virtualHeight - mediumHeight) / 2);
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX = rsvpStartX70(word, focusIndex, virtualWidth, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout = serif70WordLayout(word, focusIndex);
    const uint16_t phantomColor = blendOverBackground(wordColor(), kPhantomAlphaMedium);

    clearVirtualBuffer(virtualWidth, virtualHeight);
    drawRsvpAnchorGuide(anchorX, textY, mediumHeight);
    if (!beforeText.isEmpty()) {
      const TextLayoutMetrics beforeLayout = serif70WordLayout(beforeText, -1);
      const int beforeX =
          currentX + currentLayout.minX - kPhantomCurrentGapMedium - beforeLayout.maxX;
      drawSerif70TextAt(beforeText, beforeX, textY, phantomColor);
    }
    drawRsvp70WordAt(word, currentX, textY, focusIndex);
    if (!afterText.isEmpty()) {
      const TextLayoutMetrics afterLayout = serif70WordLayout(afterText, -1);
      const int afterX =
          currentX + currentLayout.maxX + kPhantomCurrentGapMedium - afterLayout.minX;
      drawSerif70TextAt(afterText, afterX, textY, phantomColor);
    }
    if (showFooter) {
      drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                           : footerStatusLabel,
                 chrome);
    }
    if (chrome.showPreviousSentenceHint) {
      drawPreviousSentenceHint(virtualWidth, chrome);
    }
    drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
    drawClockBadge(virtualWidth, virtualHeight);
    if (chrome.showBattery) {
      drawBatteryBadge(virtualWidth, virtualHeight, chrome);
    }
    flushScaledFrame(scale, virtualWidth, virtualHeight);
    return;
  }

  const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int textHeight = scaledPercentDimension(
      baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word)), style.scalePercent);
  const int textY = std::max(0, (virtualHeight - textHeight) / 2);
  const int focusIndex = findFocusLetterIndex(word);
  const int currentX =
      rsvpStartXScaledPercent(word, focusIndex, virtualWidth, style.scalePercent, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const TextLayoutMetrics currentLayout =
      serifWordLayoutScaledPercent(word, focusIndex, style.scalePercent);
  const uint16_t phantomColor = blendOverBackground(wordColor(), style.alpha);

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, textY, textHeight);
  if (!beforeText.isEmpty()) {
    const TextLayoutMetrics beforeLayout =
        serifWordLayoutScaledPercent(beforeText, -1, style.scalePercent);
    const int beforeX = currentX + currentLayout.minX - style.currentGap - beforeLayout.maxX;
    drawSerifTextScaledAt(beforeText, beforeX, textY, phantomColor, style.scalePercent);
  }
  drawRsvpWordScaledPercentAt(word, currentX, textY, focusIndex, style.scalePercent);
  if (!afterText.isEmpty()) {
    const TextLayoutMetrics afterLayout =
        serifWordLayoutScaledPercent(afterText, -1, style.scalePercent);
    const int afterX = currentX + currentLayout.maxX + style.currentGap - afterLayout.minX;
    drawSerifTextScaledAt(afterText, afterX, textY, phantomColor, style.scalePercent);
  }
  if (showFooter) {
    drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                         : footerStatusLabel,
               chrome);
  }
  if (chrome.showPreviousSentenceHint) {
    drawPreviousSentenceHint(virtualWidth, chrome);
  }
  drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
  drawClockBadge(virtualWidth, virtualHeight);
  if (chrome.showBattery) {
    drawBatteryBadge(virtualWidth, virtualHeight, chrome);
  }
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderWordTickerView(const std::vector<ContextWord> &words,
                                          size_t currentWordIndex, uint8_t fontSizeLevel,
                                          uint16_t motionPermille, const String &chapterLabel,
                                          uint8_t progressPercent, const String &overlayText,
                                          bool showFooter, ReaderChrome chrome) {
  if (words.empty()) {
    renderRsvpWord("", chapterLabel, progressPercent, showFooter, "", chrome);
    return;
  }
  if (currentWordIndex >= words.size()) {
    currentWordIndex = words.size() - 1;
  }
  if (motionPermille > 1000) {
    motionPermille = 1000;
  }

  const bool canUseBandOnly = !showFooter && overlayText.isEmpty() &&
                              !chrome.showPreviousSentenceHint && !chrome.showEdgeMenuHints &&
                              tickerPlaybackFrameActive_;
  const String chromeKey = readerChromeKey(chrome);
  String renderKey;
  renderKey.reserve(96 + chromeKey.length() + chapterLabel.length() + overlayText.length() +
                    batteryLabel_.length());
  renderKey += "ticker|";
  renderKey += String(fontSizeLevel);
  renderKey += "|i:";
  renderKey += String(currentWordIndex);
  renderKey += "|m:";
  renderKey += String(motionPermille);
  renderKey += "|f:";
  renderKey += String(showFooter ? 1 : 0);
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);
  renderKey += "|wc:";
  renderKey += String(words.size());
  renderKey += "|rc:";
  renderKey += chromeKey;
  if (!canUseBandOnly) {
    renderKey += "|c:";
    renderKey += chapterLabel;
    renderKey += "|p:";
    renderKey += String(progressPercent);
    renderKey += "|o:";
    renderKey += overlayText;
    renderKey += "|b:";
    renderKey += batteryLabel_;
  }
  const size_t keyStart = currentWordIndex > 2 ? currentWordIndex - 2 : 0;
  const size_t keyEnd = std::min(words.size(), currentWordIndex + 3);
  for (size_t index = keyStart; index < keyEnd; ++index) {
    renderKey += "|";
    renderKey += words[index].text;
  }
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int overlayY =
      std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
  const uint16_t textColor = wordColor();

  if (fontSizeLevel == 1) {
    auto layoutFor = [&](size_t index) { return serif70WordLayout(words[index].text, -1); };
    auto widthFor = [&](const TextLayoutMetrics &layout) { return textLayoutWidth(layout); };

    const int gap = kWordTickerGapMedium;
    const int mediumHeight =
        mediumGlyphHeightForTypeface(effectiveReaderTypefaceForText(words[currentWordIndex].text));
    const int textY = std::max(0, (virtualHeight - mediumHeight) / 2);
    const TextLayoutMetrics currentLayout = layoutFor(currentWordIndex);
    const int currentWidth = widthFor(currentLayout);
    const int currentLeftBase = (virtualWidth - currentWidth) / 2;
    int shiftX = 0;
    if (currentWordIndex + 1 < words.size()) {
      const int nextWidth = widthFor(layoutFor(currentWordIndex + 1));
      const int travel = gap + (currentWidth / 2) + (nextWidth / 2);
      shiftX = static_cast<int>((static_cast<int32_t>(travel) * motionPermille) / 1000);
    }

    const int bandTop = std::max(0, textY - kWordTickerBandPadding);
    const int bandBottom =
        std::min(virtualHeight, textY + mediumHeight + kWordTickerBandPadding);
    if (canUseBandOnly) {
      fillVirtualRect(0, bandTop, virtualWidth, bandBottom - bandTop, backgroundColor());
    } else {
      clearVirtualBuffer(virtualWidth, virtualHeight);
    }
    int left = currentLeftBase - shiftX;
    int originX = left - currentLayout.minX;
    drawSerif70TextAt(words[currentWordIndex].text, originX, textY, textColor);

    int nextLeft = left + currentWidth + gap;
    for (size_t index = currentWordIndex + 1; index < words.size(); ++index) {
      if (nextLeft >= virtualWidth + gap) {
        break;
      }
      const TextLayoutMetrics layout = layoutFor(index);
      const int width = widthFor(layout);
      originX = nextLeft - layout.minX;
      drawSerif70TextAt(words[index].text, originX, textY, textColor);
      nextLeft += width + gap;
    }

    int prevRight = left - gap;
    for (size_t index = currentWordIndex; index > 0;) {
      --index;
      if (prevRight <= -gap) {
        break;
      }
      const TextLayoutMetrics layout = layoutFor(index);
      const int width = widthFor(layout);
      const int prevLeft = prevRight - width;
      originX = prevLeft - layout.minX;
      drawSerif70TextAt(words[index].text, originX, textY, textColor);
      prevRight = prevLeft - gap;
    }
    if (!overlayText.isEmpty()) {
      drawTinyTextCentered(overlayText, overlayY, focusColor(), kTinyScale);
    }
    if (showFooter) {
      drawFooter(chapterLabel, String(progressPercent) + "%", chrome);
    }
    if (chrome.showPreviousSentenceHint) {
      drawPreviousSentenceHint(virtualWidth, chrome);
    }
    drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
    if (!canUseBandOnly) {
      drawClockBadge(virtualWidth, virtualHeight);
      if (chrome.showBattery) {
        drawBatteryBadge(virtualWidth, virtualHeight, chrome);
      }
      flushScaledFrame(scale, virtualWidth, virtualHeight);
      tickerPlaybackFrameActive_ = !showFooter && overlayText.isEmpty();
    } else {
      flushFullWidthLogicalBand(bandTop, bandBottom);
    }
    return;
  }

  const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
  auto layoutFor = [&](size_t index) {
    return serifWordLayoutScaledPercent(words[index].text, -1, style.scalePercent);
  };
  auto widthFor = [&](const TextLayoutMetrics &layout) { return textLayoutWidth(layout); };

  int gap = kWordTickerGapLarge;
  if (fontSizeLevel == 1) {
    gap = kWordTickerGapMedium;
  } else if (fontSizeLevel == 2) {  // Small only; Extra Large (index 3) keeps the large gap.
    gap = kWordTickerGapSmall;
  }
  gap = std::max(4, scaledPercentDimension(gap, style.scalePercent));

  const int textHeight = scaledPercentDimension(
      baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(words[currentWordIndex].text)),
      style.scalePercent);
  const int textY = std::max(0, (virtualHeight - textHeight) / 2);
  const TextLayoutMetrics currentLayout = layoutFor(currentWordIndex);
  const int currentWidth = widthFor(currentLayout);
  const int currentLeftBase = (virtualWidth - currentWidth) / 2;
  int shiftX = 0;
  if (currentWordIndex + 1 < words.size()) {
    const int nextWidth = widthFor(layoutFor(currentWordIndex + 1));
    const int travel = gap + (currentWidth / 2) + (nextWidth / 2);
    shiftX = static_cast<int>((static_cast<int32_t>(travel) * motionPermille) / 1000);
  }

  const int bandTop = std::max(0, textY - kWordTickerBandPadding);
  const int bandBottom = std::min(virtualHeight, textY + textHeight + kWordTickerBandPadding);
  if (canUseBandOnly) {
    fillVirtualRect(0, bandTop, virtualWidth, bandBottom - bandTop, backgroundColor());
  } else {
    clearVirtualBuffer(virtualWidth, virtualHeight);
  }
  int left = currentLeftBase - shiftX;
  int originX = left - currentLayout.minX;
  drawSerifTextScaledAt(words[currentWordIndex].text, originX, textY, textColor,
                        style.scalePercent);

  int nextLeft = left + currentWidth + gap;
  for (size_t index = currentWordIndex + 1; index < words.size(); ++index) {
    if (nextLeft >= virtualWidth + gap) {
      break;
    }
    const TextLayoutMetrics layout = layoutFor(index);
    const int width = widthFor(layout);
    originX = nextLeft - layout.minX;
    drawSerifTextScaledAt(words[index].text, originX, textY, textColor, style.scalePercent);
    nextLeft += width + gap;
  }

  int prevRight = left - gap;
  for (size_t index = currentWordIndex; index > 0;) {
    --index;
    if (prevRight <= -gap) {
      break;
    }
    const TextLayoutMetrics layout = layoutFor(index);
    const int width = widthFor(layout);
    const int prevLeft = prevRight - width;
    originX = prevLeft - layout.minX;
    drawSerifTextScaledAt(words[index].text, originX, textY, textColor, style.scalePercent);
    prevRight = prevLeft - gap;
  }
  if (!overlayText.isEmpty()) {
    drawTinyTextCentered(overlayText, overlayY, focusColor(), kTinyScale);
  }
  if (showFooter) {
    drawFooter(chapterLabel, String(progressPercent) + "%", chrome);
  }
  if (chrome.showPreviousSentenceHint) {
    drawPreviousSentenceHint(virtualWidth, chrome);
  }
  drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
  if (!canUseBandOnly) {
    drawClockBadge(virtualWidth, virtualHeight);
    if (chrome.showBattery) {
      drawBatteryBadge(virtualWidth, virtualHeight, chrome);
    }
    flushScaledFrame(scale, virtualWidth, virtualHeight);
    tickerPlaybackFrameActive_ = !showFooter && overlayText.isEmpty();
  } else {
    flushFullWidthLogicalBand(bandTop, bandBottom);
  }
}

void DisplayManager::renderTypographyPreview(const String &beforeText, const String &word,
                                             const String &afterText, uint8_t fontSizeLevel,
                                             const String &title, const String &line1,
                                             const String &line2) {
  const TypographyConfig config = activeTypographyConfig();
  const String renderKey =
      "typography_preview|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + title + "|" + line1 + "|" + line2 + "|t:" +
      String(static_cast<unsigned int>(config.typeface)) +
      "|h:" + String(config.focusHighlight ? 1 : 0) +
      "|tr:" +
      String(static_cast<int>(config.trackingPx)) + "|a:" +
      String(static_cast<unsigned int>(config.anchorPercent)) + "|w:" +
      String(static_cast<unsigned int>(config.guideHalfWidth)) + "|g:" +
      String(static_cast<unsigned int>(config.guideGap)) + "|b:" + batteryLabel_ + "|d:" +
      String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int tinyHeight = kTinyGlyphHeight * kTinyScale;
  const int titleY = 14;
  const int line2Y = std::max(titleY + tinyHeight + 1, virtualHeight - tinyHeight - 12);
  const int line1Y = std::max(titleY + tinyHeight + 1, line2Y - tinyHeight - 8);
  const int textTop = titleY + tinyHeight + 12;
  const int textBottom = std::max(textTop + 1, line1Y - 14);
  const int maxLabelWidth = virtualWidth - 24;

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawTinyTextCentered(fitTinyText(title, maxLabelWidth, kTinyScale), titleY, wordColor(),
                       kTinyScale);

  if (fontSizeLevel == 1) {
    const int textHeight = mediumGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
    int textY = (textTop + textBottom - textHeight) / 2;
    textY = std::max(textTop, std::min(textY, textBottom - textHeight));
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX = rsvpStartX70(word, focusIndex, virtualWidth, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout = serif70WordLayout(word, focusIndex);
    const uint16_t phantomColor = blendOverBackground(wordColor(), kPhantomAlphaMedium);

    drawRsvpAnchorGuide(anchorX, textY, textHeight);
    if (!beforeText.isEmpty()) {
      const TextLayoutMetrics beforeLayout = serif70WordLayout(beforeText, -1);
      const int beforeX =
          currentX + currentLayout.minX - kPhantomCurrentGapMedium - beforeLayout.maxX;
      drawSerif70TextAt(beforeText, beforeX, textY, phantomColor);
    }
    drawRsvp70WordAt(word, currentX, textY, focusIndex);
    if (!afterText.isEmpty()) {
      const TextLayoutMetrics afterLayout = serif70WordLayout(afterText, -1);
      const int afterX =
          currentX + currentLayout.maxX + kPhantomCurrentGapMedium - afterLayout.minX;
      drawSerif70TextAt(afterText, afterX, textY, phantomColor);
    }
  } else {
    const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
    const int textHeight = scaledPercentDimension(
        baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word)), style.scalePercent);
    int textY = (textTop + textBottom - textHeight) / 2;
    textY = std::max(textTop, std::min(textY, textBottom - textHeight));
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX =
        rsvpStartXScaledPercent(word, focusIndex, virtualWidth, style.scalePercent, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout =
        serifWordLayoutScaledPercent(word, focusIndex, style.scalePercent);
    const uint16_t phantomColor = blendOverBackground(wordColor(), style.alpha);

    drawRsvpAnchorGuide(anchorX, textY, textHeight);
    if (!beforeText.isEmpty()) {
      const TextLayoutMetrics beforeLayout =
          serifWordLayoutScaledPercent(beforeText, -1, style.scalePercent);
      const int beforeX = currentX + currentLayout.minX - style.currentGap - beforeLayout.maxX;
      drawSerifTextScaledAt(beforeText, beforeX, textY, phantomColor, style.scalePercent);
    }
    drawRsvpWordScaledPercentAt(word, currentX, textY, focusIndex, style.scalePercent);
    if (!afterText.isEmpty()) {
      const TextLayoutMetrics afterLayout =
          serifWordLayoutScaledPercent(afterText, -1, style.scalePercent);
      const int afterX = currentX + currentLayout.maxX + style.currentGap - afterLayout.minX;
      drawSerifTextScaledAt(afterText, afterX, textY, phantomColor, style.scalePercent);
    }
  }

  if (!line1.isEmpty()) {
    drawTinyTextCentered(fitTinyText(line1, maxLabelWidth, kTinyScale), line1Y, focusColor(),
                         kTinyScale);
  }
  if (!line2.isEmpty()) {
    drawTinyTextCentered(fitTinyText(line2, maxLabelWidth, kTinyScale), line2Y, dimColor(),
                         kTinyScale);
  }
  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderPhantomRsvpWordWithWpm(const String &beforeText, const String &word,
                                                  const String &afterText, uint8_t fontSizeLevel,
                                                  uint16_t wpm, const String &chapterLabel,
                                                  uint8_t progressPercent, bool showFooter,
                                                  const String &footerStatusLabel,
                                                  ReaderChrome chrome) {
  const String wpmText = String(wpm) + " WPM";
  const String renderKey =
      "rsvp_phantom_wpm|" + beforeText + "|" + word + "|" + afterText + "|s:" +
      String(fontSizeLevel) + "|" + wpmText + "|" + chapterLabel + "|" +
      String(progressPercent) + "|" + String(showFooter ? 1 : 0) + "|f:" + footerStatusLabel +
      "|b:" + batteryLabel_ + "|rc:" + readerChromeKey(chrome) + "|d:" +
      String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  if (fontSizeLevel == 1) {
    const int scale = 1;
    const int virtualWidth = kDisplayWidth;
    const int virtualHeight = kDisplayHeight;
    const int mediumHeight = mediumGlyphHeightForTypeface(effectiveReaderTypefaceForText(word));
    const int textY = std::max(0, (virtualHeight - mediumHeight) / 2);
    const int wpmY =
        std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
    const int focusIndex = findFocusLetterIndex(word);
    const int currentX = rsvpStartX70(word, focusIndex, virtualWidth, false);
    const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
    const TextLayoutMetrics currentLayout = serif70WordLayout(word, focusIndex);
    const uint16_t phantomColor = blendOverBackground(wordColor(), kPhantomAlphaMedium);

    clearVirtualBuffer(virtualWidth, virtualHeight);
    drawRsvpAnchorGuide(anchorX, textY, mediumHeight);
    if (!beforeText.isEmpty()) {
      const TextLayoutMetrics beforeLayout = serif70WordLayout(beforeText, -1);
      const int beforeX =
          currentX + currentLayout.minX - kPhantomCurrentGapMedium - beforeLayout.maxX;
      drawSerif70TextAt(beforeText, beforeX, textY, phantomColor);
    }
    drawRsvp70WordAt(word, currentX, textY, focusIndex);
    if (!afterText.isEmpty()) {
      const TextLayoutMetrics afterLayout = serif70WordLayout(afterText, -1);
      const int afterX =
          currentX + currentLayout.maxX + kPhantomCurrentGapMedium - afterLayout.minX;
      drawSerif70TextAt(afterText, afterX, textY, phantomColor);
    }
    drawTinyTextCentered(wpmText, wpmY, focusColor(), kTinyScale);
    if (showFooter) {
      drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                           : footerStatusLabel,
                 chrome);
    }
    if (chrome.showPreviousSentenceHint) {
      drawPreviousSentenceHint(virtualWidth, chrome);
    }
    drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
    drawClockBadge(virtualWidth, virtualHeight);
    if (chrome.showBattery) {
      drawBatteryBadge(virtualWidth, virtualHeight, chrome);
    }
    flushScaledFrame(scale, virtualWidth, virtualHeight);
    return;
  }

  const ReaderTextStyle style = readerTextStyle(fontSizeLevel);
  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int textHeight = scaledPercentDimension(
      baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(word)), style.scalePercent);
  const int textY = std::max(0, (virtualHeight - textHeight) / 2);
  const int wpmY =
      std::max(0, virtualHeight - kTinyGlyphHeight * kTinyScale - kWpmFeedbackBottomMargin - 24);
  const int focusIndex = findFocusLetterIndex(word);
  const int currentX =
      rsvpStartXScaledPercent(word, focusIndex, virtualWidth, style.scalePercent, false);
  const int anchorX = (virtualWidth * currentAnchorPercent()) / 100;
  const TextLayoutMetrics currentLayout =
      serifWordLayoutScaledPercent(word, focusIndex, style.scalePercent);
  const uint16_t phantomColor = blendOverBackground(wordColor(), style.alpha);

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawRsvpAnchorGuide(anchorX, textY, textHeight);
  if (!beforeText.isEmpty()) {
    const TextLayoutMetrics beforeLayout =
        serifWordLayoutScaledPercent(beforeText, -1, style.scalePercent);
    const int beforeX = currentX + currentLayout.minX - style.currentGap - beforeLayout.maxX;
    drawSerifTextScaledAt(beforeText, beforeX, textY, phantomColor, style.scalePercent);
  }
  drawRsvpWordScaledPercentAt(word, currentX, textY, focusIndex, style.scalePercent);
  if (!afterText.isEmpty()) {
    const TextLayoutMetrics afterLayout =
        serifWordLayoutScaledPercent(afterText, -1, style.scalePercent);
    const int afterX = currentX + currentLayout.maxX + style.currentGap - afterLayout.minX;
    drawSerifTextScaledAt(afterText, afterX, textY, phantomColor, style.scalePercent);
  }
  drawTinyTextCentered(wpmText, wpmY, focusColor(), kTinyScale);
  if (showFooter) {
    drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                         : footerStatusLabel,
               chrome);
  }
  if (chrome.showPreviousSentenceHint) {
    drawPreviousSentenceHint(virtualWidth, chrome);
  }
  drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
  drawClockBadge(virtualWidth, virtualHeight);
  if (chrome.showBattery) {
    drawBatteryBadge(virtualWidth, virtualHeight, chrome);
  }
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderScrollView(const std::vector<ContextWord> &words, uint32_t contentToken,
                                      size_t windowStartIndex, size_t currentWordIndex,
                                      uint16_t scrollProgressPermille,
                                      const String &chapterLabel, uint8_t progressPercent,
                                      const String &overlayText,
                                      const String &footerStatusLabel, ReaderChrome chrome) {
  if (words.empty()) {
    renderRsvpWord("", chapterLabel, progressPercent, true, footerStatusLabel, chrome);
    return;
  }

  struct ContextLine {
    size_t start = 0;
    size_t end = 0;
    bool paragraphStart = false;
  };

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int overlayReserve = overlayText.isEmpty() ? 0 : (kTinyGlyphHeight * kTinyScale + 6);
  const bool showFooterRow = chrome.showChapter || chrome.showProgress;
  const int footerReserve =
      showFooterRow ? (kTinyGlyphHeight * kTinyScale + kReaderChromeMarginBottom + 6) : 6;
  const int textTop = kScrollTop;
  const int textBottom = virtualHeight - footerReserve - overlayReserve;
  const ReaderTypeface contextTypeface = currentReaderTypeface();
  const int contextGlyphHeight = std::max(
      1, (baseGlyphHeightForTypeface(contextTypeface) + kScrollSerifDivisor - 1) /
             kScrollSerifDivisor);
  const int maxLineWidth = virtualWidth - (kScrollMarginX * 2);

  size_t currentLocalIndex = 0;
  if (currentWordIndex >= windowStartIndex && currentWordIndex < windowStartIndex + words.size()) {
    currentLocalIndex = currentWordIndex - windowStartIndex;
  }
  size_t nextLocalIndex = currentLocalIndex;
  if (nextLocalIndex + 1 < words.size()) {
    ++nextLocalIndex;
  }
  if (scrollProgressPermille > 1000) {
    scrollProgressPermille = 1000;
  }

  std::vector<ContextLine> lines;
  lines.reserve(16);
  size_t currentLineIndex = 0;
  size_t nextLineIndex = 0;
  bool foundCurrentLine = false;
  bool foundNextLine = false;

  size_t index = 0;
  while (index < words.size()) {
    ContextLine line;
    line.start = index;
    line.paragraphStart = words[index].paragraphStart;
    int lineWidth = line.paragraphStart ? kScrollParagraphIndent : 0;

    while (index < words.size()) {
      if (index > line.start && words[index].paragraphStart) {
        break;
      }

      const int wordWidth = measureSerifTextWidth(words[index].text, kScrollSerifDivisor);
      const int gap = (index == line.start) ? 0 : kScrollSpaceWidth;
      if (index > line.start && lineWidth + gap + wordWidth > maxLineWidth) {
        break;
      }

      lineWidth += gap + wordWidth;
      ++index;

      if (lineWidth >= maxLineWidth) {
        break;
      }
    }

    line.end = std::max(line.start + 1, index);
    if (line.end > words.size()) {
      line.end = words.size();
    }
    if (!foundCurrentLine && currentLocalIndex >= line.start && currentLocalIndex < line.end) {
      currentLineIndex = lines.size();
      foundCurrentLine = true;
    }
    if (!foundNextLine && nextLocalIndex >= line.start && nextLocalIndex < line.end) {
      nextLineIndex = lines.size();
      foundNextLine = true;
    }
    lines.push_back(line);

    if (line.end == line.start) {
      ++index;
    }
  }

  if (lines.empty()) {
    renderRsvpWord("", chapterLabel, progressPercent, true, footerStatusLabel, chrome);
    return;
  }

  if (!foundCurrentLine) {
    currentLineIndex = 0;
  }
  if (!foundNextLine) {
    nextLineIndex = currentLineIndex;
  }

  std::vector<int> lineTops;
  lineTops.reserve(lines.size());
  int contentBottom = textTop + contextGlyphHeight;
  int y = textTop;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    if (lineIndex != 0 && lines[lineIndex].paragraphStart) {
      y += kScrollParagraphGap;
    }
    lineTops.push_back(y);
    contentBottom = y + contextGlyphHeight;
    y += kScrollLineHeight;
  }

  const int currentCenterY = lineTops[currentLineIndex] + (contextGlyphHeight / 2);
  const int nextCenterY = lineTops[nextLineIndex] + (contextGlyphHeight / 2);
  const int focusCenterY =
      currentCenterY +
      (((nextCenterY - currentCenterY) * static_cast<int>(scrollProgressPermille)) / 1000);
  const int preferredFocusY = textTop + ((textBottom - textTop) / 2);
  int scrollOffset = preferredFocusY - focusCenterY;
  const int minScrollOffset = std::min(0, textBottom - contentBottom);
  scrollOffset = std::max(minScrollOffset, std::min(0, scrollOffset));

  const String renderKey =
      "scroll|" + String(contentToken) + "|" + String(windowStartIndex) + "|" +
      String(currentWordIndex) + "|" + String(words.size()) + "|" + String(scrollOffset) +
      "|" + chapterLabel + "|" + String(progressPercent) + "|o:" + overlayText + "|f:" +
      footerStatusLabel + "|b:" + batteryLabel_ + "|rc:" + readerChromeKey(chrome) + "|d:" +
      String(darkMode_ ? 1 : 0) + "|n:" + String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;
  clearVirtualBuffer(virtualWidth, virtualHeight);

  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    const ContextLine &line = lines[lineIndex];
    const int lineY = lineTops[lineIndex] + scrollOffset;
    if (lineY + contextGlyphHeight < 0) {
      continue;
    }
    if (lineY > textBottom) {
      break;
    }

    int x = kScrollMarginX + (line.paragraphStart ? kScrollParagraphIndent : 0);
    for (size_t wordIndex = line.start; wordIndex < line.end && wordIndex < words.size();
         ++wordIndex) {
      const ContextWord &word = words[wordIndex];
      const uint16_t color =
          (word.current && currentFocusHighlightEnabled()) ? focusColor() : wordColor();
      const String visibleWord =
          fitSerifText(word.text, virtualWidth - x - kScrollMarginX, kScrollSerifDivisor);
      drawSerifTextAt(visibleWord, x, lineY, color, kScrollSerifDivisor);
      x += measureSerifTextWidth(visibleWord, kScrollSerifDivisor) + kScrollSpaceWidth;
    }
  }

  if (!overlayText.isEmpty()) {
    const int overlayY = textBottom + 8;
    drawTinyTextCentered(fitTinyText(overlayText, virtualWidth - 24, kTinyScale), overlayY,
                         focusColor(), kTinyScale);
  }

  drawFooter(chapterLabel, footerStatusLabel.isEmpty() ? String(progressPercent) + "%"
                                                       : footerStatusLabel,
             chrome);
  if (chrome.showPreviousSentenceHint) {
    drawPreviousSentenceHint(virtualWidth, chrome);
  }
  drawEdgeMenuHints(virtualWidth, virtualHeight, chrome);
  drawClockBadge(virtualWidth, virtualHeight);
  if (chrome.showBattery) {
    drawBatteryBadge(virtualWidth, virtualHeight, chrome);
  }
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderMenu(const char *const *items, size_t itemCount, size_t selectedIndex) {
  if (items == nullptr || itemCount == 0) {
    renderCenteredWord("MENU");
    return;
  }

  std::vector<String> menuItems;
  menuItems.reserve(itemCount);
  for (size_t i = 0; i < itemCount; ++i) {
    menuItems.push_back(items[i] == nullptr ? "" : items[i]);
  }

  renderMenu(menuItems, selectedIndex);
}

void DisplayManager::renderMenu(const std::vector<String> &items, size_t selectedIndex) {
  if (items.empty()) {
    renderCenteredWord("MENU");
    return;
  }

  if (selectedIndex >= items.size()) {
    selectedIndex = items.size() - 1;
  }

  String renderKey;
  renderKey.reserve(48 + batteryLabel_.length() + (items.size() * 16));
  renderKey += "menuv|";
  renderKey += String(selectedIndex);
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);
  for (const String &item : items) {
    renderKey += "|";
    renderKey += item;
  }

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const size_t itemCount = items.size();
  const size_t visibleCount =
      std::min(itemCount, static_cast<size_t>(std::max(1, virtualHeight / kCompactMenuRowHeight)));
  size_t firstVisible = 0;
  if (selectedIndex >= visibleCount / 2) {
    firstVisible = selectedIndex - visibleCount / 2;
  }
  if (firstVisible + visibleCount > itemCount) {
    firstVisible = itemCount - visibleCount;
  }

  const int rowHeight = kCompactMenuRowHeight;
  const int totalHeight = rowHeight * static_cast<int>(visibleCount);
  int y = std::max(0, (virtualHeight - totalHeight) / 2);

  clearVirtualBuffer(virtualWidth, virtualHeight);

  for (size_t row = 0; row < visibleCount; ++row) {
    const size_t itemIndex = firstVisible + row;
    const bool selected = itemIndex == selectedIndex;
    const uint16_t color = selected ? focusColor() : dimColor();
    // Keep the row inside the inscribed circle on round panels: pull the left edge in and shrink the
    // available width by the row's circular inset (worst case over the row's vertical extent).
    const int rowInset = std::max(circularSafeInsetForY(y + 3),
                                  circularSafeInsetForY(y + 3 + kTinyGlyphHeight * kTinyScale));
    const int textX = std::max(kCompactMenuX, rowInset + 4);
    const int maxWidth = std::max(16, (virtualWidth - rowInset) - textX);
    if (selected) {
      fillVirtualRect(std::max(10, rowInset), y + 2, 5, kTinyGlyphHeight * kTinyScale + 2,
                      selectedBarColor());
    }
    drawTinyTextAt(fitTinyText(items[itemIndex], maxWidth, kTinyScale), textX, y + 3, color,
                   kTinyScale);
    y += rowHeight;
  }

  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderLibrary(const std::vector<LibraryItem> &items, size_t selectedIndex) {
  if (items.empty()) {
    renderCenteredWord("LIBRARY");
    return;
  }

  if (selectedIndex >= items.size()) {
    selectedIndex = items.size() - 1;
  }

  String renderKey;
  renderKey.reserve(48 + batteryLabel_.length() + (items.size() * 32));
  renderKey += "library|";
  renderKey += String(selectedIndex);
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);
  for (const LibraryItem &item : items) {
    renderKey += "|";
    renderKey += item.title;
    renderKey += "~";
    renderKey += item.subtitle;
  }

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const size_t itemCount = items.size();
  const int usableHeight = std::max(kLibraryRowHeight, virtualHeight - (2 * kLibraryScreenPaddingY));
  const size_t visibleCount =
      std::min(itemCount, static_cast<size_t>(std::max(1, usableHeight / kLibraryRowHeight)));
  size_t firstVisible = 0;
  if (selectedIndex >= visibleCount / 2) {
    firstVisible = selectedIndex - visibleCount / 2;
  }
  if (firstVisible + visibleCount > itemCount) {
    firstVisible = itemCount - visibleCount;
  }

  const int totalHeight = kLibraryRowHeight * static_cast<int>(visibleCount);
  int y = std::max(kLibraryScreenPaddingY, (virtualHeight - totalHeight) / 2);

  clearVirtualBuffer(virtualWidth, virtualHeight);

  for (size_t row = 0; row < visibleCount; ++row) {
    const size_t itemIndex = firstVisible + row;
    const LibraryItem &item = items[itemIndex];
    const bool selected = itemIndex == selectedIndex;
    const uint16_t titleColor = selected ? focusColor() : wordColor();
    const uint16_t subtitleColor = blendOverBackground(titleColor, kLibrarySubtitleAlpha);
    const int rowY = y + static_cast<int>(row) * kLibraryRowHeight;
    // Pull each row inside the inscribed circle on round panels (worst case over the row height).
    const int rowInset = std::max(circularSafeInsetForY(rowY + 3),
                                  circularSafeInsetForY(rowY + kLibraryRowHeight - 3));
    const int textX = std::max(kLibraryInsetX, rowInset + 4);
    const int maxWidth = std::max(16, (virtualWidth - rowInset) - textX);

    if (selected) {
      fillVirtualRect(std::max(10, rowInset), rowY + 3, 5, kLibraryRowHeight - 6, selectedBarColor());
    }

    const String title = fitTinyText(item.title, maxWidth, kTinyScale);
    if (item.subtitle.isEmpty()) {
      drawTinyTextAt(title, textX, rowY + 12, titleColor, kTinyScale);
      continue;
    }

    drawTinyTextAt(title, textX, rowY + kLibraryTitleYOffset, titleColor, kTinyScale);
    drawTinyTextAt(fitTinyText(item.subtitle, maxWidth, kTinyScale), textX,
                   rowY + kLibrarySubtitleYOffset, subtitleColor, kTinyScale);
  }

  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderTextEntry(const String &title, const String &prompt, const String &value,
                                     const String &helperText,
                                     const std::vector<Button> &buttons) {
  String renderKey;
  renderKey.reserve(80 + title.length() + prompt.length() + value.length() + helperText.length() +
                    batteryLabel_.length() + (buttons.size() * 28));
  renderKey += "text-entry|";
  renderKey += title;
  renderKey += "|";
  renderKey += prompt;
  renderKey += "|";
  renderKey += value;
  renderKey += "|";
  renderKey += helperText;
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);
  for (const Button &button : buttons) {
    renderKey += "|";
    renderKey += button.label;
    renderKey += "@";
    renderKey += String(button.x);
    renderKey += ",";
    renderKey += String(button.y);
    renderKey += ",";
    renderKey += String(button.width);
    renderKey += ",";
    renderKey += String(button.height);
    renderKey += ",";
    renderKey += String(button.accent ? 1 : 0);
    renderKey += ",";
    renderKey += String(button.active ? 1 : 0);
  }

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const String headerText = title.isEmpty() ? helperText : title;
  const int fieldHeight = 28;
  // On round panels the top of the screen is too narrow for a full-width field, so drop the
  // title + field into the wider middle band (just above the keyboard) and inset the field to the
  // inscribed circle. Rectangular panels keep the original top layout.
  const bool roundPanel = Board::Config::DISPLAY_IS_ROUND;
  const int headerY = roundPanel ? 86 : 4;
  const int fieldY = roundPanel ? 110 : (headerText.isEmpty() ? 8 : 14);
  const int fieldInset =
      roundPanel ? std::max(10, roundScreenEdgeInset(fieldY)) : 10;
  const int fieldX = fieldInset;
  const int fieldWidth = virtualWidth - (2 * fieldX);
  constexpr uint8_t kFieldTextScalePercent = 36;
  const int fieldTextHeight = scaledPercentDimension(
      baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(value.isEmpty() ? prompt : value)),
      kFieldTextScalePercent);
  const int fieldTextY = fieldY + std::max(1, (fieldHeight - fieldTextHeight) / 2);

  clearVirtualBuffer(virtualWidth, virtualHeight);

  if (!headerText.isEmpty()) {
    drawTinyTextCentered(fitTinyText(headerText, virtualWidth - 20, 1), headerY, footerColor(), 1);
  }

  fillVirtualRect(fieldX, fieldY, fieldWidth, fieldHeight, dimColor());
  fillVirtualRect(fieldX + 1, fieldY + 1, fieldWidth - 2, fieldHeight - 2, backgroundColor());
  if (value.isEmpty()) {
    if (!prompt.isEmpty()) {
      const String placeholder =
          fitSerifTextScaled(prompt, fieldWidth - 16, kFieldTextScalePercent);
      drawSerifTextScaledAt(placeholder, fieldX + 8, fieldTextY, dimColor(), kFieldTextScalePercent);
    }
  } else {
    const String fieldValue =
        fitSerifTextTrailingScaled(value, fieldWidth - 16, kFieldTextScalePercent);
    drawSerifTextScaledAt(fieldValue, fieldX + 8, fieldTextY, wordColor(), kFieldTextScalePercent);
  }

  for (const Button &button : buttons) {
    if (button.width <= 2 || button.height <= 2) {
      continue;
    }

    const uint16_t borderColor =
        button.active ? selectedBarColor() : (button.accent ? focusColor() : dimColor());
    uint16_t fillColor = backgroundColor();
    if (button.active) {
      fillColor = blendOverBackground(borderColor, nightMode_ ? 128 : 40);
    } else if (button.accent) {
      fillColor = blendOverBackground(borderColor, nightMode_ ? 92 : 24);
    }

    fillVirtualRect(button.x, button.y, button.width, button.height, borderColor);
    fillVirtualRect(button.x + 1, button.y + 1, button.width - 2, button.height - 2, fillColor);

    const bool singleAsciiLetter =
        button.label.length() == 1 &&
        ((button.label[0] >= 'a' && button.label[0] <= 'z') ||
         (button.label[0] >= 'A' && button.label[0] <= 'Z'));
    const uint8_t labelScalePercent = singleAsciiLetter ? 42 : 26;
    const String label =
        fitSerifTextScaled(button.label, std::max(0, static_cast<int>(button.width) - 8),
                           labelScalePercent);
    const int labelWidth = measureSerifTextWidthScaled(label, labelScalePercent);
    const int labelHeight = scaledPercentDimension(
        baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(label)), labelScalePercent);
    const int textX =
        static_cast<int>(button.x) + std::max(0, (static_cast<int>(button.width) - labelWidth) / 2);
    const int textY =
        static_cast<int>(button.y) +
        std::max(1, (static_cast<int>(button.height) - labelHeight) / 2);
    if (singleAsciiLetter) {
      drawSerifTextScaledAt(label, textX, textY, wordColor(), labelScalePercent);
      continue;
    }

    if (!label.isEmpty()) {
      drawSerifTextScaledAt(label, textX, textY, wordColor(), labelScalePercent);
      continue;
    }

    const int fallbackScale = kTinyScale;
    const String fallbackLabel =
        fitTinyText(button.label, std::max(0, static_cast<int>(button.width) - 6), fallbackScale);
    const int fallbackWidth = measureTinyTextWidth(fallbackLabel, fallbackScale);
    const int fallbackX = static_cast<int>(button.x) +
                          std::max(0, (static_cast<int>(button.width) - fallbackWidth) / 2);
    const int fallbackY = static_cast<int>(button.y) +
                          std::max(1, (static_cast<int>(button.height) -
                                       (kTinyGlyphHeight * fallbackScale)) /
                                          2);
    drawTinyTextAt(fallbackLabel, fallbackX, fallbackY, wordColor(), fallbackScale);
  }

  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderStatus(const String &title, const String &line1, const String &line2) {
  const String renderKey = "status|" + title + "|" + line1 + "|" + line2 + "|b:" +
                           batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
                           String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(title));
  const int titleY = std::max(0, (virtualHeight - glyphHeight) / 2 - 26);
  const int line1Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              titleY + glyphHeight + 22);
  const int line2Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              line1Y + kTinyGlyphHeight * kTinyScale + 10);

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawWordLine(title, titleY, wordColor());
  if (!line1.isEmpty()) {
    drawTinyTextCentered(line1, line1Y, dimColor(), kTinyScale);
  }
  if (!line2.isEmpty()) {
    drawTinyTextCentered(line2, line2Y, focusColor(), kTinyScale);
  }
  drawBatteryBadge();

  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderProgress(const String &title, const String &line1, const String &line2,
                                    int progressPercent) {
  progressPercent = std::max(-1, std::min(100, progressPercent));
  const String renderKey =
      "progress|" + title + "|" + line1 + "|" + line2 + "|" + String(progressPercent) +
      "|b:" + batteryLabel_ + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
      String(nightMode_ ? 1 : 0);
  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = kDisplayWidth;
  const int virtualHeight = kDisplayHeight;
  const int glyphHeight = baseGlyphHeightForTypeface(effectiveReaderTypefaceForText(title));
  const int titleY = std::max(0, (virtualHeight - glyphHeight) / 2 - 34);
  const int line1Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              titleY + glyphHeight + 18);
  const int line2Y = std::min(virtualHeight - kTinyGlyphHeight * kTinyScale,
                              line1Y + kTinyGlyphHeight * kTinyScale + 10);
  const int barWidth = std::min(300, virtualWidth - 48);
  const int barHeight = 8;
  const int barX = std::max(0, (virtualWidth - barWidth) / 2);
  const int barY = std::min(virtualHeight - barHeight - 8,
                            line2Y + kTinyGlyphHeight * kTinyScale + 14);

  clearVirtualBuffer(virtualWidth, virtualHeight);
  drawWordLine(title, titleY, wordColor());
  if (!line1.isEmpty()) {
    drawTinyTextCentered(line1, line1Y, dimColor(), kTinyScale);
  }
  if (!line2.isEmpty()) {
    drawTinyTextCentered(line2, line2Y, focusColor(), kTinyScale);
  }

  if (progressPercent >= 0) {
    fillVirtualRect(barX, barY, barWidth, barHeight, dimColor());
    fillVirtualRect(barX + 1, barY + 1, barWidth - 2, barHeight - 2, backgroundColor());
    const int fillWidth = std::max(1, ((barWidth - 2) * progressPercent) / 100);
    fillVirtualRect(barX + 1, barY + 1, fillWidth, barHeight - 2, focusColor());
  }

  drawBatteryBadge();
  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderLifeScreensaver(const std::vector<uint32_t> &cells, uint16_t columns,
                                           uint16_t rows, uint32_t generation,
                                           const std::vector<uint32_t> *dimCells) {
  const String renderKey = "life|" + String(generation) + "|" + String(columns) + "|" +
                           String(rows) + "|d:" + String(darkMode_ ? 1 : 0) + "|n:" +
                           String(nightMode_ ? 1 : 0) + "|w0:" +
                           String(cells.empty() ? 0UL : static_cast<unsigned long>(cells[0])) +
                           "|w1:" +
                           String(cells.empty()
                                      ? 0UL
                                      : static_cast<unsigned long>(cells[cells.size() - 1])) +
                           "|dw0:" +
                           String((dimCells == nullptr || dimCells->empty())
                                      ? 0UL
                                      : static_cast<unsigned long>((*dimCells)[0]));
  if (!initialized_ || renderKey == lastRenderKey_ || columns == 0 || rows == 0) {
    return;
  }

  lastRenderKey_ = renderKey;

  const int scale = 1;
  const int virtualWidth = logicalWidth();
  const int virtualHeight = logicalHeight();
  const int cellSize =
      std::max(1, std::min(virtualWidth / static_cast<int>(columns),
                           virtualHeight / static_cast<int>(rows)));
  const int renderWidth = std::min(virtualWidth, static_cast<int>(columns) * cellSize);
  const int renderHeight = std::min(virtualHeight, static_cast<int>(rows) * cellSize);
  const int xOffset = std::max(0, (virtualWidth - renderWidth) / 2);
  const int yOffset = std::max(0, (virtualHeight - renderHeight) / 2);
  const uint16_t lifeColor = panelColor(wordColor());
  const uint16_t dimLifeColor = panelColor(blendOverBackground(wordColor(), nightMode_ ? 82 : 96));

  clearVirtualBuffer(virtualWidth, virtualHeight);
  auto drawPackedCells = [&](const std::vector<uint32_t> &source, uint16_t color) {
    for (int y = 0; y < static_cast<int>(rows); ++y) {
      const int dstY = yOffset + y * cellSize;
      if (dstY >= yOffset + renderHeight) {
        break;
      }
      for (int x = 0; x < static_cast<int>(columns); ++x) {
        const int dstX = xOffset + x * cellSize;
        if (dstX >= xOffset + renderWidth) {
          break;
        }
        const size_t index = static_cast<size_t>(y) * columns + static_cast<size_t>(x);
        if (!packedLifeCellAlive(source, index)) {
          continue;
        }

        const int blockWidth = std::min(cellSize, xOffset + renderWidth - dstX);
        const int blockHeight = std::min(cellSize, yOffset + renderHeight - dstY);
        for (int blockY = 0; blockY < blockHeight; ++blockY) {
          uint16_t *row = virtualFrame_ + (dstY + blockY) * kVirtualBufferWidth + dstX;
          std::fill_n(row, blockWidth, color);
        }
      }
    }
  };

  if (dimCells != nullptr) {
    drawPackedCells(*dimCells, dimLifeColor);
  }
  drawPackedCells(cells, lifeColor);

  flushScaledFrame(scale, virtualWidth, virtualHeight);
}

void DisplayManager::renderShuliScreen(int mood, const String &status, const String &stat,
                                       uint8_t goalPercent) {
  const int w = logicalWidth();
  const int h = logicalHeight();

  String key = "shuli|" + String(mood) + "|" + status + "|" + stat + "|" +
               String(static_cast<int>(goalPercent)) + "|o:" +
               String(static_cast<int>(uiOrientation_)) + "|b:" + batteryLabel_;
  if (!initialized_ || key == lastRenderKey_) {
    return;
  }
  lastRenderKey_ = key;

  // Shuli's cozy room is always dark so her orange/white fur pops (and it saves AMOLED power).
  fillVirtualRect(0, 0, w, h, kTrueBlack);

  const uint16_t furOrange = rgb565(245, 150, 70);
  const uint16_t furWhite = rgb565(245, 238, 226);
  const uint16_t pink = rgb565(242, 158, 168);
  const uint16_t pinkDk = rgb565(225, 120, 140);
  const uint16_t dark = rgb565(35, 28, 24);
  const uint16_t gold = rgb565(240, 200, 90);
  const uint16_t tear = rgb565(120, 180, 235);
  const uint16_t sickTint = rgb565(150, 200, 130);
  const uint16_t whisker = rgb565(220, 220, 220);
  const uint16_t bag = rgb565(120, 120, 140);

  auto disc = [&](int cx, int cy, int r, uint16_t color) {
    if (r < 1) r = 1;
    for (int dy = -r; dy <= r; ++dy) {
      const int half = static_cast<int>(sqrtf(static_cast<float>(r * r - dy * dy)));
      fillVirtualRect(cx - half, cy + dy, 2 * half + 1, 1, color);
    }
  };
  auto ellipse = [&](int cx, int cy, int rx, int ry, uint16_t color) {
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    for (int dy = -ry; dy <= ry; ++dy) {
      float t = 1.0f - static_cast<float>(dy * dy) / static_cast<float>(ry * ry);
      if (t < 0) t = 0;
      const int half = static_cast<int>(rx * sqrtf(t));
      fillVirtualRect(cx - half, cy + dy, 2 * half + 1, 1, color);
    }
  };
  auto triUp = [&](int cx, int apexY, int hb, int height, uint16_t color) {
    if (height < 1) height = 1;
    for (int r = 0; r <= height; ++r) {
      const int half = hb * r / height;
      fillVirtualRect(cx - half, apexY + r, 2 * half + 1, 1, color);
    }
  };
  auto triDown = [&](int cx, int topY, int hb, int height, uint16_t color) {
    if (height < 1) height = 1;
    for (int r = 0; r <= height; ++r) {
      const int half = hb * (height - r) / height;
      fillVirtualRect(cx - half, topY + r, 2 * half + 1, 1, color);
    }
  };
  auto line = [&](int x0, int y0, int x1, int y1, int thick, uint16_t color) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy;
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    const int t = thick < 1 ? 1 : thick;
    for (;;) {
      fillVirtualRect(x0 - t / 2, y0 - t / 2, t, t, color);
      if (x0 == x1 && y0 == y1) break;
      const int e2 = 2 * err;
      if (e2 > -dy) { err -= dy; x0 += sx; }
      if (e2 < dx) { err += dx; y0 += sy; }
    }
  };
  auto heart = [&](int hx, int hy, int s, uint16_t color) {
    disc(hx - s / 2, hy, s / 2, color);
    disc(hx + s / 2, hy, s / 2, color);
    triDown(hx, hy, s, s * 3 / 2, color);
  };

  const int cx = w / 2;
  const int R = std::max(28, static_cast<int>(w * 0.16f));
  const int headY = static_cast<int>(h * 0.40f);
  const int eo = static_cast<int>(R * 0.42f);
  const int eyeY = headY - static_cast<int>(R * 0.05f);
  const int er = std::max(4, static_cast<int>(R * 0.20f));
  const int th = std::max(3, R / 12);
  const int mx = cx;
  const int my = headY + static_cast<int>(R * 0.5f);

  drawTinyTextCentered("Poopik", static_cast<int>(h * 0.06f), gold, 3);

  // Body, tail, then ears behind the head.
  const int bodyCY = headY + static_cast<int>(R * 1.15f);
  ellipse(cx + static_cast<int>(R * 1.15f), bodyCY + R / 3, static_cast<int>(R * 0.5f),
          static_cast<int>(R * 0.22f), furOrange);
  ellipse(cx, bodyCY, static_cast<int>(R * 1.25f), static_cast<int>(R * 0.95f), furWhite);
  ellipse(cx + R / 2, bodyCY, static_cast<int>(R * 0.5f), static_cast<int>(R * 0.6f), furOrange);

  const int earApexY = headY - static_cast<int>(R * 1.15f);
  const int earH = static_cast<int>(R * 0.85f);
  const int earHB = static_cast<int>(R * 0.55f);
  triUp(cx - static_cast<int>(R * 0.62f), earApexY, earHB, earH, furOrange);
  triUp(cx + static_cast<int>(R * 0.62f), earApexY, earHB, earH, furOrange);
  triUp(cx - static_cast<int>(R * 0.62f), earApexY + earH / 4, earHB / 2, earH / 2, pink);
  triUp(cx + static_cast<int>(R * 0.62f), earApexY + earH / 4, earHB / 2, earH / 2, pink);

  // Head + white muzzle + forehead blaze.
  disc(cx, headY, R, furOrange);
  ellipse(cx, headY + static_cast<int>(R * 0.35f), static_cast<int>(R * 0.7f),
          static_cast<int>(R * 0.5f), furWhite);
  ellipse(cx, headY - static_cast<int>(R * 0.1f), static_cast<int>(R * 0.22f),
          static_cast<int>(R * 0.6f), furWhite);

  // Whiskers (both sides).
  for (int i = -1; i <= 1; ++i) {
    line(cx - er, my - er / 3 + i * er / 2, cx - eo - er, my - er / 2 + i * er, 2, whisker);
    line(cx + er, my - er / 3 + i * er / 2, cx + eo + er, my - er / 2 + i * er, 2, whisker);
  }

  // Nose.
  triDown(cx, headY + static_cast<int>(R * 0.26f), R / 12, R / 12, pinkDk);

  const int eL = cx - eo;
  const int eR = cx + eo;
  switch (mood) {
    case 0:  // Happy: closed ^^ eyes, big smile, blush, hearts
      line(eL - er, eyeY + er / 3, eL, eyeY - er / 3, th, dark);
      line(eL, eyeY - er / 3, eL + er, eyeY + er / 3, th, dark);
      line(eR - er, eyeY + er / 3, eR, eyeY - er / 3, th, dark);
      line(eR, eyeY - er / 3, eR + er, eyeY + er / 3, th, dark);
      line(mx - er, my, mx, my + er / 2, th, dark);
      line(mx, my + er / 2, mx + er, my, th, dark);
      disc(eL - er / 2, my - er / 4, er / 2, pink);
      disc(eR + er / 2, my - er / 4, er / 2, pink);
      heart(cx - R, headY - R, R / 5, pinkDk);
      heart(cx + R, headY - R, R / 5, pinkDk);
      break;
    case 1:  // Needy: big pleading eyes, tiny worried mouth, blush
      disc(eL, eyeY, er, furWhite); disc(eL, eyeY, er * 7 / 10, dark);
      disc(eL - er / 4, eyeY - er / 4, er * 3 / 10, furWhite);
      disc(eR, eyeY, er, furWhite); disc(eR, eyeY, er * 7 / 10, dark);
      disc(eR - er / 4, eyeY - er / 4, er * 3 / 10, furWhite);
      disc(mx, my, er / 3, dark);
      disc(eL - er / 2, my - er / 4, er / 2, pink);
      disc(eR + er / 2, my - er / 4, er / 2, pink);
      break;
    case 2:  // Grumpy: angry brows, narrowed eyes, frown
      line(eL - er, eyeY - er, eL + er / 2, eyeY - er / 3, th, dark);
      line(eR + er, eyeY - er, eR - er / 2, eyeY - er / 3, th, dark);
      fillVirtualRect(eL - er / 2, eyeY, er, th, dark);
      fillVirtualRect(eR - er / 2, eyeY, er, th, dark);
      line(mx - er, my + er / 3, mx, my - er / 4, th, dark);
      line(mx, my - er / 4, mx + er, my + er / 3, th, dark);
      break;
    case 3:  // Sad: droopy eyes, raised-outer brows, tear, frown
      disc(eL, eyeY + er / 4, er * 8 / 10, furWhite); disc(eL, eyeY + er / 4, er / 2, dark);
      disc(eR, eyeY + er / 4, er * 8 / 10, furWhite); disc(eR, eyeY + er / 4, er / 2, dark);
      line(eL - er, eyeY - er, eL, eyeY - er / 2, th, dark);
      line(eR + er, eyeY - er, eR, eyeY - er / 2, th, dark);
      triDown(eL, eyeY + er, er / 4, er * 3 / 4, tear);
      line(mx - er, my + er / 3, mx, my - er / 4, th, dark);
      line(mx, my - er / 4, mx + er, my + er / 3, th, dark);
      break;
    case 4:  // Sick: X eyes, greenish cheeks, flat mouth, sweat
      line(eL - er / 2, eyeY - er / 2, eL + er / 2, eyeY + er / 2, th, dark);
      line(eL - er / 2, eyeY + er / 2, eL + er / 2, eyeY - er / 2, th, dark);
      line(eR - er / 2, eyeY - er / 2, eR + er / 2, eyeY + er / 2, th, dark);
      line(eR - er / 2, eyeY + er / 2, eR + er / 2, eyeY - er / 2, th, dark);
      disc(eL - er / 2, my - er / 4, er / 2, sickTint);
      disc(eR + er / 2, my - er / 4, er / 2, sickTint);
      fillVirtualRect(mx - er, my, 2 * er, th, dark);
      triDown(cx + static_cast<int>(R * 0.55f), headY - R / 3, er / 4, er * 3 / 4, tear);
      break;
    default:  // 5 Miserable: X eyes, eyebags, big frown, sick cheeks
      line(eL - er / 2, eyeY - er / 2, eL + er / 2, eyeY + er / 2, th, dark);
      line(eL - er / 2, eyeY + er / 2, eL + er / 2, eyeY - er / 2, th, dark);
      line(eR - er / 2, eyeY - er / 2, eR + er / 2, eyeY + er / 2, th, dark);
      line(eR - er / 2, eyeY + er / 2, eR + er / 2, eyeY - er / 2, th, dark);
      fillVirtualRect(eL - er, eyeY + er / 2, 2 * er, th, bag);
      fillVirtualRect(eR - er, eyeY + er / 2, 2 * er, th, bag);
      line(mx - er, my + er / 2, mx, my - er / 3, th, dark);
      line(mx, my - er / 3, mx + er, my + er / 2, th, dark);
      disc(eL - er / 2, my, er / 2, sickTint);
      disc(eR + er / 2, my, er / 2, sickTint);
      break;
  }

  // Posh little crown on top of her head (her snobby look), drawn last so it sits in front.
  const int crownW = static_cast<int>(R * 0.7f);
  const int bandH = std::max(3, R / 8);
  const int bandY = headY - static_cast<int>(R * 0.82f);
  fillVirtualRect(cx - crownW / 2, bandY, crownW, bandH, gold);
  triUp(cx - crownW / 2 + crownW / 6, bandY - R / 5, crownW / 8, R / 5, gold);
  triUp(cx, bandY - R / 4, crownW / 8, R / 4, gold);
  triUp(cx + crownW / 2 - crownW / 6, bandY - R / 5, crownW / 8, R / 5, gold);
  disc(cx, bandY + bandH / 2, std::max(2, R / 16), pinkDk);

  // Status text + daily goal bar.
  int ty = bodyCY + static_cast<int>(R * 1.1f);
  drawTinyTextCentered(status, ty, furWhite, 2);
  ty += kTinyGlyphHeight * 2 + 12;
  drawTinyTextCentered(stat, ty, focusColor(), 2);
  ty += kTinyGlyphHeight * 2 + 12;
  const int barW = static_cast<int>(w * 0.5f);
  const int barX = cx - barW / 2;
  const int barH = std::max(6, R / 12);
  fillVirtualRect(barX, ty, barW, barH, rgb565(60, 60, 60));
  fillVirtualRect(barX, ty, barW * std::min<int>(goalPercent, 100) / 100, barH, gold);

  flushScaledFrame(1, w, h);
}

void DisplayManager::renderFocusTimerScreen(const String &mode, const String &genre,
                                            const String &timer, const String &instruction,
                                            const String &footer, int progressPercent,
                                            bool breakAccent) {
  (void)genre;
  progressPercent = std::max(-1, std::min(100, progressPercent));
  const int virtualWidth = logicalWidth();
  const int virtualHeight = logicalHeight();
  const bool portrait = isPortraitOrientation(uiOrientation_);
  const bool timerRunning = progressPercent >= 0;

  String renderKey;
  renderKey.reserve(80 + mode.length() + genre.length() + timer.length() + instruction.length() +
                    footer.length() + batteryLabel_.length());
  renderKey += "timer|";
  renderKey += mode;
  renderKey += "|";
  renderKey += genre;
  renderKey += "|";
  renderKey += timer;
  renderKey += "|";
  renderKey += instruction;
  renderKey += "|";
  renderKey += footer;
  renderKey += "|";
  renderKey += String(progressPercent);
  renderKey += "|o:";
  renderKey += String(static_cast<int>(uiOrientation_));
  renderKey += "|ba:";
  renderKey += String(breakAccent ? 1 : 0);
  renderKey += "|b:";
  renderKey += batteryLabel_;
  renderKey += "|d:";
  renderKey += String(darkMode_ ? 1 : 0);
  renderKey += "|n:";
  renderKey += String(nightMode_ ? 1 : 0);

  if (!initialized_ || renderKey == lastRenderKey_) {
    return;
  }

  lastRenderKey_ = renderKey;

  clearVirtualBuffer(virtualWidth, virtualHeight);

  const uint16_t accent = breakAccent ? focusTimerBreakColor() : focusColor();
  const uint16_t baseTextColor = wordColor();
  const uint16_t inverseTextColor = nightMode_ || darkMode_ ? wordColor() : kPureWhite;
  const uint16_t instructionColor = accent;
  int fillX = 0;
  int fillY = 0;
  int fillWidth = 0;
  int fillHeight = 0;
  if (timerRunning && progressPercent > 0) {
    if (portrait) {
      fillHeight = std::max(1, (virtualHeight * progressPercent) / 100);
      fillVirtualRect(0, virtualHeight - fillHeight, virtualWidth, fillHeight, accent);
      fillY = virtualHeight - fillHeight;
      fillWidth = virtualWidth;
    } else {
      fillWidth = std::max(1, (virtualWidth * progressPercent) / 100);
      fillVirtualRect(0, 0, fillWidth, virtualHeight, accent);
      fillHeight = virtualHeight;
    }
  }

  const int sidePadding = portrait ? 12 : 20;
  const int contentX = sidePadding;
  const int contentWidth = std::max(0, virtualWidth - (sidePadding * 2));
  auto wrapTinyLines = [&](const String &text, int maxWidth, int scale) {
    std::vector<String> lines;
    if (text.isEmpty() || maxWidth <= 0) {
      return lines;
    }

    auto fits = [&](const String &candidate) {
      return measureTinyTextWidth(candidate, scale) <= maxWidth;
    };

    auto appendBrokenWord = [&](const String &word, String &currentLine) {
      String segment;
      for (size_t i = 0; i < word.length(); ++i) {
        const String candidate = segment + word[i];
        if (!segment.isEmpty() && !fits(candidate)) {
          if (!currentLine.isEmpty()) {
            lines.push_back(currentLine);
            currentLine = "";
          }
          lines.push_back(segment);
          segment = String(word[i]);
        } else {
          segment = candidate;
        }
      }

      if (!segment.isEmpty()) {
        currentLine = segment;
      }
    };

    String currentLine;
    size_t index = 0;
    while (index < text.length()) {
      while (index < text.length() && text[index] == ' ') {
        ++index;
      }
      if (index >= text.length()) {
        break;
      }

      if (text[index] == '\n') {
        if (!currentLine.isEmpty()) {
          lines.push_back(currentLine);
          currentLine = "";
        }
        ++index;
        continue;
      }

      const size_t start = index;
      while (index < text.length() && text[index] != ' ' && text[index] != '\n') {
        ++index;
      }
      const String word = text.substring(start, index);
      if (currentLine.isEmpty()) {
        if (fits(word)) {
          currentLine = word;
        } else {
          appendBrokenWord(word, currentLine);
        }
        continue;
      }

      const String candidate = currentLine + " " + word;
      if (fits(candidate)) {
        currentLine = candidate;
      } else {
        lines.push_back(currentLine);
        currentLine = "";
        if (fits(word)) {
          currentLine = word;
        } else {
          appendBrokenWord(word, currentLine);
        }
      }
    }

    if (!currentLine.isEmpty()) {
      lines.push_back(currentLine);
    }

    return lines;
  };

  auto drawTinyTextAtClipped = [&](const String &text, int x, int y, uint16_t color, int scale,
                                   int clipX, int clipY, int clipWidth, int clipHeight) {
    if (clipWidth <= 0 || clipHeight <= 0) {
      return;
    }

    const int clipXEnd = clipX + clipWidth;
    const int clipYEnd = clipY + clipHeight;
    const uint16_t panel = panelColor(color);
    int cursorX = x;
    for (size_t i = 0; i < text.length(); ++i) {
      const uint8_t *rows = tinyRowsFor(text[i]);
      for (int row = 0; row < kTinyGlyphHeight; ++row) {
        for (int col = 0; col < kTinyGlyphWidth; ++col) {
          if ((rows[row] & (1 << (kTinyGlyphWidth - 1 - col))) == 0) {
            continue;
          }

          for (int yy = 0; yy < scale; ++yy) {
            const int dstY = y + row * scale + yy;
            if (dstY < 0 || dstY >= kVirtualBufferHeight || dstY < clipY || dstY >= clipYEnd) {
              continue;
            }

            for (int xx = 0; xx < scale; ++xx) {
              const int dstX = cursorX + col * scale + xx;
              if (dstX < 0 || dstX >= kVirtualBufferWidth || dstX < clipX || dstX >= clipXEnd) {
                continue;
              }
              virtualFrame_[dstY * kVirtualBufferWidth + dstX] = panel;
            }
          }
        }
      }
      cursorX += (kTinyGlyphWidth + kTinyGlyphSpacing) * scale;
    }
  };

  auto drawTinyTextAt180Clipped = [&](const String &text, int x, int y, uint16_t color, int scale,
                                      int clipX, int clipY, int clipWidth, int clipHeight) {
    if (clipWidth <= 0 || clipHeight <= 0) {
      return;
    }

    const int clipXEnd = clipX + clipWidth;
    const int clipYEnd = clipY + clipHeight;
    const uint16_t panel = panelColor(color);
    const int count = static_cast<int>(text.length());
    for (int charIndex = 0; charIndex < count; ++charIndex) {
      const int charBaseX =
          x + (count - 1 - charIndex) * (kTinyGlyphWidth + kTinyGlyphSpacing) * scale;
      const uint8_t *rows = tinyRowsFor(text[charIndex]);
      for (int row = 0; row < kTinyGlyphHeight; ++row) {
        const int dstRow = kTinyGlyphHeight - 1 - row;
        for (int col = 0; col < kTinyGlyphWidth; ++col) {
          if ((rows[row] & (1 << (kTinyGlyphWidth - 1 - col))) == 0) {
            continue;
          }
          const int dstCol = kTinyGlyphWidth - 1 - col;
          for (int yy = 0; yy < scale; ++yy) {
            const int dstY = y + dstRow * scale + yy;
            if (dstY < 0 || dstY >= kVirtualBufferHeight || dstY < clipY || dstY >= clipYEnd) {
              continue;
            }
            for (int xx = 0; xx < scale; ++xx) {
              const int dstX = charBaseX + dstCol * scale + xx;
              if (dstX < 0 || dstX >= kVirtualBufferWidth || dstX < clipX ||
                  dstX >= clipXEnd) {
                continue;
              }
              virtualFrame_[dstY * kVirtualBufferWidth + dstX] = panel;
            }
          }
        }
      }
    }
  };

  auto centeredXForTiny = [&](const String &text, int scale) {
    const int textWidth = measureTinyTextWidth(text, scale);
    return std::max(contentX, contentX + ((contentWidth - textWidth) / 2));
  };

  auto centeredXWithin = [&](const String &text, int scale, int blockX, int blockWidth) {
    const int textWidth = measureTinyTextWidth(text, scale);
    return std::max(blockX, blockX + ((blockWidth - textWidth) / 2));
  };

  auto tinyTextHeight = [](int scale) { return kTinyGlyphHeight * scale; };

  auto fitTinyScale = [&](const String &text, int scale, int maxWidth) {
    while (scale > 1 && measureTinyTextWidth(text, scale) > maxWidth) {
      --scale;
    }
    return scale;
  };

  auto clampTextY = [&](int y, int scale) {
    return std::max(0, std::min(y, virtualHeight - tinyTextHeight(scale)));
  };

  const bool compactTimerLayout = virtualWidth <= 480 && virtualHeight <= 480;
  const bool portraitFocusLayout = portrait && !breakAccent && (mode == "BEGIN" || mode == "WORK");
  int titleScale = portrait ? 5 : 7;
  if (portraitFocusLayout && timerRunning) {
    titleScale = 4;
  }
  if (compactTimerLayout) {
    titleScale = portrait ? 4 : 5;
    if (timerRunning && !portrait && virtualWidth >= 470 && virtualHeight >= 470) {
      titleScale = 6;
    }
  }
  titleScale = fitTinyScale(mode, titleScale, contentWidth);

  if (timerRunning) {
    int timerScale = portrait ? 5 : 7;
    if (compactTimerLayout) {
      timerScale = portrait ? 5 : 6;
      if (virtualWidth >= 470 && virtualHeight >= 470) {
        timerScale = portrait ? 6 : 7;
      }
    }
    timerScale = fitTinyScale(timer, timerScale, contentWidth);

    int titleY = portrait ? 120 : 24;
    int timerY = portrait ? 352 : 88;
    int footerScale = titleScale;
    int footerY = -1;
    int footerX = 0;
    if (portraitFocusLayout) {
      titleY = 92;
      timerY = 306;
    }
    if (compactTimerLayout) {
      titleY = std::max(26, virtualHeight / (portrait ? 6 : 9));
      timerY = (virtualHeight - tinyTextHeight(timerScale)) / 2;
    }
    if (!footer.isEmpty()) {
      footerScale = fitTinyScale(footer, footerScale, contentWidth);
      footerY = virtualHeight - titleY - tinyTextHeight(footerScale);
      footerY = clampTextY(footerY, footerScale);
      footerX = centeredXForTiny(footer, footerScale);
    }
    if (compactTimerLayout && footerY >= 0) {
      const int minTimerFooterGap = portrait ? 34 : 24;
      timerY = std::min(timerY, footerY - minTimerFooterGap - tinyTextHeight(timerScale));
    }
    timerY = clampTextY(timerY, timerScale);
    titleY = clampTextY(titleY, titleScale);

    if (portraitFocusLayout) {
      const int dividerWidth =
          std::min(contentWidth, 40 + (static_cast<int>(mode.length()) * 12));
      const int dividerX = contentX + ((contentWidth - dividerWidth) / 2);
      const int dividerY =
          titleY + tinyTextHeight(titleScale) + (compactTimerLayout ? 16 : 20);
      fillVirtualRect(dividerX, dividerY, dividerWidth, 2, accent);
    }

    const int titleX = centeredXForTiny(mode, titleScale);
    const int timerX = centeredXForTiny(timer, timerScale);

    drawTinyTextAt(mode, titleX, titleY, baseTextColor, titleScale);
    drawTinyTextAt(timer, timerX, timerY, accent, timerScale);

    if (footerY >= 0) {
      drawTinyTextAt180(footer, footerX, footerY, baseTextColor, footerScale);
      if (fillWidth > 0 && fillHeight > 0) {
        drawTinyTextAt180Clipped(footer, footerX, footerY, inverseTextColor, footerScale,
                                 fillX, fillY, fillWidth, fillHeight);
      }
      if (portraitFocusLayout) {
        const int footerDividerWidth =
            std::min(contentWidth, 40 + (static_cast<int>(footer.length()) * 12));
        const int footerDividerX = contentX + ((contentWidth - footerDividerWidth) / 2);
        const int footerDividerY = footerY - (compactTimerLayout ? 16 : 22);
        fillVirtualRect(footerDividerX, footerDividerY, footerDividerWidth, 2, accent);
      }
    }

    if (fillWidth > 0 && fillHeight > 0) {
      drawTinyTextAtClipped(mode, titleX, titleY, inverseTextColor, titleScale, fillX, fillY,
                            fillWidth, fillHeight);
      drawTinyTextAtClipped(timer, timerX, timerY, inverseTextColor, timerScale, fillX, fillY,
                            fillWidth, fillHeight);
    }
  } else {
    if (compactTimerLayout) {
      int instructionScale = portrait ? 2 : 3;
      int instructionBlockWidth = contentWidth;
      int instructionBlockX = contentX;
      if (portraitFocusLayout) {
        instructionBlockWidth = std::max(96, contentWidth - 18);
        instructionBlockX = contentX + ((contentWidth - instructionBlockWidth) / 2);
      }

      int timerStaticScale = portrait ? 4 : 5;
      timerStaticScale = fitTinyScale(timer, timerStaticScale, contentWidth);
      const std::vector<String> lines =
          wrapTinyLines(instruction, instructionBlockWidth, instructionScale);
      const int newlinePos = instruction.indexOf('\n');
      const String firstInstruction =
          newlinePos >= 0 ? instruction.substring(0, newlinePos) : instruction;
      const std::vector<String> firstLines =
          wrapTinyLines(firstInstruction, instructionBlockWidth, instructionScale);
      const size_t firstLineCount = newlinePos >= 0 ? firstLines.size() : 0;
      const int lineHeight = tinyTextHeight(instructionScale) + instructionScale + 4;
      const int titleHeight = tinyTextHeight(titleScale);
      const int timerHeight = timer.isEmpty() ? 0 : tinyTextHeight(timerStaticScale);
      const int dividerHeight = portraitFocusLayout ? 2 : 0;
      const int titleDividerGap = portraitFocusLayout ? 16 : 0;
      const int dividerContentGap = portraitFocusLayout ? 36 : 0;
      const int titleContentGap = portraitFocusLayout ? 0 : (portrait ? 30 : 28);
      const int timerInstructionGap = timer.isEmpty() ? 0 : 18;
      const int instructionHeight =
          lines.empty() ? 0 : static_cast<int>(lines.size()) * lineHeight;
      const int blockHeight = titleHeight + titleDividerGap + dividerHeight + dividerContentGap +
                              titleContentGap + timerHeight + timerInstructionGap +
                              instructionHeight;
      int y = std::max(24, (virtualHeight - blockHeight) / 2);

      drawTinyTextAt(mode, centeredXForTiny(mode, titleScale), y, baseTextColor, titleScale);
      y += titleHeight;

      if (portraitFocusLayout) {
        y += titleDividerGap;
        const int dividerWidth =
            std::min(contentWidth, 40 + (static_cast<int>(mode.length()) * 12));
        const int dividerX = contentX + ((contentWidth - dividerWidth) / 2);
        fillVirtualRect(dividerX, y, dividerWidth, dividerHeight, accent);
        y += dividerHeight + dividerContentGap;
      } else {
        y += titleContentGap;
      }

      if (!timer.isEmpty()) {
        drawTinyTextAt(timer, centeredXForTiny(timer, timerStaticScale), y, accent,
                       timerStaticScale);
        y += timerHeight + timerInstructionGap;
      }

      for (size_t i = 0; i < lines.size(); ++i) {
        const String &line = lines[i];
        const uint16_t lineColor = i < firstLineCount ? baseTextColor : instructionColor;
        drawTinyTextAt(line,
                       centeredXWithin(line, instructionScale, instructionBlockX,
                                       instructionBlockWidth),
                       y, lineColor, instructionScale);
        y += lineHeight;
      }
    } else {
      int titleY = portrait ? 176 : 42;
      int dividerY = 0;
      int instructionScale = portrait ? 2 : 3;
      int instructionBlockWidth = contentWidth;
      int instructionBlockX = contentX;

      if (portraitFocusLayout) {
        titleY = 126;
        instructionScale = 2;
        instructionBlockWidth = std::max(96, contentWidth - 18);
        instructionBlockX = contentX + ((contentWidth - instructionBlockWidth) / 2);
        dividerY = titleY + (kTinyGlyphHeight * titleScale) + 22;
        const int dividerWidth =
            std::min(contentWidth, 40 + (static_cast<int>(mode.length()) * 12));
        const int dividerX = contentX + ((contentWidth - dividerWidth) / 2);
        fillVirtualRect(dividerX, dividerY, dividerWidth, 2, accent);
      }

      drawTinyTextAt(mode, centeredXForTiny(mode, titleScale), titleY, baseTextColor,
                     titleScale);

      const int lineHeight = (kTinyGlyphHeight * instructionScale) + instructionScale + 4;
      int y = titleY + (kTinyGlyphHeight * titleScale) + (portrait ? 42 : 28);
      if (portraitFocusLayout) {
        y = dividerY + 66;
      }

      if (!timer.isEmpty()) {
        int timerStaticScale = portrait ? 4 : 5;
        while (timerStaticScale > 1 &&
               measureTinyTextWidth(timer, timerStaticScale) > contentWidth) {
          --timerStaticScale;
        }
        drawTinyTextAt(timer, centeredXForTiny(timer, timerStaticScale), y, accent,
                       timerStaticScale);
        y += (kTinyGlyphHeight * timerStaticScale) + timerStaticScale + 20;
      }

      if (!instruction.isEmpty()) {
        const std::vector<String> lines =
            wrapTinyLines(instruction, instructionBlockWidth, instructionScale);
        const int newlinePos = instruction.indexOf('\n');
        const String firstInstruction =
            newlinePos >= 0 ? instruction.substring(0, newlinePos) : instruction;
        const std::vector<String> firstLines =
            wrapTinyLines(firstInstruction, instructionBlockWidth, instructionScale);
        const size_t firstLineCount = newlinePos >= 0 ? firstLines.size() : 0;

        for (size_t i = 0; i < lines.size(); ++i) {
          const String &line = lines[i];
          const uint16_t lineColor = i < firstLineCount ? baseTextColor : instructionColor;
          drawTinyTextAt(line,
                         centeredXWithin(line, instructionScale, instructionBlockX,
                                         instructionBlockWidth),
                         y, lineColor, instructionScale);
          y += lineHeight;
        }
      }
    }
  }

  drawBatteryBadge(virtualWidth, virtualHeight);
  flushScaledFrame(1, virtualWidth, virtualHeight);
}
