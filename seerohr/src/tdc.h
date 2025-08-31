#pragma once

// c++ headers ------------------------------------------
#include <optional>

// external headers -------------------------------------
#include "raylib-cpp.hpp"

// project headers --------------------------------------

class Angle final {
public:
  static Angle FromDeg(float deg);
  static Angle RightAngle();
  static Angle Pi();

  constexpr Angle() = default;
  explicit Angle(float rad) : rad_(rad) {}
  ~Angle() = default;

  Angle(Angle const&) = default;
  Angle& operator=(Angle const&) = default;
  Angle(Angle&&) = default;
  Angle& operator=(Angle&&) = default;

  float AsRad() const { return rad_; }
  Angle Abs() const;
  float Sign() const;

  Angle WrapAround() const;

  float ToDeg() const;
  float Sin() const;
  float Cos() const;
  float Tan() const;

  bool ImGuiSliderDeg(char const* label, float min_deg, float max_deg, char const* format = "%.3f");
  bool ImGuiSliderDegWithId(char const* str_id, float min_deg, float max_deg, char const* value_format, char const* label_format, ...);

  Angle operator+(Angle const& other) const { return Angle(rad_ + other.rad_); }
  Angle operator-(Angle const& other) const { return Angle(rad_ - other.rad_); }
  Angle& operator+=(Angle const& other) { rad_ += other.rad_; return *this; }
  Angle& operator-=(Angle const& other) { rad_ -= other.rad_; return *this; }
  Angle operator-() const { return Angle(-rad_); }
  Angle operator*(float scalar) const { return Angle(rad_ * scalar); }
  Angle operator/(float scalar) const { return Angle(rad_ / scalar); }
  Angle& operator*=(float scalar) { rad_ *= scalar; return *this; }
  Angle& operator/=(float scalar) { rad_ /= scalar; return *this; }
  bool operator==(Angle const& other) const { return rad_ == other.rad_; }
  bool operator!=(Angle const& other) const { return rad_ != other.rad_; }
  bool operator<(Angle const& other) const { return rad_ < other.rad_; }
  bool operator<=(Angle const& other) const { return rad_ <= other.rad_; }
  bool operator>(Angle const& other) const { return rad_ > other.rad_; }
  bool operator>=(Angle const& other) const { return rad_ >= other.rad_; }

  friend Angle operator*(float scalar, Angle const& angle) { return Angle(angle.rad_ * scalar); }

private:
  float rad_ = 0.0f;
};

struct TorpedoTriangleIntermediate final {
  Angle ownship_course = Angle(0.0f);
  Angle absolute_target_bearing = Angle(0.0f);
  Angle target_course = Angle(0.0f);
};

struct TorpedoTriangleSolution final {
  Angle target_course = Angle::FromDeg(0.0f);
  Angle lead_angle = Angle::FromDeg(0.0f);
  float torpedo_time_to_target_s = 0.0f;
  Angle torpedo_gyro_angle = Angle::FromDeg(0.0f); // Signed: Positive is starboard, negative is port.
  raylib::Vector2 impact_position = { 0.0f, 0.0f };
};

class Tdc final {
public:
  void Update(
    Angle ownship_course,
    raylib::Vector2 const& ownship_position
  );

  void DrawVisualization(
    raylib::Camera2D const& camera,
    raylib::Vector2 const& ownship_position,
    Angle ownship_course
  ) const;

  void DoPanelImGui(
    Angle ownship_course
  );

private:
  // TDC inputs.
  float torpedo_speed_kn_ = 30.0f;
  Angle target_bearing_ = Angle::FromDeg(-30.0f);
  float target_range_m_ = 900.0f;
  float target_speed_kn_ = 10.0f;
  Angle angle_on_bow_ = Angle::FromDeg(90.0f);

  // TDC outputs.
  TorpedoTriangleIntermediate interm_;
  std::optional<TorpedoTriangleSolution> solution_;
};
