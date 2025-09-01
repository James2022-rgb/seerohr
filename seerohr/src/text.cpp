// TU header --------------------------------------------
#include "text.h"

// c++ headers ------------------------------------------
#include <cstdint>

#include <array>
#include <unordered_map>

// platform detection -----------------------------------
#include "mbase/platform.h"

// conditional c++ headers ------------------------------
#if MBASE_PLATFORM_LINUX
# include <cstdlib>
#endif

#if MBASE_PLATFORM_LINUX || MBASE_PLATFORM_WEB
# include <cstring>
#endif

// conditional platform headers -------------------------
#if MBASE_PLATFORM_WINDOWS
# include <Windows.h>
#elif MBASE_PLATFORM_WEB
# include <emscripten/emscripten.h>
#endif

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
  MAKE_TEXT(kTorpedoSpeed,      "Torpedogeschw.",  "Torpedo Speed",      "魚雷速度"),
  MAKE_TEXT(kTargetBearing,     "Schiffspeilung",  "Target Bearing",     "目標方位"),
  MAKE_TEXT(kTargetRange,       "Schußentfernung", "Target Range",       "目標距離"),
  MAKE_TEXT(kTargetSpeed,       "Gegnerfahrt",     "Target Speed",       "目標速度"),
  MAKE_TEXT(kAngleOnBow,        "Gegnerlage",      "Angle On Bow",       "Angle On Bow"),
  MAKE_TEXT(kOutput,            "Ausgabe",         "Output",             "出力"),
  MAKE_TEXT(kTargetCourse,      "Gegnerkurs",      "Target Course",      "目標針路"),
  MAKE_TEXT(kLeadAngle,         "Vorhaltwinkel",   "Lead Angle",         "リード角"),
  MAKE_TEXT(kTimeToImpact,      "Laufzeit",        "Time to Impact",     "魚雷航走時間"),
  MAKE_TEXT(kTorpedoGyroAngle,  "Schußwinkel",     "Torpedo Gyro Angle", "魚雷ジャイロ角"),
  MAKE_TEXT(kNoSolution,        "Keine Lösung",    "No solution",        "解なし"),
};

Language current_language = Language::kGerman;

} // namespace

Language GetSystemLanguageOrEnglish() {
  Language language = Language::kEnglish;

#if MBASE_PLATFORM_WINDOWS
  {
    // On Windows, use the system locale.
    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH] = { 0 };
    if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0) {
      if (wcsncmp(locale_name, L"de", 2) == 0) {
        language = Language::kGerman;
      }
      else if (wcsncmp(locale_name, L"ja", 2) == 0) {
        language = Language::kJapanese;
      }
      else {
        language = Language::kEnglish;
      }
    }
  }
#elif MBASE_PLATFORM_LINUX || MBASE_PLATFORM_WEB
  {
# if MBASE_PLATFORM_LINUX
    char* locale = std::getenv("LANGUAGE");
# elif MBASE_PLATFORM_WEB
    char* locale = emscripten_run_script_string("window.navigator.language");
# endif
    if (locale != nullptr) {
      if (strncmp(locale, "de", 2) == 0) {
        language = Language::kGerman;
      }
      else if (strncmp(locale, "ja", 2) == 0) {
        language = Language::kJapanese;
      }
      else {
        language = Language::kEnglish;
      }
    }
  }
#endif

  return language;
}

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
