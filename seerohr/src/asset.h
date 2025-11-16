#pragma once

// c++ headers ------------------------------------------
#include <cstddef>

#include <optional>
#include <vector>

// project headers --------------------------------------
#include "mbase/public/access.h"

class IAssetManager {
public:
  virtual ~IAssetManager() = default;
  MBASE_DEFAULT_COPY_DISALLOW_MOVE(IAssetManager);

  static IAssetManager* Get();

  virtual std::optional<std::vector<std::byte>> LoadAsset(char const* asset_path) = 0;

protected:
  IAssetManager() = default;
};
