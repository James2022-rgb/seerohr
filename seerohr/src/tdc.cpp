// TU header --------------------------------------------
#include "tdc.h"

// c++ headers ------------------------------------------
#include <cmath>

// external headers -------------------------------------
#include "imgui.h"

// http://www.tvre.org/en/torpedo-calculator-t-vh-re-s3
// http://www.tvre.org/en/gyro-angled-torpedoes
// https://www.reddit.com/r/uboatgame/comments/1f005cj/trigonometry_and_geometry_explanations_of_popular/
// https://www.reddit.com/r/uboatgame/comments/1hja5a7/the_ultimate_lookup_table_compendium/
// https://patents.google.com/patent/DE935417C/de

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

float ComputeAbsoluteTargetBearingRad(
  float ownship_course_rad,
  float relative_target_bearing_rad
) {
  return ownship_course_rad + relative_target_bearing_rad;
}

raylib::Vector2 ComputeTargetPosition(
  raylib::Vector2 const& ownship_position,
  float ownship_course_rad,
  float relative_target_bearing_rad,
  float target_range_m
) {
  float const absolute_target_bearing_rad = ComputeAbsoluteTargetBearingRad(
    ownship_course_rad,
    relative_target_bearing_rad
  );

  return {
    ownship_position.x + target_range_m * +std::cos(-absolute_target_bearing_rad + PI * 0.5f),
    ownship_position.y + target_range_m * -std::sin(-absolute_target_bearing_rad + PI * 0.5f)
  };
}

} // namespace

void Tdc::Update(float ownship_course_rad) {
  float const relative_target_bearing_rad = target_bearing_deg_ * DEG2RAD;
  float const absolute_target_bearing_rad = ComputeAbsoluteTargetBearingRad(ownship_course_rad, relative_target_bearing_rad);

  float const target_course_rad = absolute_target_bearing_rad + PI - angle_on_bow_deg_ * DEG2RAD;
  target_course_deg_ = target_course_rad * RAD2DEG;
  target_course_deg_ = std::fmod(target_course_deg_ + 360.0f, 360.0f);

  lead_angle_deg_ = std::asin(target_speed_kn_ / torpedo_speed_kn_ * std::sin(angle_on_bow_deg_ * DEG2RAD)) * RAD2DEG;

  float const intercept_angle_rad = PI - angle_on_bow_deg_ * DEG2RAD - lead_angle_deg_ * DEG2RAD;
  float const torpedo_run_distance_m = target_range_m_ / std::sin(intercept_angle_rad) * std::sin(angle_on_bow_deg_ * DEG2RAD);

  torpedo_time_to_target_s_ = torpedo_run_distance_m / (torpedo_speed_kn_ * 1852.0f / 3600.0f);

  float const torpedo_course_deg = absolute_target_bearing_rad * RAD2DEG + lead_angle_deg_;

  torpedo_gyro_angle_deg_ = (torpedo_course_deg - ownship_course_rad * RAD2DEG);
}

void Tdc::DrawVisualization(
  raylib::Camera2D const& camera,
  raylib::Vector2 const& ownship_position,
  float ownship_course_rad
) const {
  float const relative_target_bearing_rad = target_bearing_deg_ * DEG2RAD;
  float const absolute_target_bearing_rad = ComputeAbsoluteTargetBearingRad(ownship_course_rad, relative_target_bearing_rad);

  raylib::Vector2 const target_position = ComputeTargetPosition(
    ownship_position,
    ownship_course_rad,
    target_bearing_deg_ * DEG2RAD,
    target_range_m_
  );

  BeginMode2D(camera);

  float const torpedo_run_distance_m = torpedo_speed_kn_ * 1852.0f / 3600.0f * torpedo_time_to_target_s_;

  raylib::Vector2 const impact_position = ownship_position + raylib::Vector2(
    torpedo_run_distance_m * std::cos(absolute_target_bearing_rad + lead_angle_deg_ * DEG2RAD - PI * 0.5f),
    torpedo_run_distance_m * std::sin(absolute_target_bearing_rad + lead_angle_deg_ * DEG2RAD - PI * 0.5f)
  );

  // Draw a target ghost.
  {
    constexpr float kBeam = 17.3f;
    constexpr float kLength = 134.0f;

    rlPushMatrix();
      rlTranslatef(target_position.x, target_position.y, 0.0f);
      rlRotatef(target_course_deg_, 0.0f, 0.0f, 1.0f);
      rlTranslatef(-target_position.x, -target_position.y, 0.0f);
      DrawEllipseV(
        target_position,
        kBeam, kLength,
        RED
      );
    rlPopMatrix();

    rlPushMatrix();
      rlTranslatef(impact_position.x, impact_position.y, 0.0f);
      rlRotatef(target_course_deg_, 0.0f, 0.0f, 1.0f);
      rlTranslatef(-impact_position.x, -impact_position.y, 0.0f);
      DrawEllipseV(
        impact_position,
        kBeam, kLength,
        Color { 230, 41, 55, 64 }
      );
    rlPopMatrix();
  }

  // Draw a line from ownship to target.
  {
    DrawLineEx(
      ownship_position,
      target_position,
      5.0f,
      DARKGRAY
    );

#if 0
    float const r = target_range_m_ * 0.5f;

    raylib::DrawTextEx(
      GetFontDefault(),
      TextFormat("Range: %0.1f m", target_range_m_),
      ownship_position + (target_position - ownship_position).Normalize() * r + raylib::Vector2(25.0f, 0.0f),
      50.0f,
      2.0f,
      DARKGRAY
    );
#endif
  }

  // Draw angle on bow.
  DrawCircleSector(
    target_position,
    150.0f,
    target_course_deg_ - 90.0f,
    target_course_deg_ + angle_on_bow_deg_ - 90.0f,
    32,
    Fade(BLUE, 0.25f)
  );

  // Draw projected ownship course line.
  DrawLineStippled(
    ownship_position,
    ownship_position + raylib::Vector2(
      10000.0f * std::cos(ownship_course_rad - PI * 0.5f),
      10000.0f * std::sin(ownship_course_rad - PI * 0.5f)
    ),
    2.5f,
    DARKGRAY
  );

  // Draw torpedo gyro angle.
  {
    float const torpedo_gyro_angle_rad = torpedo_gyro_angle_deg_ * DEG2RAD;
    DrawCircleSector(
      ownship_position,
      200.0f,
      ownship_course_rad * RAD2DEG - 90.0f,
      ownship_course_rad* RAD2DEG + torpedo_gyro_angle_deg_ - 90.0f,
      32,
      Fade(ORANGE, 0.25f)
    );
  }

  // Draw lead angle.
  DrawCircleSector(
    ownship_position,
    150.0f,
    absolute_target_bearing_rad * RAD2DEG - 90.0f,
    absolute_target_bearing_rad * RAD2DEG + lead_angle_deg_ - 90.0f,
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
      10000.0f * std::cos((target_course_deg_ - 90.0f) * DEG2RAD),
      10000.0f * std::sin((target_course_deg_ - 90.0f) * DEG2RAD)
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
    TextFormat("Angle on Bow: %s%0.1f", angle_on_bow_deg_ > 0.0f ? "+" : "", angle_on_bow_deg_),
    target_position + raylib::Vector2(25.0f, 0.0f),
    50.0f,
    2.0f,
    DARKGRAY
  );
  raylib::DrawTextEx(
    GetFontDefault(),
    TextFormat(
      "Lead Angle: %0.1f\nTorpedo Gyro Angle: %s%0.1f",
      lead_angle_deg_,
      torpedo_gyro_angle_deg_ > 0.0f ? "+" : "", torpedo_gyro_angle_deg_
    ),
    ownship_position + raylib::Vector2(25.0f, 0.0f),
    50.0f,
    2.0f,
    DARKGRAY
  );

  EndMode2D();
}

void Tdc::DoPanelImGui(
  float ownship_course_deg
) {
  ImGui::Begin("Torpedovorhaltrechner", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("Input:");
  ImGui::Text("Own Course: %.1f", ownship_course_deg);
  ImGui::SliderFloat("Torpedo Speed (kn)", &torpedo_speed_kn_, 1.0f, 60.0f, "%.0f");
  ImGui::SliderFloat("Bearing (deg)", &target_bearing_deg_, -179.0f, 179.0f, "%.1f");
  ImGui::SliderFloat("Range (m)", &target_range_m_, 300.0f, 4000.0f, "%.0f");
  ImGui::SliderFloat("Speed (kn)", &target_speed_kn_, 0.0f, 40.0f, "%.0f");
  ImGui::SliderFloat("Angle on Bow (deg)", &angle_on_bow_deg_, -179.9f, 179.9f, "%.1f");

  ImGui::Separator();

  ImGui::Text("Output:");
  ImGui::Text("Target Course: %.1f", target_course_deg_);
  ImGui::Text("Lead Angle: %.1f", lead_angle_deg_);
  ImGui::Text("Time to Impact: %.1f s", torpedo_time_to_target_s_);
  ImGui::Text("Torpedo Gyro Angle: %.1f", torpedo_gyro_angle_deg_);

  ImGui::End();
}
