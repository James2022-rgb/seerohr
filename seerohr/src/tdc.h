#pragma once

// c++ headers ------------------------------------------
#include <optional>

// external headers -------------------------------------
#include "raylib-cpp.hpp"

// project headers --------------------------------------
#include "angle.h"

struct TorpedoSpec final {
  /// Distance in meters from the aiming device to the torpedo tube.
  float distance_to_tube = 27.0f;
  /// Initial straight run in meters; distance the torpedo runs straight ahead before starting to turn.
  float reach = 9.5f;
  /// Turn radius of the torpedo in meters.
  float turn_radius = 95.0f;
  /// Speed of the torpedo in knots.
  float speed_kn = 30.0f;

  /// Compute the offset to the equivalent point of fire, or ideeller Torpedoeintrittsort as it is called in German.
  ///
  /// * `delta`: Parallax correction angle, or Winkelparallaxverbesserung.
  /// * `rho`: Final torpedo gyro angle, or Schusswinkel.
  raylib::Vector2 ComputeEquivalentPointOfFireOffset(float delta, float rho) const;
};

struct TorpedoTriangleIntermediate final {
  Angle ownship_course = Angle(0.0f);
  Angle absolute_target_bearing = Angle(0.0f);
  Angle target_course = Angle(0.0f);
};

struct TorpedoTriangleSolution final {
  Angle target_course = Angle::FromDeg(0.0f);
  Angle lead_angle = Angle::FromDeg(0.0f);
  Angle intercept_angle = Angle::FromDeg(0.0f);
  float torpedo_time_to_target_s = 0.0f;
  Angle pseudo_torpedo_gyro_angle = Angle::FromDeg(0.0f); // Signed: Positive is starboard, negative is port.
  raylib::Vector2 impact_position = { 0.0f, 0.0f };
};

struct EquivalentPointOfFireSolution final {
  float delta = 0.0f; // Parallax correction angle, or Winkelparallaxverbesserung.
  float rho = 0.0f;   // Final torpedo gyro angle, or Schusswinkel.
};

class Tdc final {
public:
  void Update(
    Angle ownship_course,
    raylib::Vector2 const& aiming_device_position
  );

  void DrawVisualization(
    raylib::Camera2D const& camera,
    raylib::Vector2 const& ownship_position,
    raylib::Vector2 const& aiming_device_position,
    Angle ownship_course,
    float target_beam,
    float target_length
  ) const;

  void DoPanelImGui(
    Angle ownship_course
  );

private:
  //
  // TDC inputs.
  //

  TorpedoSpec torpedo_spec_;
  
  Angle target_bearing_ = Angle::FromDeg(-80.0f);
  float target_range_m_ = 900.0f;
  float target_speed_kn_ = 20.0f;
  Angle angle_on_bow_ = Angle::FromDeg(70.0f);

  // TDC outputs.
  TorpedoTriangleIntermediate interm_;
  std::optional<TorpedoTriangleSolution> solution_;
  std::optional<EquivalentPointOfFireSolution> epf_solution_;
};
