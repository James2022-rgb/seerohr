#pragma once

enum class Language {
  kGerman,
  kEnglish,
  kJapanese,
};

enum class TextId {
  kCourse,
  kInput,
  kOwnCourse,
  kTorpedoSpeed,
  kTargetBearing,
  kTargetRange,
  kTargetSpeed,
  kAngleOnBow,
  kTorpedoSpeedAndTargetSpeed,
  kOutput,
  kTargetCourse,
  kLeadAngle,
  kTorpedoRunDistance,
  kTimeToImpact,
  kPseudoTorpedoGyroAngle,
  kNoSolution,
};

Language GetSystemLanguageOrEnglish();

Language GetCurrentLanguage();
void SetCurrentLanguage(Language lang);

const char* GetText(TextId id);

const char* GetTextInLang(TextId id, Language lang);
