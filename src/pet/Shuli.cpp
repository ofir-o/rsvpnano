#include "pet/Shuli.h"

#include <algorithm>

#include "board/BoardClock.h"

namespace {
constexpr uint32_t kGoalPresets[] = {1500, 2000, 3000, 5000};
constexpr uint8_t kMaxMissedDays = 200;

const char *kPrefGoal = "shuli_goal";
const char *kPrefWords = "shuli_words";
const char *kPrefDay = "shuli_day";
const char *kPrefMissed = "shuli_miss";
const char *kPrefMet = "shuli_met";
}  // namespace

// Days since 1970-01-01 (Howard Hinnant's days_from_civil); used only for date differences.
uint32_t ShuliPet::civilDays(uint16_t y, uint8_t m, uint8_t d) {
  int yy = static_cast<int>(y);
  yy -= (m <= 2) ? 1 : 0;
  const int era = (yy >= 0 ? yy : yy - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(yy - era * 400);
  const unsigned doy =
      (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + static_cast<unsigned>(d) - 1;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return static_cast<uint32_t>(era * 146097 + static_cast<int>(doe) - 719468);
}

uint32_t ShuliPet::todayDayNumber(bool &haveClock) const {
  haveClock = false;
  if (!Board::Clock::available()) {
    return 0;
  }
  Board::Clock::DateTime now;
  bool stopped = false;
  if (!Board::Clock::read(now, &stopped) || stopped) {
    return 0;
  }
  haveClock = true;
  return civilDays(now.year, now.month, now.day);
}

void ShuliPet::begin(Preferences *prefs) {
  prefs_ = prefs;
  if (prefs_ != nullptr) {
    goalWords_ = prefs_->getUInt(kPrefGoal, goalWords_);
    wordsToday_ = prefs_->getUInt(kPrefWords, 0);
    dayNumber_ = prefs_->getUInt(kPrefDay, 0);
    missedDays_ = prefs_->getUChar(kPrefMissed, 0);
    goalMetToday_ = prefs_->getBool(kPrefMet, false);
  }
  if (goalWords_ == 0) {
    goalWords_ = 1500;
  }
  dirty_ = false;
  update();
}

void ShuliPet::update() {
  bool haveClock = false;
  const uint32_t today = todayDayNumber(haveClock);
  if (!haveClock || today == 0) {
    return;  // no reliable date source; just keep counting words for the session
  }
  if (dayNumber_ == 0) {
    dayNumber_ = today;
    persist(true);
    return;
  }
  if (today != dayNumber_) {
    rollover(today, dayNumber_);
  }
}

void ShuliPet::rollover(uint32_t today, uint32_t previous) {
  const uint32_t gap = (today > previous) ? (today - previous) : 1;
  if (goalMetToday_) {
    // The tracked day was fed; only the days strictly after it count as neglect.
    missedDays_ = static_cast<uint8_t>(std::min<uint32_t>(gap - 1, kMaxMissedDays));
  } else {
    missedDays_ =
        static_cast<uint8_t>(std::min<uint32_t>(static_cast<uint32_t>(missedDays_) + gap, kMaxMissedDays));
  }
  wordsToday_ = 0;
  goalMetToday_ = false;
  dayNumber_ = today;
  persist(true);
}

void ShuliPet::addWordsRead(uint32_t words) {
  if (words == 0) {
    return;
  }
  wordsToday_ += words;
  dirty_ = true;
  if (!goalMetToday_ && goalWords_ > 0 && wordsToday_ >= goalWords_) {
    goalMetToday_ = true;
    missedDays_ = 0;  // feeding her revives her mood immediately
    persist(true);
  }
}

void ShuliPet::flush() { persist(false); }

void ShuliPet::persist(bool force) {
  if (prefs_ == nullptr) {
    return;
  }
  if (!force && !dirty_) {
    return;
  }
  prefs_->putUInt(kPrefGoal, goalWords_);
  prefs_->putUInt(kPrefWords, wordsToday_);
  prefs_->putUInt(kPrefDay, dayNumber_);
  prefs_->putUChar(kPrefMissed, missedDays_);
  prefs_->putBool(kPrefMet, goalMetToday_);
  dirty_ = false;
}

ShuliPet::Mood ShuliPet::mood() const {
  if (goalMetToday_) {
    return Mood::Happy;
  }
  switch (missedDays_) {
    case 0:
      return Mood::Needy;
    case 1:
      return Mood::Grumpy;
    case 2:
      return Mood::Sad;
    case 3:
      return Mood::Sick;
    default:
      return Mood::Miserable;
  }
}

const char *ShuliPet::moodName() const {
  switch (mood()) {
    case Mood::Happy:
      return "Happy";
    case Mood::Needy:
      return "Needy";
    case Mood::Grumpy:
      return "Grumpy";
    case Mood::Sad:
      return "Sad";
    case Mood::Sick:
      return "Sick";
    case Mood::Miserable:
      return "Miserable";
  }
  return "";
}

const char *ShuliPet::statusLine() const {
  switch (mood()) {
    case Mood::Happy:
      return "Purrfect. You may pet me.";
    case Mood::Needy:
      return "Feed me words. Now. Please?";
    case Mood::Grumpy:
      return "You skipped a day. Rude.";
    case Mood::Sad:
      return "Two days... I'm wasting away.";
    case Mood::Sick:
      return "I feel awful. Read to me.";
    case Mood::Miserable:
      return "So sick... save me with a story.";
  }
  return "";
}

uint8_t ShuliPet::goalPercent() const {
  if (goalWords_ == 0) {
    return 100;
  }
  const uint32_t pct = (wordsToday_ * 100u) / goalWords_;
  return static_cast<uint8_t>(std::min<uint32_t>(pct, 100));
}

void ShuliPet::setGoalWords(uint32_t goal) {
  if (goal == 0) {
    goal = 2000;
  }
  goalWords_ = goal;
  if (!goalMetToday_ && wordsToday_ >= goalWords_) {
    goalMetToday_ = true;
    missedDays_ = 0;
  }
  persist(true);
}

void ShuliPet::cycleGoal() {
  const size_t count = sizeof(kGoalPresets) / sizeof(kGoalPresets[0]);
  size_t idx = 0;
  for (size_t i = 0; i < count; ++i) {
    if (goalWords_ == kGoalPresets[i]) {
      idx = i;
      break;
    }
  }
  setGoalWords(kGoalPresets[(idx + 1) % count]);
}

String ShuliPet::goalLabel() const { return String(goalWords_) + " words"; }
