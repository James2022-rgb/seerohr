// external headers -------------------------------------
#include "raylib.h"
#include "raylib-cpp.hpp"

#if defined(PLATFORM_WEB)
# include <emscripten/emscripten.h>
#endif

#include "mbase/public/platform.h"

#include "imgui.h"
#include "imgui_impl_raylib.h"

// project headers --------------------------------------
#if CONFIG_USE_RAYGUI
# include "raygui_integration.h"
# include "raygui_widgets.h"
#endif

#include "asset.h"
#include "text.h"
#include "angle.h"
#include "tdc2.h"
#include "widgets.h"

#if defined(_MSC_VER)
# pragma execution_character_set("utf-8")
#endif

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
constexpr int kScreenWidth = 1920;
constexpr int kScreenHeight = 1080;

constexpr char const* kGitHubRepoUrl = "https://github.com/James2022-rgb/seerohr";

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void InitializeApp(ImFont* im_font);
void UpdateDrawFrame();

//----------------------------------------------------------------------------------
// Helper Functions - Ship Drawing
//----------------------------------------------------------------------------------

/// Draw a simplified U-boat silhouette (top-down view)
/// If min_screen_length > 0, the ship will be scaled up to be at least that many pixels long on screen
void DrawUBoatSilhouette(
  raylib::Vector2 const& position,
  float length,
  float beam,
  Angle course,
  Color hull_color,
  float camera_zoom = 1.0f,
  float min_screen_length = 0.0f
) {
  // Calculate effective dimensions - optionally scale up for visibility
  float effective_length = length;
  float effective_beam = beam;
  
  if (min_screen_length > 0.0f && camera_zoom > 0.0f) {
    float screen_length = length * camera_zoom;
    if (screen_length < min_screen_length) {
      float scale = min_screen_length / screen_length;
      effective_length = length * scale;
      effective_beam = beam * scale;
    }
  }

  float const half_length = effective_length * 0.5f;
  float const half_beam = effective_beam * 0.5f;

  // Direction vectors
  float const cos_c = (course - Angle::RightAngle()).Cos();
  float const sin_c = (course - Angle::RightAngle()).Sin();

  raylib::Vector2 const fwd{ cos_c, sin_c };
  raylib::Vector2 const right{ -sin_c, cos_c };

  // Hull points - pointed bow, rounded stern
  raylib::Vector2 const bow = position + fwd * half_length;
  raylib::Vector2 const stern = position - fwd * half_length * 0.85f;
  raylib::Vector2 const stern_left = stern + right * half_beam * 0.5f;
  raylib::Vector2 const stern_right = stern - right * half_beam * 0.5f;

  // Mid-hull (widest point, slightly aft of center)
  raylib::Vector2 const mid_left = position + right * half_beam - fwd * half_length * 0.15f;
  raylib::Vector2 const mid_right = position - right * half_beam - fwd * half_length * 0.15f;

  // Forward taper points
  raylib::Vector2 const fwd_left = position + fwd * half_length * 0.55f + right * half_beam * 0.45f;
  raylib::Vector2 const fwd_right = position + fwd * half_length * 0.55f - right * half_beam * 0.45f;

  // Draw hull triangles (counter-clockwise winding order for raylib)
  DrawTriangle(bow, fwd_right, fwd_left, hull_color);
  DrawTriangle(fwd_left, fwd_right, mid_right, hull_color);
  DrawTriangle(fwd_left, mid_right, mid_left, hull_color);
  DrawTriangle(mid_left, mid_right, stern_right, hull_color);
  DrawTriangle(mid_left, stern_right, stern_left, hull_color);

  // Conning tower (darker)
  float const tower_length = effective_length * 0.12f;
  float const tower_width = effective_beam * 0.35f;
  raylib::Vector2 const tower_center = position + fwd * half_length * 0.05f;

  Color tower_color = hull_color;
  tower_color.r = static_cast<unsigned char>(tower_color.r * 0.6f);
  tower_color.g = static_cast<unsigned char>(tower_color.g * 0.6f);
  tower_color.b = static_cast<unsigned char>(tower_color.b * 0.6f);

  raylib::Vector2 const t1 = tower_center + fwd * tower_length * 0.5f + right * tower_width * 0.5f;
  raylib::Vector2 const t2 = tower_center + fwd * tower_length * 0.5f - right * tower_width * 0.5f;
  raylib::Vector2 const t3 = tower_center - fwd * tower_length * 0.5f - right * tower_width * 0.5f;
  raylib::Vector2 const t4 = tower_center - fwd * tower_length * 0.5f + right * tower_width * 0.5f;

  DrawTriangle(t1, t3, t4, tower_color);
  DrawTriangle(t1, t2, t3, tower_color);
}

//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main() {
  // Initialization
  //--------------------------------------------------------------------------------
  raylib::Window window(kScreenWidth, kScreenHeight, "seerohr");

  SetTargetFPS(60);

  ImGui::CreateContext();
  {
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_RendererHasTextures;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  }

  ImGui::StyleColorsDark();

  SetCurrentLanguage(GetSystemLanguageOrEnglish());

  ImFont* im_font = nullptr;
  {
    std::optional<std::vector<std::byte>> opt_bytes = IAssetManager::Get()->LoadAsset("mplus_fonts/Mplus1-Regular.ttf");
    assert(opt_bytes.has_value());

    void* bytes = ImGui::MemAlloc(opt_bytes->size());
    memcpy(bytes, opt_bytes->data(), opt_bytes->size());

    ImGuiIO& io = ImGui::GetIO();
    im_font = io.Fonts->AddFontFromMemoryTTF(
      bytes, int(opt_bytes->size()), 0.0f,
      nullptr,
      io.Fonts->GetGlyphRangesJapanese()
    );
  }

  InitializeApp(im_font);

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 0, true);
#else
  
  //--------------------------------------------------------------------------------

  // Mainloop
  while (!WindowShouldClose()){
    UpdateDrawFrame();
  }
#endif

  // De-Initialization
  //--------------------------------------------------------------------------------
  ImGui_ImplRaylib_Shutdown();
  ImGui::DestroyContext();

  //--------------------------------------------------------------------------------

  return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

struct Ship final {
  float distance_to_aiming_device = 0.0f;

  raylib::Vector2 position;
  Angle course = Angle::FromDeg(0.0f); // North is 0 degrees, clockwise.
  float speed_kn = 0.0f;   // Speed in knots (nautical miles per hour).

  raylib::Vector2 GetAimingDevicePosition() const {
    return raylib::Vector2 {
      position.x + distance_to_aiming_device * (course - Angle::RightAngle()).Cos(),
      position.y + distance_to_aiming_device * (course - Angle::RightAngle()).Sin()
    };
  }
};

class State final {
public:
  void InitializeApp(ImFont* im_font) {
    constexpr float kPositionX = 5000.0f;
    constexpr float kPositionY = 2500.0f;

    {
      constexpr float kZoom = 0.55f;
      constexpr float kCenterX = kPositionX;
      constexpr float kCenterY = kPositionY;  // Use ship Y position

      float x = kCenterX - float(GetScreenWidth())  / 2.0f / kZoom;
      float y = kCenterY - float(GetScreenHeight()) / 2.0f / kZoom;

      camera_.SetTarget({ x, y });
      camera_.SetOffset({ 0.0f, 0.0f });
      camera_.SetRotation(0.0f);
      camera_.SetZoom(kZoom);
    }

    {
      ownship_.position = raylib::Vector2 { kPositionX, kPositionY };
      ownship_.course = Angle::FromDeg(90.0f); // East
      ownship_.speed_kn = 0.0f;                // Stationary
    }

    im_font_ = im_font;

    // Setup custom ImGui style for a cleaner look
    SetupImGuiStyle();
  }

  void SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Rounding
    style.WindowRounding = 0.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    
    // Padding and spacing
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    
    // Colors - dark theme with blue accents
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.10f, 0.12f, 0.94f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.12f, 0.14f, 0.90f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.25f, 0.30f, 0.50f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.40f, 0.70f, 0.60f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.50f, 0.82f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.55f, 0.90f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.40f, 0.70f, 0.50f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.50f, 0.82f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.55f, 0.90f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.54f, 1.00f);
  }

  void Tick() {
    ImGui_ImplRaylib_ProcessEvents();

    ImGui_ImplRaylib_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    if (raylib::Mouse::IsButtonDown(MOUSE_BUTTON_RIGHT))
    {
      raylib::Vector2 const delta = raylib::Mouse::GetDelta() / camera_.GetZoom();
      camera_.SetTarget(raylib::Vector2(camera_.GetTarget()) - delta);
    }

    if (!io.WantCaptureMouse) {
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

    // Toggle TDC panel visibility with Tab key
    if (IsKeyPressed(KEY_TAB)) {
      show_tdc_panel_ = !show_tdc_panel_;
    }

#if 1
    tdc_.Update(ownship_.course, ownship_.GetAimingDevicePosition());
#endif
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

      // Ownship - U-boat silhouette
      constexpr float kOwnshipBeam = 6.21f;
      constexpr float kOwnshipLength = 72.39f;
      constexpr float kMinScreenLength = 80.0f;  // Minimum 80 pixels on screen
      DrawUBoatSilhouette(
        ownship_.position,
        kOwnshipLength,
        kOwnshipBeam,
        ownship_.course,
        Color { 100, 110, 120, 255 },  // Steel gray
        camera_.GetZoom(),
        kMinScreenLength
      );

      EndMode2D();
    }

    constexpr float kTargetBeam = 17.3f;
    constexpr float kTargetLength = 134.0f;

#if 1
    tdc_.DrawVisualization(
      camera_,
      ownship_.position,
      ownship_.GetAimingDevicePosition(),
      ownship_.course,
      kTargetBeam,
      kTargetLength
    );
#endif

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

    // Draw controls hint at top-right
    {
      constexpr int kPadding = 10;
      constexpr int kWidth = 280;
      constexpr int kHeight = 70;
      int x = GetScreenWidth() - kWidth - kPadding;
      int y = kPadding;
      
      DrawRectangle(x, y, kWidth, kHeight, Fade(Color{20, 25, 30, 255}, 0.85f));
      DrawRectangleLines(x, y, kWidth, kHeight, Fade(Color{40, 50, 60, 255}, 0.8f));

      raylib::DrawText("Controls:", x + 10, y + 8, 10, Color{200, 205, 210, 255});
      raylib::DrawText("- Right Click + Drag to move camera", x + 10, y + 26, 10, Color{150, 155, 160, 255});
      raylib::DrawText("- Mouse Wheel to Zoom", x + 10, y + 42, 10, Color{150, 155, 160, 255});
      raylib::DrawText("- Tab to toggle TDC panel", x + 10, y + 58, 10, Color{150, 155, 160, 255});
    }

    {
      ImGui::PushFont(im_font_);

      // Top-left overlay panel (no window decorations)
      DrawOverlayPanel();

      // Bottom TDC panel (fixed, no window decorations)
      if (show_tdc_panel_) {
        DrawTdcBottomPanel();
      }

      ImGui::PopFont();
    }

    ImGui::Render();
    ImGui_ImplRaylib_RenderDrawData(ImGui::GetDrawData());

    EndDrawing();
  }

private:
  void DrawOverlayPanel() {
    ImGuiWindowFlags window_flags = 
      ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove;

    constexpr float kPadding = 10.0f;
    ImGui::SetNextWindowPos(ImVec2(kPadding, kPadding), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    if (ImGui::Begin("##overlay", nullptr, window_flags)) {
      // GitHub button
      if (ImGui::Button("GitHub")) {
        OpenUrl(kGitHubRepoUrl);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("(Source Code)");

      ImGui::Separator();

      // Language selection
      Language current_lang = GetCurrentLanguage();
      if (ImGui::RadioButton("Deutsch", current_lang == Language::kGerman)) {
        SetCurrentLanguage(Language::kGerman);
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("English", current_lang == Language::kEnglish)) {
        SetCurrentLanguage(Language::kEnglish);
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("日本語", current_lang == Language::kJapanese)) {
        SetCurrentLanguage(Language::kJapanese);
      }

      ImGui::Separator();

      // U-Boat section
      ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "U-Boat");
      ownship_.course.ImGuiSliderDegWithId("Course", 0.0f, 359.99f, "%.1f", "%s (deg)", GetText(TextId::kCourse));

#if 0
      {
        float position_x = ownship_.position.x;
        float position_y = ownship_.position.y;
        if (ImGui::InputFloat("Position X (m)", &position_x, 10.0f, 100.0f, "%.1f")) {
          ownship_.position.x = position_x;
        }
        if (ImGui::InputFloat("Position Y (m)", &position_y, 10.0f, 100.0f, "%.1f")) {
          ownship_.position.y = position_y;
        }
      }
#endif

#if 0
      ImGui::Separator();

      // Camera info (collapsible)
      if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Zoom: %.2f", camera_.GetZoom());
        ImGui::Text("Target: (%.1f, %.1f)", camera_.GetTarget().x, camera_.GetTarget().y);
      }
#endif
    }
    ImGui::End();
  }

  void DrawTdcBottomPanel() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Calculate panel dimensions
    // Height needs to accommodate: title (~25px) + spacing + dials (~130px with labels) + sliders/output (~150px) + padding
    float const panel_height = 420.0f;
    float const screen_width = io.DisplaySize.x;
    float const screen_height = io.DisplaySize.y;

    ImGuiWindowFlags window_flags = 
      ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(ImVec2(0, screen_height - panel_height), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(screen_width, panel_height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.4f, 0.5f, 0.6f));

    if (ImGui::Begin("##tdc_panel", nullptr, window_flags)) {
      // Draw a subtle top border line
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 window_pos = ImGui::GetWindowPos();
      draw_list->AddLine(
        ImVec2(window_pos.x, window_pos.y),
        ImVec2(window_pos.x + screen_width, window_pos.y),
        IM_COL32(80, 120, 180, 180),
        2.0f
      );

      // Title with accent color
      ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Torpedovorhaltrechner");
      ImGui::SameLine(screen_width - 120.0f);
      ImGui::TextDisabled("%s", GetText(TextId::kTabToHide));
      
      ImGui::Spacing();

      // Use the TDC's panel rendering
      tdc_.DoPanelImGui(ownship_.course);
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
  }

  ImFont* im_font_ = nullptr;

  raylib::Camera2D camera_;

  Ship ownship_;

  tdc2::Tdc tdc_;

  bool show_tdc_panel_ = true;
};
static State s_state;

void InitializeApp(ImFont* im_font) {
  s_state.InitializeApp(im_font);
}

void UpdateDrawFrame(void) {
  s_state.Tick();

  s_state.Draw();
}
