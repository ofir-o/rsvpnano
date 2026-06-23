#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>

// Shuli — a fluffy orange-and-white, posh-looking but sweet-and-super-needy cat that you "feed" by
// reading. Meet the daily word goal and she is happy and clingy; skip a day and she gets angry, and
// the longer you neglect her the sicker and more miserable she becomes. Reading again revives her
// (gentle model — she never permanently dies).
class ShuliPet {
 public:
  enum class Mood : uint8_t {
    Happy = 0,   // goal met today: purring, sweet, adoring
    Needy,       // a new day, not fed yet, but not neglected: begging for attention
    Grumpy,      // missed 1 day: offended / angry
    Sad,         // missed 2 days
    Sick,        // missed 3 days
    Miserable,   // missed 4+ days: very sick and pitiful
  };

  // Loads persisted state from NVS and reconciles it with today's date.
  void begin(Preferences *prefs);
  // Call periodically; handles day rollover (counts missed days, resets the daily counter).
  void update();
  // Feed Shuli: call as words are read. Meeting the daily goal makes her happy and clears neglect.
  void addWordsRead(uint32_t words);
  // Persist current state (call sparingly, e.g. on pause / leaving the screen).
  void flush();

  Mood mood() const;
  const char *moodName() const;
  const char *statusLine() const;  // a short needy/posh quip for the current mood
  uint32_t wordsToday() const { return wordsToday_; }
  uint32_t goalWords() const { return goalWords_; }
  uint8_t goalPercent() const;
  bool goalMetToday() const { return goalMetToday_; }
  uint8_t missedDays() const { return missedDays_; }

  void setGoalWords(uint32_t goal);
  void cycleGoal();                // step through a few preset daily goals (Settings)
  String goalLabel() const;        // e.g. "2000 words"

 private:
  static uint32_t civilDays(uint16_t y, uint8_t m, uint8_t d);  // days since 1970-01-01
  uint32_t todayDayNumber(bool &haveClock) const;              // 0 when no RTC
  void rollover(uint32_t today, uint32_t previous);
  void persist(bool force);

  Preferences *prefs_ = nullptr;
  uint32_t goalWords_ = 2000;
  uint32_t wordsToday_ = 0;
  uint32_t dayNumber_ = 0;   // the civil-day the daily counter applies to (0 = unknown)
  uint8_t missedDays_ = 0;
  bool goalMetToday_ = false;
  bool dirty_ = false;
  uint32_t lastPersistMs_ = 0;
};
