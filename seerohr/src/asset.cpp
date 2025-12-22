// TU header --------------------------------------------
#include "asset.h"

// c++ headers ------------------------------------------
#include <cinttypes>

#include <memory>
#include <mutex>
#include <atomic>
#include <map>

// project headers --------------------------------------
#include "mbase/public/platform.h"

// conditional c++ headers ------------------------------
#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
# include <filesystem>
# include <fstream>
#elif MBASE_PLATFORM_WEB
# include <filesystem>
#endif

// conditional external headers -------------------------
#if MBASE_PLATFORM_WEB
# include <emscripten/fetch.h>
# include <emscripten/console.h>

# include <emscripten/emscripten.h> // emscripten_sleep
#endif

namespace {

std::filesystem::path ResolveAssetPath(char const* asset_path) {
#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
  // Assume that the current working directory has the "assets" directory, and `asset_path` is relative to it.
  static std::filesystem::path const kRootPath = "assets";
  return kRootPath / asset_path;
#elif MBASE_PLATFORM_WEB
  // Assume that the "assets" directory is served at the same level as the HTML file, and `asset_path` is relative to it.
  static std::filesystem::path const kRootPath = "/seerohr/assets"; // Root-relative path in the web server.
  return kRootPath / asset_path;
#else
  // MBASE_LOG_ERROR("ResolveAssetPath: Not implemented for this platform.");
  return std::nullopt;
#endif
}

std::mutex s_ptr_mutex;
std::unique_ptr<IAssetManager> s_asset_manager;

}

class AssetManager final : public IAssetManager {
public:
  [[nodiscard]] AssetManager() = default;
  ~AssetManager() override = default;
  MBASE_DISALLOW_COPY_MOVE(AssetManager);

  //
  // IAssetManager implementation
  //

  std::optional<std::vector<std::byte>> LoadAsset(char const* asset_path) override {
    std::filesystem::path const resolved_path = ResolveAssetPath(asset_path);

#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
    std::fstream file(resolved_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
      return std::nullopt;
    }

    std::streampos size = 0;
    {
      file.seekg(0, std::ios::end);
      size = file.tellg();
      file.seekg(0, std::ios::beg);
    }

    std::vector<std::byte> file_buffer;
    file_buffer.resize(size);
    file.read(reinterpret_cast<char*>(file_buffer.data()), size);

    return file_buffer;
#elif MBASE_PLATFORM_WEB
    struct FetchContext {
      std::atomic<bool> done{false};
      std::atomic<bool> success{false};
      std::vector<std::byte> data;
    };

    FetchContext ctx;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.userData = &ctx;

    attr.onsuccess = [](emscripten_fetch_t* fetch) {
      FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
      printf("Fetch succeeded: %s, %llu bytes\n", fetch->url, fetch->numBytes);
      ctx->data.resize(fetch->numBytes);
      memcpy(ctx->data.data(), fetch->data, fetch->numBytes);
      ctx->success = true;
      ctx->done = true;
      emscripten_fetch_close(fetch);
    };

    attr.onerror = [](emscripten_fetch_t* fetch) {
      FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
      printf("Fetch failed: %s, HTTP status %d\n", fetch->url, fetch->status);
      ctx->success = false;
      ctx->done = true;
      emscripten_fetch_close(fetch);
    };

    printf("Fetching asset: %s\n", resolved_path.c_str());
    emscripten_fetch(&attr, resolved_path.c_str());

    // Wait for completion using ASYNCIFY
    while (!ctx.done) {
      emscripten_sleep(10);
    }

    if (!ctx.success) {
      return std::nullopt;
    }

    return std::move(ctx.data);
#else
    // MBASE_LOG_ERROR("AssetManager::LoadAsset: Not implemented for this platform.");
    return std::nullopt;
#endif
  }

private:
};

IAssetManager* IAssetManager::Get() {
  std::lock_guard lock(s_ptr_mutex);
  if (s_asset_manager == nullptr) {
    s_asset_manager = std::make_unique<AssetManager>();
  }
  return s_asset_manager.get();
}
