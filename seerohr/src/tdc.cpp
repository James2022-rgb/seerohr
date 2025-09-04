// TU header --------------------------------------------
#include "tdc.h"

// c++ headers ------------------------------------------
#include <cmath>

#include <numbers>

// external headers -------------------------------------
#include "imgui.h"

// project headers --------------------------------------
#include "text.h"
#include "raylib_widgets.h"
#include "widgets.h"

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

Angle Angle::Abs() const {
  return Angle(std::abs(rad_));
}
float Angle::Sign() const {
  return (rad_ > 0.0f) ? 1.0f : ((rad_ < 0.0f) ? -1.0f : 0.0f);
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

bool Angle::ImGuiSliderDegWithId(char const* str_id, float min_deg, float max_deg, char const* value_format, char const* label_format, ...) {
  va_list args;
  va_start(args, label_format);

  float deg = this->ToDeg();
  bool changed = SliderFloatWithIdV(str_id, &deg, min_deg, max_deg, value_format, ImGuiSliderFlags_None, label_format, args);
  if (changed) {
    rad_ = deg * DEG2RAD;
  }
  va_end(args);
  return changed;
}

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

  Angle angle_on_bow = Angle::FromDeg(0.0f); // Signed: Positive is starboard, negative is port.

  TorpedoTriangleIntermediate PrepareSolve(
    Angle ownship_course
  ) const {
    Angle const absolute_target_bearing = ComputeAbsoluteTargetBearing(
      ownship_course,
      this->target_bearing
    );

    Angle target_course = absolute_target_bearing + Angle::Pi() - this->angle_on_bow;
    target_course = target_course.WrapAround();

    return TorpedoTriangleIntermediate {
      .ownship_course = ownship_course,
      .absolute_target_bearing = absolute_target_bearing,
      .target_course = target_course,
    };
  }

  std::optional<TorpedoTriangleSolution> Solve(
    TorpedoTriangleIntermediate const& interm,
    raylib::Vector2 const& ownship_position
  ) const {
    assert(this->torpedo_speed_kn > 0.0f);

    if (this->angle_on_bow.AsRad() == 0.0f || std::abs(this->angle_on_bow.AsRad()) == std::numbers::pi_v<float>) {
      // No solution using torpedo triangle; target course line is identical to ownship line.

      float target_speed_seen_from_torpedo_kn = ((this->angle_on_bow.AsRad() == 0.0f) ? -this->target_speed_kn : this->target_speed_kn) - this->torpedo_speed_kn;

      if (target_speed_seen_from_torpedo_kn >= 0.0f){
        // No solution; target is too fast for torpedo to ever catch up.
        return std::nullopt;
      }

      // Closing: Positive, Moving away: Negative
      float signed_target_speed_kn = (this->angle_on_bow.AsRad() == 0.0f) ? this->target_speed_kn : -this->target_speed_kn;

      float torpedo_time_to_target_s = this->target_range_m / ((this->torpedo_speed_kn + signed_target_speed_kn) * 1852.0f / 3600.0f);
      float torpedo_run_distance_m = (this->torpedo_speed_kn * 1852.0f / 3600.0f) * torpedo_time_to_target_s;

      raylib::Vector2 const impact_position = ownship_position + raylib::Vector2 (
        torpedo_run_distance_m * (interm.absolute_target_bearing - Angle::RightAngle()).Cos(),
        torpedo_run_distance_m * (interm.absolute_target_bearing - Angle::RightAngle()).Sin()
      );

      return TorpedoTriangleSolution {
        .target_course = interm.target_course,
        .lead_angle = Angle(0.0f),
        .torpedo_time_to_target_s = torpedo_time_to_target_s,
        .torpedo_gyro_angle = this->target_bearing,
        .impact_position = impact_position,
      };
    }

    float const sin_lead_angle = this->target_speed_kn / this->torpedo_speed_kn * this->angle_on_bow.Abs().Sin();
    if (1.0f < sin_lead_angle) {
      // No solution; target is too fast leaving no valid lead angle for given torpedo speed and target course.
      // NOTE: sin_lead_angle == 0.0f is only possible when `this->target_speed_kn` or `this->angle_on_bow` is zero.
      return std::nullopt;
    }

    assert(0.0f <= sin_lead_angle && sin_lead_angle <= 1.0f);

    Angle const lead_angle = Angle(std::asin(sin_lead_angle));
    Angle const intercept_angle = Angle::Pi() - this->angle_on_bow.Abs() - lead_angle;
    if (intercept_angle.AsRad() <= 0.0f) {
      // No solution; target is too fast leaving no valid lead angle for given torpedo speed and target course.
      return std::nullopt;
    }

    float const torpedo_run_distance_m = this->target_range_m / intercept_angle.Sin() * this->angle_on_bow.Abs().Sin();

    float torpedo_time_to_target_s = 0.0f;
    {
      float torpedo_speed_mps = this->torpedo_speed_kn * 1852.0f / 3600.0f;

      torpedo_time_to_target_s = torpedo_run_distance_m / torpedo_speed_mps;
    }

    Angle const torpedo_course = interm.absolute_target_bearing + (this->angle_on_bow.Sign() * lead_angle);
    Angle const torpedo_gyro_angle = torpedo_course - interm.ownship_course;

    // `- Angle::RightAngle()` corrects for coordinate space difference.
    raylib::Vector2 const impact_position = ownship_position + raylib::Vector2 (
      torpedo_run_distance_m * (interm.absolute_target_bearing + (this->angle_on_bow.Sign() * lead_angle) - Angle::RightAngle()).Cos(),
      torpedo_run_distance_m * (interm.absolute_target_bearing + (this->angle_on_bow.Sign() * lead_angle) - Angle::RightAngle()).Sin()
    );

    return TorpedoTriangleSolution {
      .target_course = interm.target_course,
      .lead_angle = lead_angle,
      .torpedo_time_to_target_s = torpedo_time_to_target_s,
      .torpedo_gyro_angle = torpedo_gyro_angle,
      .impact_position = impact_position,
    };
  }
};

} // namespace

void Tdc::Update(
  Angle ownship_course,
  raylib::Vector2 const& ownship_position
) {
  TorpedoTriangle triangle {
    .torpedo_speed_kn = torpedo_speed_kn_,
    .target_bearing = target_bearing_,
    .target_range_m = target_range_m_,
    .target_speed_kn = target_speed_kn_,
    .angle_on_bow = angle_on_bow_
  };

  interm_ = triangle.PrepareSolve(ownship_course);

  solution_ = triangle.Solve(interm_, ownship_position);
}

void Tdc::DrawVisualization(
  raylib::Camera2D const& camera,
  raylib::Vector2 const& ownship_position,
  Angle ownship_course
) const {
  raylib::Vector2 const target_position = ComputeTargetPosition(
    ownship_position,
    ownship_course,
    target_bearing_,
    target_range_m_
  );

  if (!solution_.has_value()) {
    // No solution, nothing to draw.
    // return;
  }

  BeginMode2D(camera);

  // Draw a target ghost.
  {
    constexpr float kTargetBeam = 17.3f;
    constexpr float kTargetLength = 134.0f;

    rlPushMatrix();
      rlTranslatef(target_position.x, target_position.y, 0.0f);
      rlRotatef(interm_.target_course.ToDeg(), 0.0f, 0.0f, 1.0f);
      rlTranslatef(-target_position.x, -target_position.y, 0.0f);
      DrawEllipseV(
        target_position,
        kTargetBeam, kTargetLength,
        RED
      );
    rlPopMatrix();

    if (solution_.has_value()) {
      rlPushMatrix();
        rlTranslatef(solution_->impact_position.x, solution_->impact_position.y, 0.0f);
        rlRotatef(interm_.target_course.ToDeg(), 0.0f, 0.0f, 1.0f);
        rlTranslatef(-solution_->impact_position.x, -solution_->impact_position.y, 0.0f);
        DrawEllipseV(
          solution_->impact_position,
          kTargetBeam, kTargetLength,
          Color { 230, 41, 55, 64 }
        );
      rlPopMatrix();
    }
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
    interm_.target_course.ToDeg() - 90.0f,
    interm_.target_course.ToDeg() + angle_on_bow_.ToDeg() - 90.0f,
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

  if (solution_.has_value()) {
    // Draw torpedo gyro angle.
    {
      DrawCircleSector(
        ownship_position,
        200.0f,
        ownship_course.ToDeg() - 90.0f,
        ownship_course.ToDeg() + solution_->torpedo_gyro_angle.ToDeg() - 90.0f,
        32,
        Fade(ORANGE, 0.25f)
      );
    }

    // Draw lead angle.
    DrawCircleSector(
      ownship_position,
      150.0f,
      interm_.absolute_target_bearing.ToDeg() - 90.0f,
      interm_.absolute_target_bearing.ToDeg() + (angle_on_bow_.Sign() * solution_->lead_angle.ToDeg()) - 90.0f,
      32,
      Fade(BLUE, 0.25f)
    );

    // Draw torpedo course line to impact position.
    DrawLineEx(
      ownship_position,
      solution_->impact_position,
      5.0f,
      ORANGE
    );

    // Draw projected target course line to impact position.
    DrawLineStippled(
      target_position,
      solution_->impact_position,
      5.0f,
      GRAY
    );
  }

  // Draw Draw projected target course line.
  DrawLineEx(
    target_position,
    target_position + raylib::Vector2(
      10000.0f * (interm_.target_course - Angle::RightAngle()).Cos(),
      10000.0f * (interm_.target_course - Angle::RightAngle()).Sin()
    ),
    2.5f,
    DARKGRAY
  );

  if (solution_.has_value()) {
    DrawCircleV(
      solution_->impact_position,
      10.0f,
      ORANGE
    );
  }

  raylib::DrawTextEx(
    GetFontDefault(),
    TextFormat("%s: %s%0.1f", GetText(TextId::kAngleOnBow), angle_on_bow_.AsRad() > 0.0f ? "+" : "", angle_on_bow_.ToDeg()),
    target_position + raylib::Vector2(25.0f, 0.0f),
    50.0f,
    2.0f,
    DARKGRAY
  );

  if (solution_.has_value()) {
    raylib::DrawTextEx(
      GetFontDefault(),
      TextFormat(
        "%s: %0.1f\n%s: %s%0.1f",
        GetText(TextId::kLeadAngle), solution_->lead_angle.ToDeg(),
        GetText(TextId::kTorpedoGyroAngle), solution_->torpedo_gyro_angle.AsRad() > 0.0f ? "+" : "", solution_->torpedo_gyro_angle.ToDeg(),
        solution_->torpedo_gyro_angle.AsRad() > 0.0f ? "+" : "", solution_->torpedo_gyro_angle.ToDeg()
      ),
      ownship_position + raylib::Vector2(25.0f, 0.0f),
      50.0f,
      2.0f,
      DARKGRAY
    );
  }

  EndMode2D();
}

void Tdc::DoPanelImGui(
  Angle ownship_course
) {
  ImGui::Begin("Torpedovorhaltrechner", nullptr, ImGuiWindowFlags_AlwaysAutoResize);



  ImGui::Text("%s:", GetText(TextId::kInput));
  ImGui::Text("%s: %.1f", GetText(TextId::kOwnCourse), ownship_course.ToDeg());
  SliderFloatWithId("Torpedo Speed", &torpedo_speed_kn_, 1.0f, 60.0f, "%.0f", ImGuiSliderFlags_None, "%s (kn)", GetText(TextId::kTorpedoSpeed));
  target_bearing_.ImGuiSliderDegWithId("TargetBearing", -179.0f, 179.0f, "%.1f", "%s (deg)", GetText(TextId::kTargetBearing));
  SliderFloatWithId("TargetRange", &target_range_m_, 300.0f, 4000.0f, "%.0f", ImGuiSliderFlags_None, "%s (m)", GetText(TextId::kTargetRange));
  SliderFloatWithId("TargetSpeed", &target_speed_kn_, 0.0f, 40.0f, "%.0f", ImGuiSliderFlags_None, "%s (kn)", GetText(TextId::kTargetSpeed));
  angle_on_bow_.ImGuiSliderDegWithId("AngleOnBow", -180.0f, 180.0f, "%.1f", "%s (deg)", GetText(TextId::kAngleOnBow));

  ImGui::Separator();

  ImGui::Text("%s:", GetText(TextId::kOutput));
  if (!solution_.has_value()) {
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", GetText(TextId::kNoSolution));
  }
  else {
    TorpedoTriangleSolution const& solution = solution_.value();

    ImGui::Text("%s: %.1f deg", GetText(TextId::kTargetCourse), solution.target_course.ToDeg());
    ImGui::Text("%s: %.1f deg", GetText(TextId::kLeadAngle), solution.lead_angle.ToDeg());
    ImGui::Text("%s: %s %.1f deg", GetText(TextId::kTorpedoGyroAngle), solution.torpedo_gyro_angle.AsRad() >= 0.0f ? "R" : "L", solution.torpedo_gyro_angle.Abs().ToDeg());
    ImGui::Text("%s: %.1f s", GetText(TextId::kTimeToImpact), solution.torpedo_time_to_target_s);
  }

  ImGui::End();
}
