
// external headers -------------------------------------
#include "raylib.h"
#include "raylib-cpp.hpp"

#if defined(PLATFORM_WEB)
# include <emscripten/emscripten.h>
#endif

#include "imgui.h"
#include "rlImGui.h"

// project headers --------------------------------------
#if CONFIG_USE_RAYGUI
# include "raygui_integration.h"
# include "raygui_widgets.h"
#endif

#include "tdc.h"

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void InitializeApp();
void UpdateDrawFrame();

//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main() {
  // Initialization
  //--------------------------------------------------------------------------------
  raylib::Window window(kScreenWidth, kScreenHeight, "seerohr");

  SetTargetFPS(60);

  rlImGuiSetup(true);

  InitializeApp();

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
  
  //--------------------------------------------------------------------------------

  // Mainloop
  while (!WindowShouldClose()){
    UpdateDrawFrame();
  }
#endif

  // De-Initialization
  //--------------------------------------------------------------------------------
  rlImGuiShutdown();

  //--------------------------------------------------------------------------------

  return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

struct Ship final {
  raylib::Vector2 position;
  Angle course = Angle::FromDeg(0.0f); // North is 0 degrees, clockwise.
  float speed_kn = 0.0f;   // Speed in knots (nautical miles per hour).
};

class State final {
public:
  void InitializeApp() {
    constexpr float kPositionX = 5000.0f;
    constexpr float kPositionY = 2500.0f;

    {
      constexpr float kZoom = 0.45f;
      constexpr float kCenterX = kPositionX;
      constexpr float kCenterY = 1850.0f;

      float x = kCenterX - float(GetScreenWidth())  / 2.0f / kZoom;
      float y = kCenterY - float(GetScreenHeight()) / 2.0f / kZoom;

      camera_.SetTarget({ x, y });
      camera_.SetOffset({ 0.0f, 0.0f });
      camera_.SetRotation(0.0f);
      camera_.SetZoom(kZoom);
    }

    {
      ownship_.position = raylib::Vector2 { kPositionX, kPositionY };
      ownship_.course = Angle::FromDeg(0.0f); // North
      ownship_.speed_kn = 0.0f;               // Stationary
    }
  }

  void Tick() {
    if (raylib::Mouse::IsButtonDown(MOUSE_BUTTON_RIGHT))
    {
      raylib::Vector2 const delta = raylib::Mouse::GetDelta() / camera_.GetZoom();
      camera_.SetTarget(raylib::Vector2(camera_.GetTarget()) - delta);
    }

    {
      float const wheel = raylib::Mouse::GetWheelMove();
      if (wheel != 0.0f)
      {
        raylib::Vector2 const mouse_pos = raylib::Mouse::GetPosition();

        // Get the world point that is under the mouse
        raylib::Vector2 const mouse_world_pos = GetScreenToWorld2D(mouse_pos, camera_);

        // Set the offset to where the mouse is
        camera_.SetOffset(mouse_pos);

        // Set the target to match, so that the camera maps the world space point
        // under the cursor to the screen space point under the cursor at any zoom
        camera_.SetTarget(mouse_world_pos);

        // Zoom increment:
        // Uses log scaling to provide consistent zoom speed.
        float const scale = 0.2f * wheel;
        camera_.SetZoom(Clamp(expf(logf(camera_.GetZoom()) + scale), 0.125f, 64.0f));
      }
    }

    tdc_.Update(ownship_.course);
  }

  void Draw() {
    BeginDrawing();

    ClearBackground(RAYWHITE);

    {
      BeginMode2D(camera_);

      rlPushMatrix();
        rlTranslatef(5000.0f, 5000.0f, 0.0f);
        rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        DrawGrid(200, 50.0f);
      rlPopMatrix();

      // Ownship.
      rlPushMatrix();
        constexpr float kOwnshipBeam = 6.21f;
        constexpr float kOwnshipLength = 72.39f;
        rlTranslatef(ownship_.position.x, ownship_.position.y, 0.0f);
        rlRotatef(ownship_.course.ToDeg(), 0.0f, 0.0f, 1.0f);
        rlTranslatef(-ownship_.position.x, -ownship_.position.y, 0.0f);
        DrawEllipseV(ownship_.position, kOwnshipBeam, kOwnshipLength, LIGHTGRAY);
      rlPopMatrix();

      EndMode2D();
    }

    tdc_.DrawVisualization(
      camera_,
      ownship_.position,
      ownship_.course
    );

    {
      raylib::Vector2 const mouse_pos = raylib::Mouse::GetPosition();
      raylib::Vector2 const mouse_world_pos = GetScreenToWorld2D(mouse_pos, camera_);

      DrawCircleV(mouse_pos, 4.0f, DARKGRAY);
      raylib::DrawTextEx(
        GetFontDefault(),
        TextFormat("[%i, %i]", int32_t(mouse_world_pos.x), int32_t(mouse_world_pos.y)),
        mouse_pos + raylib::Vector2( -44.0f, -24.0f),
        20.0f,
        2.0f,
        BLACK
      );
    }

    {
      DrawRectangle(10, 10, 250, 80, Fade(SKYBLUE, 0.5f));
      DrawRectangleLines(10, 10, 250, 80, BLUE);

      raylib::DrawText("Controls:", 20, 20, 10, BLACK);
      raylib::DrawText("- Right Click + Drag to move camera", 40, 40, 10, DARKGRAY);
      raylib::DrawText("- Mouse Wheel to Zoom", 40, 60, 10, DARKGRAY);
    }

    {
      rlImGuiBegin();

      ImGui::ShowDemoWindow();

      {
        ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Zoom: %.2f", camera_.GetZoom());
        ImGui::Text("Target: (%.1f, %.1f)", camera_.GetTarget().x, camera_.GetTarget().y);
        ImGui::Text("Offset: (%.1f, %.1f)", camera_.GetOffset().x, camera_.GetOffset().y);
        ImGui::End();
      }

      {
        ImGui::Begin("U-Boat", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ownship_.course.ImGuiSliderDeg("Course (deg)", 0.0f, 359.99f, "%.1f");
        ImGui::End();
      }

      tdc_.DoPanelImGui(ownship_.course);

      rlImGuiEnd();
    }

    EndDrawing();
  }

private:
  raylib::Camera2D camera_;

  Ship ownship_;

  Tdc tdc_;
};
static State s_state;

void InitializeApp() {
  s_state.InitializeApp();
}

void UpdateDrawFrame(void) {
  s_state.Tick();
  s_state.Draw();
}
