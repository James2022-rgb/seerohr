#pragma once

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
