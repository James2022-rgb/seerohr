// TU header --------------------------------------------
#include "widgets.h"

// c++ headers ------------------------------------------
#include <cstdarg>
#include <cmath>

#include <array>
#include <string>
#include <algorithm>
#include <numbers>

bool SliderFloatWithId(
  char const* str_id,
  float* v,
  float v_min,
  float v_max,
  char const* value_format,
  ImGuiSliderFlags flags,
  char const* label_format,
  ...
) {
  va_list args;
  va_start(args, label_format);
  bool changed = SliderFloatWithIdV(str_id, v, v_min, v_max, value_format, flags, label_format, args);
  va_end(args);
  return changed;
}

bool SliderFloatWithIdV(
  char const* str_id,
  float* v,
  float v_min,
  float v_max,
  char const* value_format,
  ImGuiSliderFlags flags,
  char const* label_format,
  va_list label_args
) {
  std::array<char, 256> buffer { 0 };
  vsnprintf(buffer.data(), buffer.size(), label_format, label_args);
  std::string label = std::string(buffer.data()) + "###" + std::string(str_id);
  return ImGui::SliderFloat(label.c_str(), v, v_min, v_max, value_format, flags);
}

// Code from `imgui.cpp`.
namespace {

int ImFormatString(char* buf, size_t buf_size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
#ifdef IMGUI_USE_STB_SPRINTF
    int w = stbsp_vsnprintf(buf, (int)buf_size, fmt, args);
#else
    int w = vsnprintf(buf, buf_size, fmt, args);
#endif
    va_end(args);
    if (buf == NULL)
        return w;
    if (w == -1 || w >= (int)buf_size)
        w = (int)buf_size - 1;
    buf[w] = 0;
    return w;
}

// Convert UTF-8 to 32-bit character, process single character input.
// A nearly-branchless UTF-8 decoder, based on work of Christopher Wellons (https://github.com/skeeto/branchless-utf8).
// We handle UTF-8 decoding error by skipping forward.
int ImTextCharFromUtf8(unsigned int* out_char, const char* in_text, const char* in_text_end)
{
    static const char lengths[32] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0 };
    static const int masks[]  = { 0x00, 0x7f, 0x1f, 0x0f, 0x07 };
    static const uint32_t mins[] = { 0x400000, 0, 0x80, 0x800, 0x10000 };
    static const int shiftc[] = { 0, 18, 12, 6, 0 };
    static const int shifte[] = { 0, 6, 4, 2, 0 };
    int len = lengths[*(const unsigned char*)in_text >> 3];
    int wanted = len + (len ? 0 : 1);

    if (in_text_end == NULL)
        in_text_end = in_text + wanted; // Max length, nulls will be taken into account.

    // Copy at most 'len' bytes, stop copying at 0 or past in_text_end. Branch predictor does a good job here,
    // so it is fast even with excessive branching.
    unsigned char s[4];
    s[0] = in_text + 0 < in_text_end ? in_text[0] : 0;
    s[1] = in_text + 1 < in_text_end ? in_text[1] : 0;
    s[2] = in_text + 2 < in_text_end ? in_text[2] : 0;
    s[3] = in_text + 3 < in_text_end ? in_text[3] : 0;

    // Assume a four-byte character and load four bytes. Unused bits are shifted out.
    *out_char  = (uint32_t)(s[0] & masks[len]) << 18;
    *out_char |= (uint32_t)(s[1] & 0x3f) << 12;
    *out_char |= (uint32_t)(s[2] & 0x3f) <<  6;
    *out_char |= (uint32_t)(s[3] & 0x3f) <<  0;
    *out_char >>= shiftc[len];

    // Accumulate the various error conditions.
    int e = 0;
    e  = (*out_char < mins[len]) << 6; // non-canonical encoding
    e |= ((*out_char >> 11) == 0x1b) << 7;  // surrogate half?
    e |= (*out_char > IM_UNICODE_CODEPOINT_MAX) << 8;  // out of range we can store in ImWchar (FIXME: May evolve)
    e |= (s[1] & 0xc0) >> 2;
    e |= (s[2] & 0xc0) >> 4;
    e |= (s[3]       ) >> 6;
    e ^= 0x2a; // top two bits of each tail byte correct?
    e >>= shifte[len];

    if (e)
    {
        // No bytes are consumed when *in_text == 0 || in_text == in_text_end.
        // One byte is consumed in case of invalid first byte of in_text.
        // All available bytes (at most `len` bytes) are consumed on incomplete/invalid second to last bytes.
        // Invalid or incomplete input may consume less bytes than wanted, therefore every byte has to be inspected in s.
        wanted = std::min(wanted, !!s[0] + !!s[1] + !!s[2] + !!s[3]);
        *out_char = IM_UNICODE_CODEPOINT_INVALID;
    }

    return wanted;
}

}

namespace {

ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}
ImVec2 operator*(const ImVec2& lhs, float rhs) {
    return ImVec2(lhs.x * rhs, lhs.y * rhs);
}

inline ImVec2 Rot(ImVec2 p, float c, float s) { return ImVec2(p.x * c - p.y * s, p.x * s + p.y * c); }

void AddTextOnArc(ImDrawList* dl, ImFont* font, float font_size,
                         ImVec2 center, float radius,
                         float mid_angle_rad, bool clockwise,
                         ImU32 col, const char* text_utf8,
                         float letter_spacing_px = 0.0f,
                         float radial_offset_px = 0.0f)
{
  if (!text_utf8 || !text_utf8[0] || radius <= 1.0f) return;

  const float scale = font_size / font->LegacySize;

  // Measure total advance in pixels (approx; good enough for dial labels)
  float total = 0.0f;
  {
    const char* p = text_utf8;
    while (*p)
    {
      unsigned int cpt = 0;
      int bytes = ImTextCharFromUtf8(&cpt, p, nullptr);
      if (bytes == 0) break;
      p += bytes;
      const ImFontGlyph* g = font->GetFontBaked(font_size)->FindGlyph((ImWchar)cpt);
      if (!g) continue;
      total += g->AdvanceX * scale + letter_spacing_px;
    }
    total = std::max(0.0f, total - letter_spacing_px);
  }

  const float dir = clockwise ? +1.0f : -1.0f;
  const float start_angle = mid_angle_rad - dir * (total / radius) * 0.5f;

  float pen = 0.0f;
  const char* p = text_utf8;

  while (*p)
  {
    unsigned int cpt = 0;
    int bytes = ImTextCharFromUtf8(&cpt, p, nullptr);
    if (bytes == 0) break;
    p += bytes;

    const ImFontGlyph* g = font->GetFontBaked(font_size)->FindGlyph((ImWchar)cpt);
    if (!g) continue;

    const float adv = g->AdvanceX * scale;

    // Angle where this glyph's baseline origin sits
    const float a = start_angle + dir * (pen / radius);

    ImVec2 rdir(std::cos(a), std::sin(a));
    ImVec2 pos = center + rdir * (radius + radial_offset_px);

    // Baseline direction is tangent to the circle
    const float rot = a + (clockwise ? +std::numbers::pi_v<float> * 0.5f : -std::numbers::pi_v<float> * 0.5f);

    // Draw glyph at current pen position, but we want it placed as if x=0 at this arc point.
    // We can just call the rotated text routine with a substring per glyph? That’s wasteful.
    // Instead, draw single glyph quad inline (same math as AddTextRotated).
    const float c = std::cos(rot), s = std::sin(rot);
    const ImTextureRef tex = font->ContainerAtlas->TexID;

    const float x = 0.0f; // per-glyph baseline origin
    ImVec2 p0((x + g->X0 * scale), (g->Y0 * scale));
    ImVec2 p1((x + g->X1 * scale), (g->Y0 * scale));
    ImVec2 p2((x + g->X1 * scale), (g->Y1 * scale));
    ImVec2 p3((x + g->X0 * scale), (g->Y1 * scale));

    ImVec2 q0 = pos + Rot(p0, c, s);
    ImVec2 q1 = pos + Rot(p1, c, s);
    ImVec2 q2 = pos + Rot(p2, c, s);
    ImVec2 q3 = pos + Rot(p3, c, s);

    ImVec2 uv0(g->U0, g->V0);
    ImVec2 uv1(g->U1, g->V0);
    ImVec2 uv2(g->U1, g->V1);
    ImVec2 uv3(g->U0, g->V1);

    dl->AddImageQuad(tex, q0, q1, q2, q3, uv0, uv1, uv2, uv3, col);

    // Advance along arc
    pen += adv + letter_spacing_px;
  }
}

void PathFillSector(ImDrawList* dl, ImVec2 c, float r, float a0, float a1, ImU32 col, int seg = 48) {
    dl->PathClear();
    dl->PathLineTo(c);
    dl->PathArcTo(c, r, a0, a1, seg);
    dl->PathFillConvex(col);
}

void AddCenteredText(ImDrawList* dl, ImVec2 p, ImU32 col, const char* txt) {
    ImVec2 sz = ImGui::CalcTextSize(txt);
    dl->AddText(ImVec2(p.x - sz.x * 0.5f, p.y - sz.y * 0.5f), col, txt);
}

static void AddCenteredTextEx(ImDrawList* dl, ImFont* font, float font_size,
                             ImVec2 center, ImU32 col, const char* txt)
{
  ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, txt);
  dl->AddText(font, font_size, center - ts * 0.5f, col, txt);
}

static void AddCenteredChar(ImDrawList* dl, ImFont* font, float font_size,
                            ImVec2 center, ImU32 col, char ch)
{
  char buf[2] = { ch, 0 };
  AddCenteredTextEx(dl, font, font_size, center, col, buf);
}

// Draw a “UBOAT-ish” bottom window readout for AoB.
// - Center shows two digits: tens + ones (e.g., 37 => "37", 0 => "00").
// - Left/right show preview tens digits.
// - Small triangle cursor shows fractional position within the current 10° block.
static void DrawAoBBottomWindowReadout(ImDrawList* dl,
                                       ImFont* font,
                                       ImVec2 w0, ImVec2 w1,
                                       float aob_signed_deg,
                                       const AoBDialStyle& st,
                                       bool right_shows_next_tens = true, // set false to get “9 00 9” at 0°
                                       float decade_deg = 10.0f)
{
  const float a = std::fabs(aob_signed_deg);

  // Split into tens/ones inside the 0..99 range (outer ring can communicate hundreds)
  const int deg_i = (int)std::floor(a + 0.5f); // round to nearest degree; use floor(a) if you prefer
  const int ones = deg_i % 10;
  const int tens = (deg_i / 10) % 10;

  const int prev_tens = (tens + 9) % 10;
  const int next_tens = (tens + 1) % 10;

  const bool port = (aob_signed_deg < 0.0f);
  const ImU32 col_center = port ? st.col_port : st.col_stbd;

  // Layout
  const ImVec2 wc((w0.x + w1.x) * 0.5f, (w0.y + w1.y) * 0.5f);
  const float ww = (w1.x - w0.x);
  const float hh = (w1.y - w0.y);

  const float fs_center = hh * 0.95f;
  const float fs_side   = fs_center * 0.80f;

  const ImVec2 pL(w0.x + ww * 0.18f, wc.y + hh * 0.02f);
  const ImVec2 pR(w0.x + ww * 0.82f, wc.y + hh * 0.02f);

  // Center two digits: tens + ones, spaced a bit like a mechanical window
  const float digit_dx = fs_center * 0.38f;
  const ImVec2 pT(wc.x - digit_dx, wc.y + hh * 0.02f);
  const ImVec2 pO(wc.x + digit_dx, wc.y + hh * 0.02f);

  // Clip to window so nothing leaks outside the pill
  dl->PushClipRect(w0, w1, true);

  // Side previews: (prev tens) and (next tens) by default.
  // If you want the exact “9 00 9” look at 0°, set right_shows_next_tens=false.
  AddCenteredChar(dl, font, fs_side, pL, st.col_port,  (char)('0' + prev_tens));
  AddCenteredChar(dl, font, fs_side, pR, st.col_stbd,  (char)('0' + (right_shows_next_tens ? next_tens : prev_tens)));

  // Center digits
  AddCenteredChar(dl, font, fs_center, pT, col_center, (char)('0' + tens));
  AddCenteredChar(dl, font, fs_center, pO, col_center, (char)('0' + ones));

  // “Where you are in the first digit” cursor:
  // fraction inside the current decade (10° by default). If you use decade_deg=20, it becomes “within 20° block”.
  const float decade = std::max(1e-3f, decade_deg);
  const float frac_in_decade = std::fmod(a, decade) / decade; // 0..1
  const float pad = ww * 0.08f;
  const float cx = (w0.x + pad) + frac_in_decade * (ww - 2.0f * pad);

  const float tri_h = hh * 0.28f;
  const float tri_w = tri_h * 0.65f;

  const ImVec2 t0(cx, w0.y + hh * 0.05f);
  const ImVec2 t1(cx - tri_w, w0.y + hh * 0.05f + tri_h);
  const ImVec2 t2(cx + tri_w, w0.y + hh * 0.05f + tri_h);

  // Shadow + triangle
  dl->AddTriangleFilled(t0 + ImVec2(1,1), t1 + ImVec2(1,1), t2 + ImVec2(1,1), IM_COL32(0,0,0,120));
  dl->AddTriangleFilled(t0, t1, t2, IM_COL32(240,240,240,180));

  dl->PopClipRect();
}

}

// signed AoB: negative=links (port), positive=stb (starboard). range [-180,+180]
bool AoBDialProcedural(
  char const* id,
  char const* label,
  float radius,
  float* aob_signed_deg,
  AoBDialStyle const& st,
  ImFont* label_font,
  float label_font_size
) {
  ImGuiIO& io = ImGui::GetIO();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  if (!label_font) label_font = ImGui::GetFont();
  if (label_font_size <= 0.0f) label_font_size = ImGui::GetFontSize();

  ImVec2 pos = ImGui::GetCursorScreenPos();
  const float gap = radius * st.label_gap;
  const float lab_h = radius * st.label_h;

  ImVec2 size(radius * 2.0f, radius * 2.0f + gap + lab_h);
  ImGui::InvisibleButton(id, size);

  const ImVec2 c(pos.x + radius, pos.y + radius);

  // ---- interaction: drag sets AoB
  bool changed = false;
  if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    ImVec2 m = io.MousePos;
    ImVec2 d = m - c;

    if (d.x * d.x + d.y * d.y <= radius * radius) {
      ImVec2 v(m.x - c.x, m.y - c.y);
      float ang = std::atan2(v.y, v.x);            // 0 right, +90 down
      float rel = ang - (-std::numbers::pi_v<float> *0.5f);           // 0 at top
      while (rel <= -std::numbers::pi_v<float>) rel += 2.0f * std::numbers::pi_v<float>;
      while (rel > std::numbers::pi_v<float>) rel -= 2.0f * std::numbers::pi_v<float>;   // (-pi, pi]
      float deg = rel / std::numbers::pi_v<float> *180.0f;            // -180..+180
      deg = std::clamp(deg, -180.0f, 180.0f);
      if (deg != *aob_signed_deg) { *aob_signed_deg = deg; changed = true; }
    }
  }

  // ---- radii
  const float r_outer = radius;
  const float r_bez_o = radius * 0.98f;
  const float r_bez_i = radius * (0.98f - st.bezel_thick);
  const float r_ticks = radius * st.tick_outer_r;
  const float r_color = radius * 0.62f;

  // ---- face + bezel (a simple bevel illusion)
  dl->AddCircleFilled(c, r_outer, st.col_bezel_in, 128);
  dl->AddCircleFilled(c, r_bez_o, st.col_bezel_out, 128);
  dl->AddCircleFilled(c, r_bez_i, st.col_face, 128);

  // ---- colored halves (right=stb, left=links)
  // Right half: -90..+90
  PathFillSector(dl, c, r_color, -std::numbers::pi_v<float>*0.5f, +std::numbers::pi_v<float>*0.5f, st.col_stbd);
  // Left half: +90..+270
  PathFillSector(dl, c, r_color, +std::numbers::pi_v<float>*0.5f, +std::numbers::pi_v<float>*1.5f, st.col_port);

  constexpr float kCutoutSizeRad = 90.0f * (std::numbers::pi_v<float> / 180.0f);
  const float bottom = std::numbers::pi_v<float> *0.5f;
  PathFillSector(
    dl, c, r_color * 1.02f,
    bottom - kCutoutSizeRad * 0.5f, bottom + kCutoutSizeRad * 0.5f,
    st.col_face,
    24
  );

  // ---- bottom window (pill), drawn *on top* of the red/green field
  {
    float ww = r_color * 1.05f;         // window width
    float hh = r_color * 0.20f;         // window height
    ImVec2 wc = ImVec2(c.x, c.y + r_color * 0.40f); // window center

    ImVec2 w0(wc.x - ww * 0.5f, wc.y - hh * 0.5f);
    ImVec2 w1(wc.x + ww * 0.5f, wc.y + hh * 0.5f);

    // Slight shadow so it feels inset
    dl->AddRectFilled(w0 + ImVec2(2,2), w1 + ImVec2(2,2), IM_COL32(0,0,0,120), hh * 0.5f);

    // Window body (black)
    dl->AddRectFilled(w0, w1, IM_COL32(0, 0, 0, 255), hh * 0.5f);

    // Readout
    DrawAoBBottomWindowReadout(dl, label_font, w0, w1, *aob_signed_deg, st,
      /*right_shows_next_tens=*/true,  // set false to get “9 00 9” at 0°
      /*decade_deg=*/10.0f);           // or 20.0f if you want “within the 20° block”
  }

  // ---- ticks + mirrored numbers
  for (int deg = 0; deg <= 180; deg += 5)
  {
    const bool major = (deg % 20) == 0;
    const bool mid   = (deg % 10) == 0;

    const float t = (deg / 180.0f) * std::numbers::pi_v<float>; // 0..pi
    for (int side = -1; side <= 1; side += 2)
    {
      const float a = -std::numbers::pi_v<float>*0.5f + side * t;

      const float in_len = major ? st.tick_major_in : (mid ? st.tick_mid_in : st.tick_min_in);
      const float thick  = major ? radius*0.020f : radius*0.014f;

      ImVec2 p0(c.x + std::cos(a) * r_ticks,              c.y + std::sin(a) * r_ticks);
      ImVec2 p1(c.x + std::cos(a) * (r_ticks - radius*in_len),
                c.y + std::sin(a) * (r_ticks - radius*in_len));

      dl->AddLine(p0, p1, st.col_tick, thick);

      if (major && deg != 0)
      {
        char buf[8];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "%d", deg);

        // Keep numbers upright (classic instrument readability)
        ImVec2 pt(c.x + std::cos(a) * (radius * 0.78f),
                  c.y + std::sin(a) * (radius * 0.78f));
        AddCenteredText(dl, pt, st.col_text, buf);
      }
    }
  }

  // ---- arc labels “Bug links / Bug rechts”
  // Place them roughly where the UBOAT dial has them. Tweak angles/radius to match your reference.
  AddTextOnArc(dl, label_font, label_font_size,
                c, radius * 0.74f,
    std::numbers::pi_v<float> * 1.07f, true,  st.col_text, "Bug links",
                1.0f, -label_font_size * 0.1f);

  AddTextOnArc(dl, label_font, label_font_size,
                c, radius * 0.74f,
    std::numbers::pi_v<float> * -0.07f, true, st.col_text, "Bug rechts",
                1.0f, -label_font_size * 0.1f);

  // ---- needle
  float aob = std::clamp(*aob_signed_deg, -180.0f, 180.0f);
  float tt  = (std::abs(aob) / 180.0f) * std::numbers::pi_v<float>;
  float ang = -std::numbers::pi_v<float>*0.5f + (aob >= 0.0f ? +tt : -tt);

  ImVec2 dir(std::cos(ang), std::sin(ang));
  ImVec2 nrm(-dir.y, dir.x);

  ImVec2 tip  = c + dir * (radius * st.needle_len);
  ImVec2 base = c - dir * (radius * 0.10f);

  float w = radius * st.needle_w;

  // shadow
  ImVec2 sh(1.5f, 1.5f);
  dl->AddTriangleFilled(base + nrm*w + sh, base - nrm*w + sh, tip + sh, st.col_shadow);

  // needle body
  dl->AddTriangleFilled(base + nrm*w, base - nrm*w, tip, st.col_needle);

  // center cap
  dl->AddCircleFilled(c, radius * 0.10f, IM_COL32(20,20,20,255), 48);
  dl->AddCircle(c, radius * 0.10f, IM_COL32(255,255,255,70), 48, radius * 0.01f);

  // “glass” highlight
  dl->PathClear();
  dl->PathArcTo(c, radius * 0.86f, -2.5f, -1.4f, 24);
  dl->PathStroke(IM_COL32(255,255,255,40), 0, radius * 0.03f);

  if (label && label[0])
  {
    const float gap   = radius * st.label_gap;
    const float lab_h = radius * st.label_h;

    const float plate_pad_x = radius * 0.10f;
    const float plate_w     = radius * 2.0f - plate_pad_x * 2.0f;

    ImVec2 plate0(pos.x + plate_pad_x, pos.y + radius * 2.0f + gap);
    ImVec2 plate1(plate0.x + plate_w,  plate0.y + lab_h);

    const float rounding = radius * st.label_rounding;
    const float border_th = std::max(1.0f, radius * st.label_border_th);

    // Shadow
    dl->AddRectFilled(plate0 + ImVec2(2,2), plate1 + ImVec2(2,2), IM_COL32(0,0,0,120), rounding);

    // Plate body
    dl->AddRectFilled(plate0, plate1, st.col_plate_fill, rounding);

    // Border
    dl->AddRect(plate0, plate1, st.col_plate_border, rounding, 0, border_th);

    // Optional "screw heads" (purely for vibes)
    if (st.label_screws)
    {
      const float sr = radius * st.screw_r;
      const float inset = sr * 1.6f;

      ImVec2 s0(plate0.x + inset, plate0.y + inset);
      ImVec2 s1(plate1.x - inset, plate0.y + inset);
      ImVec2 s2(plate0.x + inset, plate1.y - inset);
      ImVec2 s3(plate1.x - inset, plate1.y - inset);

      auto Screw = [&](ImVec2 sc)
      {
        dl->AddCircleFilled(sc, sr, IM_COL32(180,180,180,200), 24);
        dl->AddCircle(sc, sr, IM_COL32(0,0,0,160), 24, std::max(1.0f, sr * 0.20f));
        // little slot
        dl->AddLine(sc - ImVec2(sr*0.6f, 0), sc + ImVec2(sr*0.6f, 0), IM_COL32(0,0,0,160), std::max(1.0f, sr * 0.25f));
      };

      Screw(s0); Screw(s1); Screw(s2); Screw(s3);
    }

    // Text centered on plate
    ImVec2 tc((plate0.x + plate1.x) * 0.5f, (plate0.y + plate1.y) * 0.5f);
    AddCenteredTextEx(dl, label_font, label_font_size, tc, st.col_plate_text, label);
  }

  // Layout advance
  ImGui::Dummy(size);
  return changed;
}
