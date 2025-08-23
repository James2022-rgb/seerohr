#pragma once

#if CONFIG_USE_RAYGUI

// c++ headers ------------------------------------------
#include <cstdint>

#include <functional>

// external headers -------------------------------------
#include "raylib.h"

using SrPanelContentContext = std::function<Rectangle(Rectangle const& bounds)>;
using SrPanelContent = std::function<void(SrPanelContentContext const& ctx)>;

bool SrGuiPanel(
  Rectangle const& bounds,
  char const* label,
  SrPanelContent const& content
);

bool SrGuiValueBoxI32(
  Rectangle const& bounds,
  char const* label,
  int32_t* value,
  int32_t min_value,
  int32_t max_value
);

bool SrGuiValueBoxU32(
  Rectangle const& bounds,
  char const* label,
  uint32_t* value,
  uint32_t min_value,
  uint32_t max_value
);

bool SrGuiSliderF32(
  Rectangle const& bounds,
  char const* label,
  char const* value_text,
  float* value,
  float min_value,
  float max_value
);

bool SrGuiSliderI32(
  Rectangle const& bounds,
  char const* label,
  char const* value_text,
  int32_t* value,
  int32_t min_value,
  int32_t max_value
);

#endif
