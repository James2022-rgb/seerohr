// TU header --------------------------------------------
#include "tdc.h"

// c++ headers ------------------------------------------
#include <cmath>

#include <numbers>

// external headers -------------------------------------
#include "imgui.h"

// http://www.tvre.org/en/torpedo-calculator-t-vh-re-s3
// http://www.tvre.org/en/gyro-angled-torpedoes
// https://www.reddit.com/r/uboatgame/comments/1f005cj/trigonometry_and_geometry_explanations_of_popular/
// https://www.reddit.com/r/uboatgame/comments/1hja5a7/the_ultimate_lookup_table_compendium/
// https://patents.google.com/patent/DE935417C/de


Angle Angle::FromDeg(float deg) {
  return Angle(deg * DEG2RAD);
}
Angle Angle::RightAngle() {
  return Angle(std::numbers::pi_v<float> *0.5f);
}
Angle Angle::Pi() {
  return Angle(std::numbers::pi_v<float>);
}

Angle Angle::WrapAround() const {
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  float r = std::fmod(rad_, kTwoPi);
  if (r < 0.0f) r += kTwoPi;
  return Angle(r);
}

float Angle::ToDeg() const {
  return rad_ * RAD2DEG;
}

float Angle::Sin() const { return std::sin(rad_); }
float Angle::Cos() const { return std::cos(rad_); }
float Angle::Tan() const { return std::tan(rad_); }

bool Angle::ImGuiSliderDeg(char const* label, float min_deg, float max_deg, char const* format) {
  float deg = this->ToDeg();
  if (ImGui::SliderFloat(label, &deg, min_deg, max_deg, format)) {
    rad_ = deg * DEG2RAD;
    return true;
  }
  return false;
}

namespace {

void DrawLineStippled(
  Vector2 startPos, Vector2 endPos, float thick, Color color
) {
  float const dx = endPos.x - startPos.x;
  float const dy = endPos.y - startPos.y;
  float const length = std::sqrt(dx * dx + dy * dy);
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

} // namespace

namespace {

Angle ComputeAbsoluteTargetBearing(
  Angle ownship_course,
  Angle relative_target_bearing
) {
  return ownship_course + relative_target_bearing;
}

raylib::Vector2 ComputeTargetPosition(
  raylib::Vector2 const& ownship_position,
  Angle ownship_course,
  Angle relative_target_bearing,
  float target_range_m
) {
  Angle const absolute_target_bearing = ComputeAbsoluteTargetBearing(
    ownship_course,
    relative_target_bearing
  );

  

  return { 
    ownship_position.x + target_range_m * +(-absolute_target_bearing + Angle::RightAngle()).Cos(),
    ownship_position.y + target_range_m * -(-absolute_target_bearing + Angle::RightAngle()).Sin()
  };
}

struct TorpedoTriangle final {
  float torpedo_speed_kn = 0.0f;
  Angle target_bearing = Angle::FromDeg(0.0f);
  float target_range_m = 0.0f;
  float target_speed_kn = 0.0f;

  Angle angle_on_bow = Angle::FromDeg(0.0f);

  std::optional<TorpedoTriangleSolution> Solve(Angle ownship_course) const {
    assert(this->torpedo_speed_kn > 0.0f);
    assert(-std::numbers::pi_v<float> <= this->angle_on_bow.AsRad() && this->angle_on_bow.AsRad() <= std::numbers::pi_v<float>);

    if (this->angle_on_bow.AsRad() == 0.0f) {
      return std::nullopt;
    }

    Angle const absolute_target_bearing = ComputeAbsoluteTargetBearing(
      ownship_course,
      this->target_bearing
    );

    Angle target_course = absolute_target_bearing + Angle::Pi() - this->angle_on_bow;
    target_course = target_course.WrapAround();

    Angle const lead_angle = Angle(std::asin(this->target_speed_kn / this->torpedo_speed_kn * this->angle_on_bow.Sin()));

    Angle const intercept_angle = Angle::Pi() - this->angle_on_bow - lead_angle;
    float const torpedo_run_distance_m = this->target_range_m / intercept_angle.Sin() * this->angle_on_bow.Sin();

    Angle const torpedo_course = absolute_target_bearing + lead_angle;

    Angle const torpedo_gyro_angle = torpedo_course - ownship_course;

    float const torpedo_speed_mps = this->torpedo_speed_kn * 1852.0f / 3600.0f;

    return TorpedoTriangleSolution {
      .target_course = target_course,
      .lead_angle = lead_angle,
      .torpedo_time_to_target_s = torpedo_run_distance_m / torpedo_speed_mps,
      .torpedo_gyro_angle = torpedo_gyro_angle
    };
  }
};

} // namespace

void Tdc::Update(Angle ownship_course) {
  TorpedoTriangle triangle {
    .torpedo_speed_kn = torpedo_speed_kn_,
    .target_bearing = target_bearing_,
    .target_range_m = target_range_m_,
    .target_speed_kn = target_speed_kn_,
    .angle_on_bow = angle_on_bow_
  };

  solution_ = triangle.Solve(ownship_course);
}

void Tdc::DrawVisualization(
  raylib::Camera2D const& camera,
  raylib::Vector2 const& ownship_position,
  Angle ownship_course
) const {
  Angle const absolute_target_bearing = ComputeAbsoluteTargetBearing(
    ownship_course,
    target_bearing_
  );

  raylib::Vector2 const target_position = ComputeTargetPosition(
    ownship_position,
    ownship_course,
    target_bearing_,
    target_range_m_
  );

  if (!solution_.has_value()) {
    // No solution, nothing to draw.
    return;
  }

  TorpedoTriangleSolution const& solution = solution_.value();

  BeginMode2D(camera);

  float const torpedo_run_distance_m = torpedo_speed_kn_ * 1852.0f / 3600.0f * solution.torpedo_time_to_target_s;

  raylib::Vector2 const impact_position = ownship_position + raylib::Vector2(
    torpedo_run_distance_m * (absolute_target_bearing + solution.lead_angle - Angle::RightAngle()).Cos(),
    torpedo_run_distance_m * (absolute_target_bearing + solution.lead_angle - Angle::RightAngle()).Sin()
  );

  // Draw a target ghost.
  {
    constexpr float kBeam = 17.3f;
    constexpr float kLength = 134.0f;

    rlPushMatrix();
      rlTranslatef(target_position.x, target_position.y, 0.0f);
      rlRotatef(solution.target_course.ToDeg(), 0.0f, 0.0f, 1.0f);
      rlTranslatef(-target_position.x, -target_position.y, 0.0f);
      DrawEllipseV(
        target_position,
        kBeam, kLength,
        RED
      );
    rlPopMatrix();

    rlPushMatrix();
      rlTranslatef(impact_position.x, impact_position.y, 0.0f);
      rlRotatef(solution.target_course.ToDeg(), 0.0f, 0.0f, 1.0f);
      rlTranslatef(-impact_position.x, -impact_position.y, 0.0f);
      DrawEllipseV(
        impact_position,
        kBeam, kLength,
        Color { 230, 41, 55, 64 }
      );
    rlPopMatrix();
  }

  // Draw a line from ownship to target.
  DrawLineEx(
    ownship_position,
    target_position,
    5.0f,
    DARKGRAY
  );

  // Draw angle on bow.
  DrawCircleSector(
    target_position,
    150.0f,
    solution.target_course.ToDeg() - 90.0f,
    solution.target_course.ToDeg() + angle_on_bow_.ToDeg() - 90.0f,
    32,
    Fade(BLUE, 0.25f)
  );

  // Draw projected ownship course line.
  DrawLineStippled(
    ownship_position,
    ownship_position + raylib::Vector2(
      10000.0f * (ownship_course - Angle::RightAngle()).Cos(),
      10000.0f * (ownship_course - Angle::RightAngle()).Sin()
    ),
    2.5f,
    DARKGRAY
  );

  // Draw torpedo gyro angle.
  {
    DrawCircleSector(
      ownship_position,
      200.0f,
      ownship_course.ToDeg() - 90.0f,
      ownship_course.ToDeg() + solution.torpedo_gyro_angle.ToDeg() - 90.0f,
      32,
      Fade(ORANGE, 0.25f)
    );
  }

  // Draw lead angle.
  DrawCircleSector(
    ownship_position,
    150.0f,
    absolute_target_bearing.ToDeg() - 90.0f,
    absolute_target_bearing.ToDeg() + solution.lead_angle.ToDeg() - 90.0f,
    32,
    Fade(BLUE, 0.25f)
  );

  // Draw torpedo course line to impact position.
  DrawLineEx(
    ownship_position,
    impact_position,
    5.0f,
    ORANGE
  );

  // Draw projected target course line to impact position.
  DrawLineStippled(
    target_position,
    impact_position,
    5.0f,
    GRAY
  );

  // Draw Draw projected target course line.
  DrawLineEx(
    target_position,
    target_position + raylib::Vector2(
      10000.0f * (solution.target_course - Angle::RightAngle()).Cos(),
      10000.0f * (solution.target_course - Angle::RightAngle()).Sin()
    ),
    2.5f,
    DARKGRAY
  );

  DrawCircleV(
    impact_position,
    10.0f,
    ORANGE
  );

  raylib::DrawTextEx(
    GetFontDefault(),
    TextFormat("Angle on Bow: %s%0.1f", angle_on_bow_.AsRad() > 0.0f ? "+" : "", angle_on_bow_.ToDeg()),
    target_position + raylib::Vector2(25.0f, 0.0f),
    50.0f,
    2.0f,
    DARKGRAY
  );
  raylib::DrawTextEx(
    GetFontDefault(),
    TextFormat(
      "Lead Angle: %0.1f\nTorpedo Gyro Angle: %s%0.1f",
      solution.lead_angle.ToDeg(),
      solution.torpedo_gyro_angle.AsRad() > 0.0f ? "+" : "", solution.torpedo_gyro_angle.ToDeg()
    ),
    ownship_position + raylib::Vector2(25.0f, 0.0f),
    50.0f,
    2.0f,
    DARKGRAY
  );

  EndMode2D();
}

void Tdc::DoPanelImGui(
  Angle ownship_course
) {
  ImGui::Begin("Torpedovorhaltrechner", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("Input:");
  ImGui::Text("Own Course: %.1f", ownship_course.ToDeg());
  ImGui::SliderFloat("Torpedo Speed (kn)", &torpedo_speed_kn_, 1.0f, 60.0f, "%.0f");
  target_bearing_.ImGuiSliderDeg("Target Bearing (deg)", -179.0f, 179.0f, "%.1f");
  ImGui::SliderFloat("Target Range (m)", &target_range_m_, 300.0f, 4000.0f, "%.0f");
  ImGui::SliderFloat("Target Speed (kn)", &target_speed_kn_, 0.0f, 40.0f, "%.0f");
  angle_on_bow_.ImGuiSliderDeg("Angle on Bow (deg)", -179.9f, 179.9f, "%.1f");

  ImGui::Separator();

  ImGui::Text("Output:");
  if (!solution_.has_value()) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No solution");
  }
  else {
    TorpedoTriangleSolution const& solution = solution_.value();

    ImGui::Text("Target Course: %.1f", solution.target_course.ToDeg());
    ImGui::Text("Lead Angle: %.1f", solution.lead_angle.ToDeg());
    ImGui::Text("Time to Impact: %.1f s", solution.torpedo_time_to_target_s);
    ImGui::Text("Torpedo Gyro Angle: %.1f", solution.torpedo_gyro_angle.ToDeg());
  }

  ImGui::End();
}
