# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**seerohr** is a C++23 application that visualizes Torpedo Data Computer (TDC / Torpedovorhaltrechner) calculations. It renders torpedo intercept geometry and custom naval instrument widgets. Builds for desktop (Windows/Linux) and web (Emscripten/WebAssembly).

## Build Commands

All build scripts are batch files in `seerohr/`. The CMake source directory is `seerohr/` (not the repo root). Clone with `--recursive` to pull submodules.

### Desktop (Windows)

```bash
# Generate Visual Studio project (from seerohr/)
cmake -B build

# Build
cmake --build build

# Fresh regenerate
cmake --fresh -B build
```

### Web (Emscripten)

```bash
# Generate (from seerohr/)
mkdir build_web && cd build_web
emcmake cmake .. -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXECUTABLE_SUFFIX=".html"

# Build
cmake --build build_web

# Deploy (copies .html/.js/.wasm + assets to deploy_web/)
# See dpl_web.bat

# Serve locally
simple-http-server deploy_web
```

### CI

- **Native** (Windows MSVC + Linux Clang): `.github/workflows/cmake-multi-platform.yml`
- **Web** (Emscripten → GitHub Pages): `.github/workflows/cmake-web.yml`

Linux requires: `libasound2-dev libgl1-mesa-dev libxcursor-dev libxi-dev libxinerama-dev libxrandr-dev libxxf86vm-dev`

## Architecture

### Directory Layout

- `seerohr/src/` — All application source code
- `seerohr/thirdparty/` — Git submodules (raylib, imgui, rlImGui, raylib-cpp)
- `seerohr/assets/` — Runtime assets (M+ fonts for Japanese/international text)
- `mbase/` — Shared C++ utilities library (git submodule, separate repo)

### Key Source Files

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point, window/ImGui init, main loop, U-boat rendering |
| `tdc2.cpp/h` | TDC solver: torpedo triangle, parallax correction, intercept geometry |
| `widgets.cpp/h` | Custom ImGui dial widgets (AoB, bearing, torpedo speed, range) |
| `angle.cpp/h` | Type-safe `Angle` wrapper (internal radians, degree conversions) |
| `asset.cpp/h` | `IAssetManager` interface for font loading |
| `numerical.cpp/h` | Numerical computation helpers |
| `text.cpp/h` | Text rendering utilities |
| `raylib_widgets.cpp/h` | Raylib 2D drawing utilities |

### Rendering Stack

- **raylib** — Window management, 2D graphics rendering, camera system
- **Dear ImGui** (via **rlImGui** bridge) — All UI panels, controls, and custom dial widgets
- raygui is present but disabled (`USE_RAYGUI=OFF`)

### Platform Abstraction

`main.cpp` uses `#ifdef PLATFORM_WEB` for Emscripten-specific paths (`emscripten_set_main_loop` vs standard while loop, `emscripten_run_script` for URL opening). Desktop uses `ShellExecuteA` (Windows) or `xdg-open` (Linux).

### Key Types

- `tdc2::Tdc` — Core solver computing torpedo intercept solutions
- `tdc2::TorpedoTriangle` — Geometric triangle for intercept calculation
- `tdc2::TorpedoSpec` — Physical torpedo parameters (reach, speed, turn radius)
- `tdc2::ParallaxCorrectionSolution` — Full output with computed angles and positions
- `tdc2::Ship` — Vessel with position, course, speed
- `Angle` — Radian-based angle with degree conversion operators

## Conventions

- C++23 standard (`CMAKE_CXX_STANDARD 23`)
- MSVC on Windows, Clang on Linux
- Namespaced code (e.g., `tdc2::`)
- `std::optional<T>` for fallible operations
- Compiler warnings treated as errors on GCC/Clang (`-Werror`)
