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

/// Draw a simplified warship/cargo ship silhouette (top-down view)
void DrawShipSilhouette(
  raylib::Vector2 const& position,
  float length,
  float beam,
  Angle course,
  Color hull_color
) {
  float const half_length = length * 0.5f;
  float const half_beam = beam * 0.5f;

  // Direction vectors
  float const cos_c = (course - Angle::RightAngle()).Cos();
  float const sin_c = (course - Angle::RightAngle()).Sin();

  raylib::Vector2 const fwd{ cos_c, sin_c };
  raylib::Vector2 const right{ -sin_c, cos_c };

  // Hull points - pointed bow, squared-off stern (typical cargo/warship)
  raylib::Vector2 const bow = position + fwd * half_length;
  
  // Bow taper points
  raylib::Vector2 const bow_left = position + fwd * half_length * 0.7f + right * half_beam * 0.5f;
  raylib::Vector2 const bow_right = position + fwd * half_length * 0.7f - right * half_beam * 0.5f;
  
  // Mid-hull (widest point)
  raylib::Vector2 const mid_left = position + right * half_beam + fwd * half_length * 0.2f;
  raylib::Vector2 const mid_right = position - right * half_beam + fwd * half_length * 0.2f;
  
  // Stern quarter
  raylib::Vector2 const stern_quarter_left = position + right * half_beam * 0.9f - fwd * half_length * 0.5f;
  raylib::Vector2 const stern_quarter_right = position - right * half_beam * 0.9f - fwd * half_length * 0.5f;
  
  // Stern (squared off)
  raylib::Vector2 const stern_left = position + right * half_beam * 0.7f - fwd * half_length * 0.9f;
  raylib::Vector2 const stern_right = position - right * half_beam * 0.7f - fwd * half_length * 0.9f;

  // Draw hull triangles (counter-clockwise winding)
  // Bow section
  DrawTriangle(bow, bow_right, bow_left, hull_color);
  
  // Forward hull
  DrawTriangle(bow_left, bow_right, mid_right, hull_color);
  DrawTriangle(bow_left, mid_right, mid_left, hull_color);
  
  // Mid hull
  DrawTriangle(mid_left, mid_right, stern_quarter_right, hull_color);
  DrawTriangle(mid_left, stern_quarter_right, stern_quarter_left, hull_color);
  
  // Stern section
  DrawTriangle(stern_quarter_left, stern_quarter_right, stern_right, hull_color);
  DrawTriangle(stern_quarter_left, stern_right, stern_left, hull_color);

  // Superstructure (bridge) - darker color
  Color super_color = hull_color;
  super_color.r = static_cast<unsigned char>(super_color.r * 0.65f);
  super_color.g = static_cast<unsigned char>(super_color.g * 0.65f);
  super_color.b = static_cast<unsigned char>(super_color.b * 0.65f);

  float const super_length = length * 0.18f;
  float const super_width = beam * 0.5f;
  raylib::Vector2 const super_center = position - fwd * half_length * 0.25f;

  raylib::Vector2 const s1 = super_center + fwd * super_length * 0.5f + right * super_width * 0.5f;
  raylib::Vector2 const s2 = super_center + fwd * super_length * 0.5f - right * super_width * 0.5f;
  raylib::Vector2 const s3 = super_center - fwd * super_length * 0.5f - right * super_width * 0.5f;
  raylib::Vector2 const s4 = super_center - fwd * super_length * 0.5f + right * super_width * 0.5f;

  DrawTriangle(s1, s3, s4, super_color);
  DrawTriangle(s1, s2, s3, super_color);

  // Forward gun turret / cargo hold marker
  Color deck_color = hull_color;
  deck_color.r = static_cast<unsigned char>(deck_color.r * 0.75f);
  deck_color.g = static_cast<unsigned char>(deck_color.g * 0.75f);
  deck_color.b = static_cast<unsigned char>(deck_color.b * 0.75f);

  float const turret_radius = beam * 0.2f;
  raylib::Vector2 const turret_pos = position + fwd * half_length * 0.35f;
  DrawCircleV(turret_pos, turret_radius, deck_color);
}
