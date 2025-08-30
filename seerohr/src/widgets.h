#pragma once

// external headers -------------------------------------
#include "imgui.h"

bool SliderFloatWithId(
  char const* str_id,
  float* v,
  float v_min,
  float v_max,
  char const* value_format,
  ImGuiSliderFlags flags,
  char const* label_format,
  ...
);

bool SliderFloatWithIdV(
  char const* str_id,
  float* v,
  float v_min,
  float v_max,
  char const* value_format,
  ImGuiSliderFlags flags,
  char const* label_format,
  va_list args
);
