// TU header --------------------------------------------
#include "widgets.h"

// c++ headers ------------------------------------------
#include <cstdarg>

#include <array>
#include <string>

bool SliderFloatWithId(
  char const* str_id,
  float* v,
  float v_min,
  float v_max,
  char const* value_format,
  ImGuiSliderFlags flags,
  char const* label_format,
  ...
) {
  va_list args;
  va_start(args, label_format);
  bool changed = SliderFloatWithIdV(str_id, v, v_min, v_max, value_format, flags, label_format, args);
  va_end(args);
  return changed;
}

bool SliderFloatWithIdV(
  char const* str_id,
  float* v,
  float v_min,
  float v_max,
  char const* value_format,
  ImGuiSliderFlags flags,
  char const* label_format,
  va_list label_args
) {
  std::array<char, 256> buffer { 0 };
  vsnprintf(buffer.data(), buffer.size(), label_format, label_args);
  std::string label = std::string(buffer.data()) + "###" + std::string(str_id);
  return ImGui::SliderFloat(label.c_str(), v, v_min, v_max, value_format, flags);
}
