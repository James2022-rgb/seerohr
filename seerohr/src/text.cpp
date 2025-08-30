// TU header --------------------------------------------
#include "text.h"

// c++ headers ------------------------------------------
#include <cstdint>

#include <array>
#include <unordered_map>

#if defined(_MSC_VER)
# pragma execution_character_set("utf-8")
#endif

constexpr uint32_t kLanguageCount = 3;

namespace {

#define MAKE_TEXT(id, de, en, jp) std::make_pair(TextId::id, std::array<const char*, kLanguageCount>{{ de, en, jp }})

std::unordered_map<TextId, std::array<const char*, kLanguageCount>> const kTextMap = {
  MAKE_TEXT(kCourse,            "Kurs",            "Course",             "針路"),
  MAKE_TEXT(kInput,             "Eingabe",         "Input",              "入力"),
  MAKE_TEXT(kOwnCourse,         "Eigenkurs",       "Own Course",         "自艦針路"),
  MAKE_TEXT(kTorpedoSpeed,      "Torpedogeschw.",  "Torpedo Speed",      "魚雷"),
  MAKE_TEXT(kTargetBearing,     "Schiffspeilung",  "Target Bearing",     "目標方位"),
  MAKE_TEXT(kTargetRange,       "Schußentfernung", "Target Range",       "目標距離"),
  MAKE_TEXT(kTargetSpeed,       "Gegnerfahrt",     "Target Speed",       "目標速度"),
  MAKE_TEXT(kAngleOnBow,        "Gegnerlage",      "Angle On Bow",       "Angle On Bow"),
  MAKE_TEXT(kOutput,            "Ausgabe",         "Output",             "出力"),
  MAKE_TEXT(kTargetCourse,      "Gegnerkurs",      "Target Course",      "目標針路"),
  MAKE_TEXT(kLeadAngle,         "Vorhaltwinkel",   "Lead Angle",         "リード角"),
  MAKE_TEXT(kTimeToImpact,      "Laufzeit",        "Time to Impact",     "航走時間"),
  MAKE_TEXT(kTorpedoGyroAngle,  "Schußwinkel",     "Torpedo Gyro Angle", "魚雷ジャイロ角"),
  MAKE_TEXT(kNoSolution,        "Keine Lösung",    "No solution",        "解なし"),
};

Language current_language = Language::kGerman;

} // namespace

Language GetCurrentLanguage() {
  return current_language;
}

void SetCurrentLanguage(Language lang) {
  current_language = lang;
}

const char* GetText(TextId id) {
  return GetTextInLang(id, current_language);
}

const char* GetTextInLang(TextId id, Language lang) {
  auto it = kTextMap.find(id);
  if (it == kTextMap.end()) {
    return "???";
  }
  uint32_t lang_index = static_cast<uint32_t>(lang);
  if (lang_index >= kLanguageCount) {
    lang_index = 1; // default to English
  }
  return it->second[lang_index];
}
