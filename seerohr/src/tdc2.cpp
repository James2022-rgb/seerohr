// TU header --------------------------------------------
#include "tdc2.h"

// c++ headers ------------------------------------------
#include <cassert>

#include <algorithm>
#include <numbers>

// external headers -------------------------------------
#include "imgui.h"

// project headers --------------------------------------
#include "text.h"
#include "raylib_widgets.h"
#include "widgets.h"
#include "numerical.h"

// http://www.tvre.org/en/torpedo-calculator-t-vh-re-s3
// http://www.tvre.org/en/gyro-angled-torpedoes
// https://www.reddit.com/r/uboatgame/comments/1f005cj/trigonometry_and_geometry_explanations_of_popular/
// https://www.reddit.com/r/uboatgame/comments/1hja5a7/the_ultimate_lookup_table_compendium/
// https://patents.google.com/patent/DE935417C/de

namespace tdc2 {

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

    // Try to solve using torpedo triangle.

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
    raylib::Vector2 const impact_position = aiming_device_position + raylib::Vector2(
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

} // namespace

namespace {
struct ParallaxCorrectionSolver final {
  /// Solve for the parallax correction using geometric iteration.
  ///
  /// This method iteratively refines the parallax correction angle (delta) using geometric relationships.
  /// It uses the initial guess (`rho0`) and the observed target position to compute the correction.
  /// 
  /// ## Parameters
  /// - `rho0`: Initial guess for the torpedo gyro angle (Schusswinkel) in radians.
  /// - `aiming_device_position`: For computing the parallax-corrected impact position.
  /// - `ownship_course_rad`: The course of the ownship in radians.
  static bool SolveByGeometry(
    TorpedoSpec const& torpedo_spec,
    TorpedoTriangle const& triangle,
    float rho0,
    raylib::Vector2 const& aiming_device_position,
    float ownship_course_rad,
    ParallaxCorrectionSolution& out_pc_solution
  ) {
    constexpr uint32_t kIters = 64;
    constexpr float kTolerance = 1e-6f;
    constexpr float kLambda = 0.6f;

    // Target range, as observed from the aiming device.
    float const los = triangle.target_range_m;

    // Signed target bearing.
    float const omega1 = triangle.target_bearing.AsRad();

    // Signed angle on bow.
    float const gamma1 = triangle.angle_on_bow.AsRad();

    auto wrap_pi = [](float angle) -> float {
      return std::remainder(angle, 2.0f * std::numbers::pi_v<float>); // (-pi, pi]
    };

    // Target position, as observed from the aiming device.
    raylib::Vector2 const T { los * std::cos(omega1), los * std::sin(omega1) };

    float rho = rho0; // Initialize with initial guess for rho.

    for (uint32_t i = 0; i < kIters; ++i) {
      // Relative to ownship course.
      raylib::Vector2 const epf_offset = torpedo_spec.ComputeEquivalentPointOfFireOffset(rho);

      raylib::Vector2 const e_to_t = T - epf_offset;

      // Target bearing, as observed from this equivalent point of fire.
      float const omega2 = std::atan2(e_to_t.y, e_to_t.x);

      // Parallax correction delta.
      float const delta = wrap_pi(omega1 - omega2);

      // Signed angle on bow, as observed from the equivalent point of fire.
      float const gamma2 = wrap_pi(gamma1 - delta);

      // Lead angle as seen from the equivalent point of fire.
      float beta2 = 0.0f;
      {
        float sin_beta = (triangle.target_speed_kn / torpedo_spec.speed_kn) * std::sin(gamma2);

        if (sin_beta < -1.0f || 1.0f < sin_beta) {
          // No solution; target is too fast leaving no valid lead angle for given torpedo speed and target course.
          return false;
        }

        beta2 = std::asin(sin_beta);
      }

      // Desired rho.
      float const rho_target = wrap_pi(omega2 + beta2);

      // Relaxed update on the circle.
      float const step = wrap_pi(rho_target - rho);
      rho = wrap_pi(rho + kLambda * step);

      if (std::abs(step) < kTolerance) {
        // Converged.

        // Intercept angle, as seen from the equivalent point of fire.
        float const alpha2 = std::numbers::pi_v<float> - gamma2 - beta2;

        float const los2 = e_to_t.Length();
        float const torpedo_run_distance_m = los2 * (std::sin(gamma2) / std::sin(alpha2));

        float const torpedo_speed_mps = torpedo_spec.speed_kn * 1852.0f / 3600.0f;
        float const torpedo_time_to_target_s = torpedo_run_distance_m / torpedo_speed_mps;

        raylib::Vector2 const e = aiming_device_position + epf_offset.Rotate(ownship_course_rad - std::numbers::pi_v<float> / 2.0f);
        raylib::Vector2 const impact_position = e + raylib::Vector2(
          torpedo_run_distance_m * std::cos(ownship_course_rad - std::numbers::pi_v<float> / 2.0f + rho),
          torpedo_run_distance_m * std::sin(ownship_course_rad - std::numbers::pi_v<float> / 2.0f + rho)
        );

        out_pc_solution.delta = delta;
        out_pc_solution.rho = rho;
        out_pc_solution.gamma = gamma2;
        out_pc_solution.beta = beta2;
        out_pc_solution.epf_offset = epf_offset;
        out_pc_solution.torpedo_run_distance_m = torpedo_run_distance_m;
        out_pc_solution.torpedo_time_to_target_s = torpedo_time_to_target_s;
        out_pc_solution.impact_position = impact_position;
        return true;
      }
    }

    // No convergence.
    return false;
  }

  // Code currently disabled. SIEMENS approach as in the 1944 patent.
#if 0
  /// Numerically solve the equation H(Δ) = Δ - F(Δ) * sin(Δ + G(Δ)) for Δ
  /// where:
  ///  F(Δ) = 1/e * X(ρ)
  ///  G(Δ) = ω + θ(Δ)
  ///  X(ρ) = sqrt(x(ρ)^2 + y(ρ)^2)
  ///  θ(ρ) = arctan(y(ρ) / x(ρ))
  ///  ρ = ω + Δ - β
  ///
  /// ## Outputs
  /// Δ is the parallax correction angle, or Winkelparallaxverbesserung.
  /// ρ is the final torpedo gyro angle, or Schusswinkel.
  static bool SolveSiemens(
    TorpedoSpec const& torpedo_spec,
    TorpedoTriangle const& triangle,
    ParallaxCorrectionSolution& out_solution
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
      float gamma = 0.0f; // γ = θ1 - Δ: Angle on bow as seen from the equivalent point of fire.
      float beta = 0.0f;  // β: Lead angle as seen from the equivalent point of fire.
      float rho = 0.0f;   // ρ = ω + Δ - β: Final torpedo gyro angle
      raylib::Vector2 epf_offset {};
    } solution;

    // Evaluate H(Δ) = Δ - F(Δ) * sin(Δ + G(Δ))
    auto evaluate = [&ctx, &solution](float delta) -> std::optional<float> {
      float const gamma = ctx.gamma1 - delta;

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

      float const rho = -(ctx.omega + delta - beta);

      // X(ρ): Offset to the equivalent point of fire.
      raylib::Vector2 const epf_offset = ctx.torpedo_spec.ComputeEquivalentPointOfFireOffset(rho);

      // F(ρ) = 1/e * X(ρ)
      float f = 1.0f / ctx.e * std::sqrt(epf_offset.x * epf_offset.x + epf_offset.y * epf_offset.y);

      // θ(ρ): Angle between ownship course and line to the equivalent point of fire.
      float theta = std::atan2(epf_offset.y, epf_offset.x);

      // G(Δ) = ω + θ(ρ)
      float g = ctx.omega + theta;

      solution.delta = delta;
      solution.gamma = ctx.gamma1 - delta;
      solution.beta = beta;
      solution.rho = rho;
      solution.epf_offset = epf_offset;

      return delta - f * std::sin(delta + g);
    };

    std::optional<float> opt_root = FindRootsBisection(
      evaluate,
      -std::numbers::pi_v<float>,
      std::numbers::pi_v<float>,
      1e-6f,
      100
    );

    if (opt_root.has_value()) {
      float delta = opt_root.value();

      evaluate(delta);

      out_solution.delta = delta;
      out_solution.rho   = solution.rho;
      out_solution.gamma = solution.gamma;
      out_solution.beta = solution.beta;
      out_solution.epf_offset = solution.epf_offset;
      return true;
    }

    return false;
  }
#endif
};

} // namespace

raylib::Vector2 TorpedoSpec::ComputeEquivalentPointOfFireOffset(float rho) const {
  float const abs_rho = std::abs(rho);
  float const sin_abs_rho = std::sin(abs_rho);
  float const cos_abs_rho = std::cos(abs_rho);

  float x = this->distance_to_tube + this->reach + this->turn_radius * sin_abs_rho - (this->turn_radius * abs_rho + this->reach) * cos_abs_rho;
  float y = this->turn_radius * (1.0f - cos_abs_rho) - (this->turn_radius * abs_rho + this->reach) * sin_abs_rho;

  float sign = (rho >= 0.0f) ? -1.0f : 1.0f;

  // Positive (starboard) rho gives positive y.
  return { x, y * sign };
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

  pc_solution_ = std::nullopt;
  tri_solution_ = triangle.Solve(interm_, aiming_device_position);

  if (tri_solution_.has_value()) {
    ParallaxCorrectionSolution pc_solution;

#if 1
    bool result = ParallaxCorrectionSolver::SolveByGeometry(
      torpedo_spec_,
      triangle,
      tri_solution_->pseudo_torpedo_gyro_angle.AsRad(),
      aiming_device_position,
      ownship_course.AsRad(),
      pc_solution
    );
#else
    bool result = ParallaxCorrectionSolver::SolveSiemens(torpedo_spec_, triangle, pc_solution);
#endif

    if (result) {
      pc_solution_ = pc_solution;
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
  constexpr float kTriangleAlpha = 0.1f;

  raylib::Vector2 const target_position = ComputeTargetPosition(
    aiming_device_position,
    ownship_course,
    target_bearing_,
    target_range_m_
  );

  BeginMode2D(camera);

#if 0
  // Draw points representing equivalent point of fire, for rho values -120deg to +120deg.
  {
    for (float rho_deg = -120.0f; rho_deg <= 120.0f; rho_deg += 1.0f) {
      float rho = rho_deg * DEG2RAD;
      raylib::Vector2 const epf_offset = torpedo_spec_.ComputeEquivalentPointOfFireOffset(rho);
      raylib::Vector2 const epf_position = aiming_device_position + raylib::Vector2 (
        epf_offset.x * (ownship_course - Angle::RightAngle()).Cos() - epf_offset.y * (ownship_course - Angle::RightAngle()).Sin(),
        epf_offset.x * (ownship_course - Angle::RightAngle()).Sin() + epf_offset.y * (ownship_course - Angle::RightAngle()).Cos()
      );
      DrawCircleV(
        epf_position,
        3.0f,
        GRAY
      );
    }
  }
#endif

  // Draw a target ghost.
  {
    // Current target position - solid ship
    DrawShipSilhouette(
      target_position,
      target_length,
      target_beam,
      interm_.target_course,
      Color{ 180, 60, 60, 255 }  // Muted red
    );

    // If we have a parallax-corrected solution, draw a target ghost at the corrected impact position (fainter)
    if (pc_solution_.has_value()) {
      DrawShipSilhouette(
        pc_solution_->impact_position,
        target_length,
        target_beam,
        interm_.target_course,
        Color { 180, 60, 60, 80 }  // Same red but transparent (ghost)
      );
    }
  }

  // Draw a line from aiming device to target.
  DrawLineEx(
    aiming_device_position,
    target_position,
    5.0f,
    Fade(DARKGRAY, 0.1f)
  );

  // Draw angle on bow.
  DrawCircleSector(
    target_position,
    150.0f,
    interm_.target_course.ToDeg() - 90.0f,
    interm_.target_course.ToDeg() + angle_on_bow_.ToDeg() - 90.0f,
    32,
    Fade(GREEN, 0.08f)
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

  if (tri_solution_.has_value()) {
    // Draw lead angle.
    DrawCircleSector(
      aiming_device_position,
      150.0f,
      interm_.absolute_target_bearing.ToDeg() - 90.0f,
      interm_.absolute_target_bearing.ToDeg() + (angle_on_bow_.Sign() * tri_solution_->lead_angle.ToDeg()) - 90.0f,
      32,
      Fade(BLUE, 0.1f)
    );

    // Draw projected target course line to impact position.
    DrawLineStippled(
      target_position,
      tri_solution_->impact_position,
      5.0f,
      GRAY
    );

    // Draw the torpedo triangle (transparent green, dashed lines).
    {
      Color const triangle_color = Fade(GREEN, kTriangleAlpha);

      // Side 1: Aiming device to target (line of sight)
      DrawLineStippled(
        aiming_device_position,
        target_position,
        3.0f,
        triangle_color
      );

      // Side 2: Target to impact position (target run)
      DrawLineStippled(
        target_position,
        tri_solution_->impact_position,
        3.0f,
        triangle_color
      );

      // Side 3: Aiming device to impact position (torpedo run)
      DrawLineStippled(
        aiming_device_position,
        tri_solution_->impact_position,
        3.0f,
        triangle_color
      );

      // Draw filled triangle (both winding orders to handle any vertex arrangement)
      DrawTriangle(
        aiming_device_position,
        target_position,
        tri_solution_->impact_position,
        triangle_color
      );
      DrawTriangle(
        aiming_device_position,
        tri_solution_->impact_position,
        target_position,
        triangle_color
      );
    }
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

  // If we have a non-parallax-corrected solution, draw the impact position but fainter.
  if (tri_solution_.has_value()) {
    DrawCircleV(
      tri_solution_->impact_position,
      10.0f,
      Fade(GREEN, 0.5f)
    );
  }

  if (pc_solution_.has_value()) {
    raylib::Vector2 const epf_offset_physical = torpedo_spec_.ComputeEquivalentPointOfFireOffset(pc_solution_->rho);
    raylib::Vector2 const epf_offset_screen = { epf_offset_physical.x, -epf_offset_physical.y };

    raylib::Vector2 const epf_position = aiming_device_position + raylib::Vector2(
      epf_offset_screen.x * (ownship_course - Angle::RightAngle()).Cos() - epf_offset_screen.y * (ownship_course - Angle::RightAngle()).Sin(),
      epf_offset_screen.x * (ownship_course - Angle::RightAngle()).Sin() + epf_offset_screen.y * (ownship_course - Angle::RightAngle()).Cos()
    );

    // Draw the torpedo triangle from the equivalent point of fire (transparent blue).
    {
      Color const epf_triangle_color = Fade(BLUE, kTriangleAlpha);

      // Side 1: EPF to target (line of sight from EPF)
      DrawLineEx(
        epf_position,
        target_position,
        3.0f,
        epf_triangle_color
      );

      // Side 2: Target to impact position (target run)
      DrawLineEx(
        target_position,
        pc_solution_->impact_position,
        3.0f,
        epf_triangle_color
      );

      // Side 3: EPF to impact position (torpedo run from EPF)
      DrawLineEx(
        epf_position,
        pc_solution_->impact_position,
        3.0f,
        epf_triangle_color
      );

      // Draw filled triangle (both winding orders)
      DrawTriangle(
        epf_position,
        target_position,
        pc_solution_->impact_position,
        epf_triangle_color
      );
      DrawTriangle(
        epf_position,
        pc_solution_->impact_position,
        target_position,
        epf_triangle_color
      );
    }

    // Draw equivalent point of fire.
    DrawCircleV(
      epf_position,
      8.0f,
      Color { 255, 165, 0, 128 }
    );

    raylib::Vector2 tube_position = aiming_device_position + raylib::Vector2(
      torpedo_spec_.distance_to_tube * (ownship_course - Angle::RightAngle()).Cos(),
      torpedo_spec_.distance_to_tube * (ownship_course - Angle::RightAngle()).Sin()
    );

    // Torpedo track visualization with thickness
    {
      constexpr float kTrackThickness = 4.0f;
      Color const kTrackColor = Color { 255, 140, 0, 200 };  // Orange with some transparency

      // Gyro angle: positive is starboard, negative is port
      float const gyro_angle = pc_solution_->rho;

      // Forward vector at launch
      raylib::Vector2 const e0(
        (ownship_course - Angle::RightAngle()).Cos(),
        (ownship_course - Angle::RightAngle()).Sin()
      );
      // Left-normal vector at launch
      raylib::Vector2 const l0(
        -(ownship_course - Angle::RightAngle()).Sin(),
         (ownship_course - Angle::RightAngle()).Cos()
      );

      // p0: Tube position (start of reach)
      raylib::Vector2 const p0 = tube_position;
      
      // p1: End of reach / start of turn
      raylib::Vector2 const p1 = p0 + e0 * torpedo_spec_.reach;

      // Chord from the start of the turn to the end of the turn
      raylib::Vector2 const chord =
        e0 * (torpedo_spec_.turn_radius * std::sin(std::abs(gyro_angle))) +
        l0 * (gyro_angle > 0.0f ? 1.0f : -1.0f) * (torpedo_spec_.turn_radius * (1.0f - std::cos(std::abs(gyro_angle))));

      // p2: End of turn / start of final straight run
      raylib::Vector2 const p2 = p1 + chord;

      // 1. Draw the reach (gerader Vorlauf) - straight line from tube to turn start
      DrawLineEx(p0, p1, kTrackThickness, kTrackColor);

      // 2. Draw the turning arc with thickness
      {
        raylib::Vector2 const center = p1 + l0 * (gyro_angle > 0.0f ? 1.0f : -1.0f) * torpedo_spec_.turn_radius;

        float start_angle = std::atan2(p1.y - center.y, p1.x - center.x);
        float end_angle = std::atan2(p2.y - center.y, p2.x - center.x);
        
        if (gyro_angle > 0.0f) {
          if (start_angle > end_angle) {
            start_angle -= std::numbers::pi_v<float> * 2.0f;
          }
        }
        else {
          if (end_angle > start_angle) {
            end_angle -= std::numbers::pi_v<float> * 2.0f;
          }
        }

        // Draw arc as a series of line segments for thickness
        constexpr int kArcSegments = 32;
        float const angle_step = (end_angle - start_angle) / static_cast<float>(kArcSegments);
        
        for (int i = 0; i < kArcSegments; ++i) {
          float const a0 = start_angle + angle_step * static_cast<float>(i);
          float const a1 = start_angle + angle_step * static_cast<float>(i + 1);
          
          raylib::Vector2 const seg_start = center + raylib::Vector2(
            torpedo_spec_.turn_radius * std::cos(a0),
            torpedo_spec_.turn_radius * std::sin(a0)
          );
          raylib::Vector2 const seg_end = center + raylib::Vector2(
            torpedo_spec_.turn_radius * std::cos(a1),
            torpedo_spec_.turn_radius * std::sin(a1)
          );
          
          DrawLineEx(seg_start, seg_end, kTrackThickness, kTrackColor);
        }
      }

      // 3. Draw the final straight run from end of turn to impact position
      DrawLineEx(p2, pc_solution_->impact_position, kTrackThickness, kTrackColor);

      // Draw markers at key points
      DrawCircleV(p0, 5.0f, Color { 100, 100, 200, 255 });  // Tube position - blue
      DrawCircleV(p1, 4.0f, Color { 200, 100, 100, 255 });  // End of reach - red
      DrawCircleV(p2, 4.0f, Color { 255, 165, 0, 255 });    // End of turn - orange
    }

    // Draw impact position marker (parallax corrected)
    DrawCircleV(
      pc_solution_->impact_position,
      12.0f,
      Color { 255, 100, 0, 255 }  // Bright orange
    );
    DrawCircleLinesV(
      pc_solution_->impact_position,
      16.0f,
      Color { 255, 100, 0, 180 }
    );

    {
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
      constexpr float kSign = 1.0f;

      float const gyro_angle = kSign * pc_solution_->rho;

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

      #if 0
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
      #endif

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
    }
  }

  EndMode2D();
}

void Tdc::DoPanelImGui(
  Angle ownship_course
) {
  constexpr float kMaxTorpedoSpeedKn = 100.0f;
  constexpr float kMaxTargetSpeedKn = 100.0f;

  // Input section
  ImGui::BeginGroup();
  {
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s:", GetText(TextId::kInput));
    ImGui::Text("%s: %.1f", GetText(TextId::kOwnCourse), ownship_course.ToDeg());
    
    ImGui::PushItemWidth(180.0f);
    SliderFloatWithId("Torpedo Speed", &torpedo_spec_.speed_kn, 1.0f, kMaxTorpedoSpeedKn, "%.0f", ImGuiSliderFlags_None, "%s (kn)", GetText(TextId::kTorpedoSpeed));
    target_bearing_.ImGuiSliderDegWithId("TargetBearing", -179.0f, 179.0f, "%.2f", "%s (deg)", GetText(TextId::kTargetBearing));
    SliderFloatWithId("TargetRange", &target_range_m_, 300.0f, 4000.0f, "%.0f", ImGuiSliderFlags_None, "%s (m)", GetText(TextId::kTargetRange));
    SliderFloatWithId("TargetSpeed", &target_speed_kn_, 0.0f, kMaxTargetSpeedKn, "%.0f", ImGuiSliderFlags_None, "%s (kn)", GetText(TextId::kTargetSpeed));
    angle_on_bow_.ImGuiSliderDegWithId("AngleOnBow", -180.0f, 180.0f, "%.1f", "%s (deg)", GetText(TextId::kAngleOnBow));
    ImGui::PopItemWidth();
  }
  ImGui::EndGroup();

  ImGui::SameLine(0.0f, 30.0f);

  // Dials section
  ImGui::BeginGroup();
  {
    float target_bearing_deg = target_bearing_.ToDeg();
    if (BearingDialStacked("TargetBearingDial", GetText(TextId::kTargetBearing), 90.0f, &target_bearing_deg)) {
      target_bearing_ = Angle::FromDeg(target_bearing_deg);
    }

    ImGui::SameLine();

    float aob_deg = angle_on_bow_.ToDeg();
    if (AoBDialProcedural("AoBDial", GetText(TextId::kAngleOnBow), 100.0f, &aob_deg)) {
      angle_on_bow_ = Angle::FromDeg(aob_deg);
    }

    ImGui::SameLine();

    TorpGeschwUndGegnerfahrtDial(
      "TorpedoSpeedAndTargetSpeedDial",
      GetText(TextId::kTorpedoSpeedAndTargetSpeed),
      125.0f,
      &target_speed_kn_,
      &torpedo_spec_.speed_kn
    );

    ImGui::SameLine();

    {
      static const DialKnot kSchussentfernungKnots[] = {
        // value(hm), bearing_deg (continuous, increasing; 0°=north, 90°=east)
        {   3.0f,   0.0f },   // start at north
        {  10.0f,  70.0f },   // early region
        {  30.0f, 250.0f },   // big span (precision zone)
        { 100.0f, 330.0f },   // end near north, but leave a gap to 360°
      };

      float range_hm = target_range_m_ / 100.0f;

      if (TargetRangeDialNonLinear("TargetRangeDial", GetText(TextId::kTargetRange), 100.0f, &range_hm, kSchussentfernungKnots, IM_ARRAYSIZE(kSchussentfernungKnots))) {
        target_range_m_ = std::clamp(range_hm * 100.0f, 300.0f, 10000.0f);
      }
    }
  }
  ImGui::EndGroup();

  ImGui::SameLine(0.0f, 30.0f);

  // Output section
  ImGui::BeginGroup();
  {
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s:", GetText(TextId::kOutput));
    if (!tri_solution_.has_value()) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", GetText(TextId::kNoSolution));
    }
    else {
      TorpedoTriangleSolution const& solution = tri_solution_.value();

      ImGui::Text("%s: %.1f deg", GetText(TextId::kTargetCourse), solution.target_course.ToDeg());
      ImGui::Text("%s: %.1f deg", GetText(TextId::kLeadAngle), solution.lead_angle.ToDeg());
      ImGui::Text("%s: %s %.1f deg", GetText(TextId::kPseudoTorpedoGyroAngle), solution.pseudo_torpedo_gyro_angle.AsRad() >= 0.0f ? "R" : "L", solution.pseudo_torpedo_gyro_angle.Abs().ToDeg());
      ImGui::Text("%s: %.1f s", GetText(TextId::kTimeToImpact), solution.torpedo_time_to_target_s);

#if 1
      if (pc_solution_.has_value()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.5f, 1.0f), "Parallax Corrected:");
        ImGui::Text("%s: %.1f m", GetText(TextId::kTorpedoRunDistance), pc_solution_->torpedo_run_distance_m);
        ImGui::Text("%s: %.1f s", GetText(TextId::kTimeToImpact), pc_solution_->torpedo_time_to_target_s);
        ImGui::Text("Gyro Angle: %.1f deg", pc_solution_->rho * RAD2DEG);
        ImGui::Text("Parallax Correction Δ: %.2f deg", pc_solution_->delta * RAD2DEG);
      }
#endif
    }
  }
  ImGui::EndGroup();
}

} // namespace tdc2
