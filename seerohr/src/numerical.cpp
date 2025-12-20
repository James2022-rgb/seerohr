// TU header --------------------------------------------
#include "numerical.h"

// c++ headers ------------------------------------------
#include <vector>
#include <numbers>
#include <algorithm>

std::optional<float> FindRootsBisection(
  std::function<std::optional<float>(float)> const& evaluate,
  float a,
  float b,
  float tol,
  uint32_t max_iter
) {
  // Scan for brackets of x containing roots of `evaluate`.
  std::vector<std::pair<float, float>> brackets;
  {
    constexpr uint32_t kScanCount = 100;

    std::vector<float> xs;
    for (uint32_t i = 0; i <= kScanCount; ++i) {
      float x = a + (b - a) * static_cast<float>(i) / static_cast<float>(kScanCount);
      xs.push_back(x);
    }

    // x at scan points.
    std::vector<std::optional<float>> hs;
    hs.reserve(xs.size());
    for (float x : xs) {
      hs.push_back(evaluate(x));
    }

    for (uint32_t i = 0; i < kScanCount; ++i) {
      float x0 = xs[i];
      float x1 = xs[i + 1];
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
        // Catch even-multiplicity/near-miss by small |x| in the interval.
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
  } // Bracket scanning

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

  std::vector<float> roots;
  for (auto const& bracket : brackets) {
    std::optional<float> opt_root = bisection(bracket.first, bracket.second, tol, max_iter);
    if (opt_root.has_value()) {
      roots.emplace_back(opt_root.value());
    }
  }

  // Deduplicate roots that came from overlapping brackets
  std::sort(roots.begin(), roots.end());
  auto last = std::unique(
    roots.begin(), roots.end(),
    [tol](float a, float b) {
      return std::abs(a - b) < tol;
    }
  );
  roots.erase(last, roots.end());

  if (roots.empty()) {
    return std::nullopt;
  }

  {
    float x = 0.0f;
    auto best = std::min_element(roots.begin(), roots.end(),
      [](float a, float b) { return std::abs(a) < std::abs(b); });
    x = *best;

    // Final evaluation so any side effects occur.
    evaluate(x);

    return x;
  }
}
