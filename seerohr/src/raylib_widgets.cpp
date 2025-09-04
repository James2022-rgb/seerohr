// TU header --------------------------------------------
#include "raylib_widgets.h"

// c++ headers ------------------------------------------
#include <cmath>

#include <algorithm>

void DrawLineStippled(
  Vector2 startPos, Vector2 endPos, float thick, Color color
) {
  float const dx = endPos.x - startPos.x;
  float const dy = endPos.y - startPos.y;
  float const length = std::min(std::sqrt(dx * dx + dy * dy), 10000.0f);
  if (length < 1.0f) return;
  float const step = 4.0f; // Length of each dash and gap.
  int const numSteps = static_cast<int>(length / step);
  for (int i = 0; i < numSteps; ++i) {
    float const t1 = static_cast<float>(i) / numSteps;
    float const t2 = static_cast<float>(i + 1) / numSteps;
    Vector2 p1 = {
      startPos.x + t1 * dx,
      startPos.y + t1 * dy
    };
    Vector2 p2 = {
      startPos.x + t2 * dx,
      startPos.y + t2 * dy
    };
    if (i % 2 == 0) {
      DrawLineEx(p1, p2, thick, color);
    }
  }
}
