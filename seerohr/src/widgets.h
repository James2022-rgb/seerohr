#pragma once

// external headers -------------------------------------
#include "imgui.h"

bool SliderFloatWithId(
  char const* str_id,
  float* v,
  float v_min,
  float v_max,
  char const* value_format,
  ImGuiSliderFlags flags,
  char const* label_format,
  ...
);

bool SliderFloatWithIdV(
  char const* str_id,
  float* v,
  float v_min,
  float v_max,
  char const* value_format,
  ImGuiSliderFlags flags,
  char const* label_format,
  va_list args
);

struct AoBDialStyle
{
  ImU32 col_face      = IM_COL32(10,10,10,255);
  ImU32 col_bezel_out = IM_COL32(220,220,220,255);
  ImU32 col_bezel_in  = IM_COL32(60,60,60,255);

  ImU32 col_tick      = IM_COL32(245,245,245,255);
  ImU32 col_text      = IM_COL32(245,245,245,255);

  ImU32 col_port      = IM_COL32(170, 35, 35, 255); // links
  ImU32 col_stbd      = IM_COL32( 40,130, 55, 255); // stb

  ImU32 col_needle    = IM_COL32(220,200,150,255);
  ImU32 col_shadow    = IM_COL32(0,0,0,120);

  float bezel_thick   = 0.06f;   // of radius
  float tick_outer_r  = 0.92f;   // of radius
  float tick_major_in = 0.12f;   // of radius
  float tick_mid_in   = 0.08f;
  float tick_min_in   = 0.05f;

  float needle_len    = 0.68f;
  float needle_w      = 0.04f;

  // Label plate (e.g. "Gegnerlage")
  ImU32 col_plate_fill = IM_COL32(10, 10, 10, 255);
  ImU32 col_plate_border = IM_COL32(240, 240, 240, 180);
  ImU32 col_plate_text = IM_COL32(240, 240, 240, 220);

  float label_gap = 0.10f; // of radius
  float label_h = 0.38f; // of radius
  float label_rounding = 0.06f; // of radius
  float label_border_th = 0.010f; // of radius

  // Optional screw heads on the plate
  bool  label_screws = true;
  float screw_r = 0.030f; // of radius
};

bool AoBDialProcedural(
  char const* id,
  char const* label,
  float radius,
  float* aob_signed_deg,
  AoBDialStyle const& st = AoBDialStyle(),
  ImFont* label_font = nullptr,
  float label_font_size = 0.0f
);

bool BearingDialStacked_UBOAT(
  const char* id,
  const char* bottom_label,      // e.g. "Zielrichtung" / "Schiffspeilung"
  float r_bot,                   // radius of bottom (coarse) dial
  float* bearing_deg_io,         // 0..360
  const AoBDialStyle& st = AoBDialStyle(),
  ImFont* font = nullptr,
  float font_size = 0.0f
);
