// TU header --------------------------------------------
#include "tdc.h"

// c++ headers ------------------------------------------
#include <cstdint>
#include <cmath>

#include <algorithm>
#include <vector>
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

namespace {

Angle ComputeAbsoluteTargetBearing(
  Angle ownship_course,
  Angle relative_target_bearing
) {
  return ownship_course + relative_target_bearing;
}

raylib::Vector2 ComputeTargetPosition(
  raylib::Vector2 const& aiming_device_position,
  Angle ownship_course,
  Angle relative_target_bearing,
  float target_range_m
) {
  Angle const absolute_target_bearing = ComputeAbsoluteTargetBearing(
    ownship_course,
    relative_target_bearing
  );

  return {
    aiming_device_position.x + target_range_m * +(-absolute_target_bearing + Angle::RightAngle()).Cos(),
    aiming_device_position.y + target_range_m * -(-absolute_target_bearing + Angle::RightAngle()).Sin()
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
    raylib::Vector2 const& aiming_device_position
  ) const {
    assert(this->torpedo_speed_kn > 0.0f);

    // No solution using torpedo triangle; target course line is identical to ownship line.
    if (this->angle_on_bow.AsRad() == 0.0f || std::abs(this->angle_on_bow.AsRad()) == std::numbers::pi_v<float>) {
      {
        float target_speed_seen_from_torpedo_kn = ((this->angle_on_bow.AsRad() == 0.0f) ? -this->target_speed_kn : this->target_speed_kn) - this->torpedo_speed_kn;

        if (target_speed_seen_from_torpedo_kn >= 0.0f){
          // No solution; the target is too fast for the torpedo to ever catch up.
          return std::nullopt;
        }
      }

      // Closing: Positive
      // Moving away: Negative
      float signed_target_speed_kn = (this->angle_on_bow.AsRad() == 0.0f) ? this->target_speed_kn : -this->target_speed_kn;

      float torpedo_time_to_target_s = this->target_range_m / ((this->torpedo_speed_kn + signed_target_speed_kn) * 1852.0f / 3600.0f);
      float torpedo_run_distance_m = (this->torpedo_speed_kn * 1852.0f / 3600.0f) * torpedo_time_to_target_s;

      raylib::Vector2 const impact_position = aiming_device_position + raylib::Vector2 (
        torpedo_run_distance_m * (interm.absolute_target_bearing - Angle::RightAngle()).Cos(),
        torpedo_run_distance_m * (interm.absolute_target_bearing - Angle::RightAngle()).Sin()
      );

      Angle const pseudo_torpedo_gyro_angle = this->target_bearing;

      return TorpedoTriangleSolution {
        .target_course = interm.target_course,
        .lead_angle = Angle(0.0f),
        .intercept_angle = signed_target_speed_kn >= 0.0f ? Angle::Pi() : Angle(0.0f),
        .torpedo_time_to_target_s = torpedo_time_to_target_s,
        .pseudo_torpedo_gyro_angle = pseudo_torpedo_gyro_angle,
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
    Angle const pseudo_torpedo_gyro_angle = torpedo_course - interm.ownship_course;

    // `- Angle::RightAngle()` corrects for coordinate space difference.
    raylib::Vector2 const impact_position = aiming_device_position + raylib::Vector2 (
      torpedo_run_distance_m * (interm.absolute_target_bearing + (this->angle_on_bow.Sign() * lead_angle) - Angle::RightAngle()).Cos(),
      torpedo_run_distance_m * (interm.absolute_target_bearing + (this->angle_on_bow.Sign() * lead_angle) - Angle::RightAngle()).Sin()
    );

    return TorpedoTriangleSolution {
      .target_course = interm.target_course,
      .lead_angle = lead_angle,
      .intercept_angle = intercept_angle,
      .torpedo_time_to_target_s = torpedo_time_to_target_s,
      .pseudo_torpedo_gyro_angle = pseudo_torpedo_gyro_angle,
      .impact_position = impact_position,
    };
  }
};

/// Solver for Equivalent Point of Fire, or ideeller Torpedoeintrittsort as it is called in German.
struct EquivalentPointOfFireSolver final {
  /// Numerically solve the equation �� = F(��) * sin(�� + G(��)) for ��
  /// where:
  ///  F(��) = 1/e * X(��)
  ///  G(��) = �� + ��(��)
  ///  X(��) = sqrt(x(��)^2 + y(��)^2)
  ///  ��(��) = arctan(y(��) / x(��))
  ///  �� = �� + �� - ��
  ///
  /// in order to compute the parallax correction angle �� and the final torpedo gyro angle ��.
  ///
  /// ## Outputs
  /// �� is the parallax correction angle, or Winkelparallaxverbesserung.
  /// �� is the final torpedo gyro angle, or Schusswinkel.
  static bool Solve(
    TorpedoSpec const& torpedo_spec,
    TorpedoTriangle const& triangle,
    EquivalentPointOfFireSolution& out_solution
  ) {
    struct EvaluateContext final {
      TorpedoSpec const& torpedo_spec;
      float target_speed_kn;
      float e;      // Target range, as observed from the aiming device.
      float omega;  // Absolute value of target bearing, as observed from the aiming device.
      float gamma1; // Absolute value of angle on bow, as observed from the aiming device.
    } ctx {
      .torpedo_spec = torpedo_spec,
      .target_speed_kn = triangle.target_speed_kn,
      .e = triangle.target_range_m,
      .omega = triangle.target_bearing.Abs().AsRad(),
      .gamma1 = triangle.angle_on_bow.Abs().AsRad(),
    };

    struct Solution final {
      float delta = 0.0f;
      float gamma = 0.0f; // �� = ��1 - ��: Angle on bow as seen from the equivalent point of fire.
      float beta = 0.0f;  // ��: Lead angle as seen from the equivalent point of fire.
      float rho = 0.0f;   // �� = �� + �� - ��: Final torpedo gyro angle
    } solution;

    // Evaluate H(��) = �� - F(��) * sin(�� + G(��))
    auto evaluate = [&ctx, &solution](float delta) -> std::optional<float> {
      float const distance = ctx.torpedo_spec.distance_to_tube;
      float const reach = ctx.torpedo_spec.reach;
      float const turn_radius = ctx.torpedo_spec.turn_radius;

      float gamma = ctx.gamma1 - delta;

      float beta = 0.0f;  
      {
        float sin_beta = (ctx.target_speed_kn / ctx.torpedo_spec.speed_kn) * std::sin(gamma);

        //assert(-1.0f <= sin_beta && sin_beta <= 1.0f);
        if (sin_beta < -1.0f || 1.0f < sin_beta) {
          // No solution; target is too fast leaving no valid lead angle for given torpedo speed and target course.
          return std::nullopt;
        }

        beta = std::asin(sin_beta);
      }

      float rho = ctx.omega + delta - beta;
      float sin_rho = std::sin(rho);
      float cos_rho = std::cos(rho);

      // X(��): Offset to the equivalent point of fire.
      raylib::Vector2 const epf_offset = ctx.torpedo_spec.ComputeEquivalentPointOfFireOffset(delta, rho);

      // F(��) = 1/e * X(��)
      float f = 1.0f / ctx.e * std::sqrt(epf_offset.x * epf_offset.x + epf_offset.y * epf_offset.y);

      // ��(��): Angle between ownship course and line to the equivalent point of fire.
      float theta = std::atan(epf_offset.y / epf_offset.x);

      // G(��) = �� + ��(��)
      float g = ctx.omega + theta;

      solution.delta = delta;
      solution.gamma = ctx.gamma1 - delta;
      solution.beta = beta;
      solution.rho = rho;

      return delta - f * std::sin(delta + g);
    };

    // Scan for brackets of �� containing roots of H(��).
    std::vector<std::pair<float, float>> brackets;
    {
      constexpr uint32_t kScanCount = 100;

      std::vector<float> deltas;
      for (uint32_t i = 0; i <= kScanCount; ++i) {
        float a = -std::numbers::pi_v<float>;
        float b =  std::numbers::pi_v<float>;
        float delta = a + (b - a) * static_cast<float>(i) / static_cast<float>(kScanCount);
        deltas.push_back(delta);
      }

      // H(��) at scan points.
      std::vector<std::optional<float>> hs;
      hs.reserve(deltas.size());
      for (float delta : deltas) {
        hs.push_back(evaluate(delta));
      }

      for (uint32_t i = 0; i < kScanCount; ++i) {
        float x0 = deltas[i];
        float x1 = deltas[i + 1];
        std::optional<float> opt_y0 = hs[i];
        std::optional<float> opt_y1 = hs[i + 1];

        if (!opt_y0.has_value() || !opt_y1.has_value()) {
          continue;
        }

        float y0 = opt_y0.value();
        float y1 = opt_y1.value();

        if (std::isnan(y0) || std::isnan(y1)) {
          continue;
        }

        if (y0 == 0.0f) {
          brackets.emplace_back(std::make_pair(x0 - 1e-9f, x0 + 1e-9f));
        }
        else if (y0 * y1 < 0.0f) {
          // Sign change detected.
          brackets.emplace_back(std::make_pair(x0, x1));
        }
        else {
          // Catch even-multiplicity/near-miss by small |H(��)| in the interval.
          float xm = 0.5f * (x0 + x1);
          std::optional<float> opt_ym = evaluate(xm);

          if (opt_ym.has_value()) {
            float ym = opt_ym.value();

            if (std::isfinite(ym) && std::abs(ym) < 1e-8f) {
              float eps = (x1 - x0) * 0.5f;
              brackets.emplace_back(std::make_pair(xm - eps, xm + eps));
            }
          }
        }
      }
    }

    auto bisection = [&evaluate](float lo, float hi, float tol, uint32_t max_iter) -> std::optional<float> {
      std::optional<float> opt_flo = evaluate(lo);
      std::optional<float> opt_fhi = evaluate(hi);

      if (!opt_flo.has_value() || !opt_fhi.has_value()) {
        return std::nullopt;
      }

      float flo = opt_flo.value();
      float fhi = opt_fhi.value();

      if (!std::isfinite(flo) || !std::isfinite(fhi)) {
        return std::nullopt;
      }
      if (flo == 0.0f) {
        return lo;
      }
      if (fhi == 0.0f) {
        return hi;
      }
      if (flo * fhi > 0.0f) {
        // Not a valid bracket.
        return std::nullopt;
      }

      for (uint32_t iter = 0; iter < max_iter; ++iter) {
        float mid = 0.5f * (lo + hi);
        std::optional<float> opt_fmid = evaluate(mid);

        if (!opt_fmid.has_value()) {
          return std::nullopt;
        }

        float fmid = opt_fmid.value();

        if (!std::isfinite(fmid)) {
          return std::nullopt;
        }
        if (fmid == 0.0f || 0.5 * (hi - lo) < tol) {
          return mid;
        }
        if (flo * fmid < 0.0f) {
          hi = mid;
          fhi = fmid;
        }
        else {
          lo = mid;
          flo = fmid;
        }
      }
      return 0.5f * (lo + hi);
    };

    constexpr float kTolerance = 1e-12f;

    std::vector<float> roots;
    for (auto const& bracket : brackets) {
      std::optional<float> opt_root = bisection(bracket.first, bracket.second, kTolerance, 100);
      if (opt_root.has_value()) {
        roots.emplace_back(opt_root.value());
      }
    }

    // Deduplicate roots that came from overlapping brackets
    std::sort(roots.begin(), roots.end());
    auto last = std::unique(
      roots.begin(), roots.end(),
      [](float a, float b) {
        return std::abs(a - b) < kTolerance;
      }
    );
    roots.erase(last, roots.end());

    if (!roots.empty()) {
      float delta = roots.front();
      evaluate(delta);

      out_solution.delta = delta;
      out_solution.rho   = solution.rho;
      return true;
    }

    return false;
  }
};

} // namespace

raylib::Vector2 TorpedoSpec::ComputeEquivalentPointOfFireOffset(float delta, float rho) const {
  float const sin_rho = std::sin(rho);
  float const cos_rho = std::cos(rho);

  // X(��): Offset to the equivalent point of fire.
  float x = this->distance_to_tube + this->reach + this->turn_radius * sin_rho - (this->turn_radius * rho + this->reach) * cos_rho;
  float y = this->turn_radius * (1.0f - cos_rho) - (this->turn_radius * rho + this->reach) * sin_rho;

  return { x, y };
}

void Tdc::Update(
  Angle ownship_course,
  raylib::Vector2 const& aiming_device_position
) {
  TorpedoTriangle triangle {
    .torpedo_speed_kn = torpedo_spec_.speed_kn,
    .target_bearing = target_bearing_,
    .target_range_m = target_range_m_,
    .target_speed_kn = target_speed_kn_,
    .angle_on_bow = angle_on_bow_
  };

  interm_ = triangle.PrepareSolve(ownship_course);

  solution_ = triangle.Solve(interm_, aiming_device_position);

  if (solution_.has_value()) {
    EquivalentPointOfFireSolution epf_solution;
    if (EquivalentPointOfFireSolver::Solve(torpedo_spec_, triangle, epf_solution)) {
      epf_solution_ = epf_solution;
    }
    else {
      epf_solution_ = std::nullopt;
    }
  }
}

void Tdc::DrawVisualization(
  raylib::Camera2D const& camera,
  raylib::Vector2 const& ownship_position,
  raylib::Vector2 const& aiming_device_position,
  Angle ownship_course,
  float target_beam,
  float target_length
) const {
  raylib::Vector2 const target_position = ComputeTargetPosition(
    aiming_device_position,
    ownship_course,
    target_bearing_,
    target_range_m_
  );

  BeginMode2D(camera);

  // Draw a target ghost.
  {
    rlPushMatrix();
      rlTranslatef(target_position.x, target_position.y, 0.0f);
      rlRotatef(interm_.target_course.ToDeg(), 0.0f, 0.0f, 1.0f);
      rlTranslatef(-target_position.x, -target_position.y, 0.0f);
      DrawEllipseV(
        target_position,
        target_beam, target_length,
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
          target_beam, target_length,
          Color { 230, 41, 55, 64 }
        );
      rlPopMatrix();
    }
  }

  // Draw a line from aiming device to target.
  DrawLineEx(
    aiming_device_position,
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
    // Draw pseudo torpedo gyro angle.
    {
      DrawCircleSector(
        aiming_device_position,
        200.0f,
        ownship_course.ToDeg() - 90.0f,
        ownship_course.ToDeg() + solution_->pseudo_torpedo_gyro_angle.ToDeg() - 90.0f,
        32,
        Fade(ORANGE, 0.25f)
      );
    }

    // Draw lead angle.
    DrawCircleSector(
      aiming_device_position,
      150.0f,
      interm_.absolute_target_bearing.ToDeg() - 90.0f,
      interm_.absolute_target_bearing.ToDeg() + (angle_on_bow_.Sign() * solution_->lead_angle.ToDeg()) - 90.0f,
      32,
      Fade(BLUE, 0.25f)
    );

    // Draw torpedo course line to impact position.
    DrawLineEx(
      aiming_device_position,
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
        GetText(TextId::kPseudoTorpedoGyroAngle), solution_->pseudo_torpedo_gyro_angle.AsRad() > 0.0f ? "+" : "", solution_->pseudo_torpedo_gyro_angle.ToDeg(),
        solution_->pseudo_torpedo_gyro_angle.AsRad() > 0.0f ? "+" : "", solution_->pseudo_torpedo_gyro_angle.ToDeg()
      ),
      ownship_position + raylib::Vector2(25.0f, 0.0f),
      50.0f,
      2.0f,
      DARKGRAY
    );
  }

  if (epf_solution_.has_value()) {
    raylib::Vector2 tube_position = aiming_device_position + raylib::Vector2(
      torpedo_spec_.distance_to_tube * (ownship_course - Angle::RightAngle()).Cos(),
      torpedo_spec_.distance_to_tube * (ownship_course - Angle::RightAngle()).Sin()
    );

    {
      // Draw torpedo reach.
      DrawLineEx(
        tube_position,
        tube_position + raylib::Vector2(
          torpedo_spec_.reach * (ownship_course - Angle::RightAngle()).Cos(),
          torpedo_spec_.reach * (ownship_course - Angle::RightAngle()).Sin()
        ),
        5.0f,
        ORANGE
      );

      raylib::Vector2 reach_end_position = tube_position + raylib::Vector2(
        torpedo_spec_.reach * (ownship_course - Angle::RightAngle()).Cos(),
        torpedo_spec_.reach * (ownship_course - Angle::RightAngle()).Sin()
      );

      // Draw two circles representing torpedo turn radius.
      raylib::Vector2 const starboard_turn_center = reach_end_position + raylib::Vector2(
        torpedo_spec_.turn_radius * (ownship_course + Angle::RightAngle() - Angle::RightAngle()).Cos(),
        torpedo_spec_.turn_radius * (ownship_course + Angle::RightAngle() - Angle::RightAngle()).Sin()
      );
      raylib::Vector2 const port_turn_center = reach_end_position + raylib::Vector2(
        torpedo_spec_.turn_radius * (ownship_course - Angle::RightAngle() - Angle::RightAngle()).Cos(),
        torpedo_spec_.turn_radius * (ownship_course - Angle::RightAngle() - Angle::RightAngle()).Sin()
      );
      DrawCircleV(
        starboard_turn_center,
        torpedo_spec_.turn_radius,
        Fade(PURPLE, 0.25f)
      );
      DrawCircleV(
        port_turn_center,
        torpedo_spec_.turn_radius,
        Fade(PURPLE, 0.25f)
      );
    }

    {
      // So gyro angle positive is starboard, negative port.
      constexpr float kSign = -1.0f;

      float const gyro_angle = kSign * epf_solution_->rho;

      // Forward vector at launch.
      raylib::Vector2 const e0(
        (ownship_course - Angle::RightAngle()).Cos(),
        (ownship_course - Angle::RightAngle()).Sin()
      );
      // Left-normal vector at launch.
      raylib::Vector2 const l0(
        -(ownship_course - Angle::RightAngle()).Sin(),
         (ownship_course - Angle::RightAngle()).Cos()
      );

      raylib::Vector2 const p0 = tube_position;
      raylib::Vector2 const p1 = p0 + e0 * torpedo_spec_.reach;

      // Chord from the start of the turn to the end of the turn, for the constant-radius, constant-curvature turn.
      raylib::Vector2 const chord =
        e0*(torpedo_spec_.turn_radius * std::sin(std::abs(gyro_angle))) +
        l0*(gyro_angle > 0.0f ? 1.0f : -1.0f) * (torpedo_spec_.turn_radius * (1.0f - std::cos(std::abs(gyro_angle))));

      // End position after the turn.
      raylib::Vector2 const p2 = p1 + chord;

      DrawCircleV(
        p0,
        4.0f,
        PURPLE
      );
      DrawCircleV(
        p1,
        4.0f,
        RED
      );
      DrawCircleV(
        p2,
        4.0f,
        ORANGE
      );

      // Draw the turning arc.
      {
        raylib::Vector2 const center = p1 + l0 * (gyro_angle > 0.0f ? 1.0f : -1.0f) * torpedo_spec_.turn_radius;

        float start_angle = std::atan2(p1.y - center.y, p1.x - center.x);
        float end_angle = std::atan2(p2.y - center.y, p2.x - center.x);
        if (gyro_angle > 0.0f) {
          if (start_angle > end_angle) {
            start_angle += std::numbers::pi_v<float> * 2.0f;
          }
        }
        else {
          if (end_angle > start_angle) {
            end_angle += std::numbers::pi_v<float> * 2.0f;
          }
        }

        DrawCircleSectorLines(
          center,
          torpedo_spec_.turn_radius,
          start_angle * RAD2DEG,
          end_angle * RAD2DEG,
          64,
          ORANGE
        );
      }

      float const torpedo_final_course = ownship_course.AsRad() + gyro_angle;

      // Draw a line representing the torpedo course after the turn.
      DrawLineEx(
        p2,
        p2 + raylib::Vector2(
          10000.0f * (Angle(torpedo_final_course) - Angle::RightAngle()).Cos(),
          10000.0f * (Angle(torpedo_final_course) - Angle::RightAngle()).Sin()
        ),
        5.0f,
        ORANGE
      );

      raylib::Vector2 epf_offset = torpedo_spec_.ComputeEquivalentPointOfFireOffset(epf_solution_->delta, epf_solution_->rho);
      epf_offset.SetY(-epf_offset.y); // Invert Y axis for our coordinate system.
      raylib::Vector2 const equivalent_point_of_fire = aiming_device_position + epf_offset;

      DrawCircleV(
        equivalent_point_of_fire,
        5.0f,
        PURPLE
      );
    }
  }

  EndMode2D();
}

void Tdc::DoPanelImGui(
  Angle ownship_course
) {
  constexpr float kMaxTorpedoSpeedKn = 100.0f;
  constexpr float kMaxTargetSpeedKn = 100.0f;

  ImGui::Begin("Torpedovorhaltrechner", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("%s:", GetText(TextId::kInput));
  ImGui::Text("%s: %.1f", GetText(TextId::kOwnCourse), ownship_course.ToDeg());
  SliderFloatWithId("Torpedo Speed", &torpedo_spec_.speed_kn, 1.0f, kMaxTorpedoSpeedKn, "%.0f", ImGuiSliderFlags_None, "%s (kn)", GetText(TextId::kTorpedoSpeed));
  target_bearing_.ImGuiSliderDegWithId("TargetBearing", -179.0f, 179.0f, "%.1f", "%s (deg)", GetText(TextId::kTargetBearing));
  SliderFloatWithId("TargetRange", &target_range_m_, 300.0f, 4000.0f, "%.0f", ImGuiSliderFlags_None, "%s (m)", GetText(TextId::kTargetRange));
  SliderFloatWithId("TargetSpeed", &target_speed_kn_, 0.0f, kMaxTargetSpeedKn, "%.0f", ImGuiSliderFlags_None, "%s (kn)", GetText(TextId::kTargetSpeed));
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
    ImGui::Text("%s: %s %.1f deg", GetText(TextId::kPseudoTorpedoGyroAngle), solution.pseudo_torpedo_gyro_angle.AsRad() >= 0.0f ? "R" : "L", solution.pseudo_torpedo_gyro_angle.Abs().ToDeg());
    ImGui::Text("%s: %.1f s", GetText(TextId::kTimeToImpact), solution.torpedo_time_to_target_s);

    if (epf_solution_.has_value()) {
      ImGui::Text("Delta: %.3f rad (%.1f deg)", epf_solution_->delta, epf_solution_->delta * RAD2DEG);
      ImGui::Text("Gyro Angle: %.1f deg", epf_solution_->rho * RAD2DEG);
    }
  }

  ImGui::End();
}
