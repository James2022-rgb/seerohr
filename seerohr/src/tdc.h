#pragma once

// external headers -------------------------------------
#include "raylib-cpp.hpp"

class Tdc final {
public:
  void Update(float ownship_course_rad);

  void DrawVisualization(
    raylib::Camera2D const& camera,
    raylib::Vector2 const& ownship_position,
    float ownship_course_rad
  ) const;

  void DoPanelImGui(
    float ownship_course_deg
  );

private:
  // TDC inputs.
  float torpedo_speed_kn_ = 30.0f;
  float target_bearing_deg_ = -30.0f;
  float target_range_m_ = 900.0f;
  float target_speed_kn_ = 10.0f;
  float angle_on_bow_deg_ = 90;

  // TDC outputs.
  float target_course_deg_ = 0.0f;
  float lead_angle_deg_ = 0.0f;
  float torpedo_time_to_target_s_ = 0.0f;
  float torpedo_gyro_angle_deg_ = 0.0f;
};
