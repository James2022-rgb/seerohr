#pragma once

// c++ headers ------------------------------------------
#include <cstdint>

#include <optional>
#include <functional>

std::optional<float> FindRootsBisection(
  std::function<std::optional<float>(float)> const& evaluate,
  float a,
  float b,
  float tol,
  uint32_t max_iter
);
