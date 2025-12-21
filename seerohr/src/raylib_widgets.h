#pragma once

// external headers -------------------------------------
#include "raylib.h"
#include "raylib-cpp.hpp"

// project headers --------------------------------------
#include "angle.h"

void DrawLineStippled(
  Vector2 startPos, Vector2 endPos, float thick, Color color
);

void DrawShipSilhouette(
  raylib::Vector2 const& position,
  float length,
  float beam,
  Angle course,
  Color hull_color
);
