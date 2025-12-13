// TU header --------------------------------------------
#include "angle.h"

// c++ headers ------------------------------------------
#include <cmath>
#include <cstdarg>

#include <numbers>

// project headers --------------------------------------
#include "raylib_widgets.h"
#include "widgets.h"

Angle Angle::FromDeg(float deg) {
  return Angle(deg * DEG2RAD);
}
Angle Angle::RightAngle() {
  return Angle(std::numbers::pi_v<float> *0.5f);
}
Angle Angle::Pi() {
  return Angle(std::numbers::pi_v<float>);
}

Angle Angle::Abs() const {
  return Angle(std::abs(rad_));
}
float Angle::Sign() const {
  return (rad_ > 0.0f) ? 1.0f : ((rad_ < 0.0f) ? -1.0f : 0.0f);
}

Angle Angle::WrapAround() const {
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  float r = std::fmod(rad_, kTwoPi);
  if (r < 0.0f) r += kTwoPi;
  return Angle(r);
}

float Angle::ToDeg() const {
  return rad_ * RAD2DEG;
}

float Angle::Sin() const { return std::sin(rad_); }
float Angle::Cos() const { return std::cos(rad_); }
float Angle::Tan() const { return std::tan(rad_); }

bool Angle::ImGuiSliderDeg(char const* label, float min_deg, float max_deg, char const* format) {
  float deg = this->ToDeg();
  if (ImGui::SliderFloat(label, &deg, min_deg, max_deg, format)) {
    rad_ = deg * DEG2RAD;
    return true;
  }
  return false;
}

bool Angle::ImGuiSliderDegWithId(char const* str_id, float min_deg, float max_deg, char const* value_format, char const* label_format, ...) {
  va_list args;
  va_start(args, label_format);

  float deg = this->ToDeg();
  bool changed = SliderFloatWithIdV(str_id, &deg, min_deg, max_deg, value_format, ImGuiSliderFlags_None, label_format, args);
  if (changed) {
    rad_ = deg * DEG2RAD;
  }
  va_end(args);
  return changed;
}
