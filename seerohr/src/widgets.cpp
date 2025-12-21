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

void AddCenteredTextEx(ImDrawList* dl, ImFont* font, float font_size,
                             ImVec2 center, ImU32 col, const char* txt)
{
  ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, txt);
  dl->AddText(font, font_size, center - ts * 0.5f, col, txt);
}

void DrawScrewHead(ImDrawList* dl, ImVec2 c, float r)
{
  // Head
  dl->AddCircleFilled(c, r, IM_COL32(190,190,190,220), 24);
  dl->AddCircle(c, r, IM_COL32(0,0,0,160), 24, std::max(1.0f, r * 0.22f));
  // Slot
  dl->AddLine(c - ImVec2(r*0.65f, 0.0f), c + ImVec2(r*0.65f, 0.0f), IM_COL32(0,0,0,170), std::max(1.0f, r * 0.25f));
  // Tiny highlight
  dl->AddCircleFilled(c - ImVec2(r*0.25f, r*0.25f), r*0.18f, IM_COL32(255,255,255,120), 12);
}

void DrawNamePlate(ImDrawList* dl, ImFont* font, float font_size,
                          ImVec2 p0, ImVec2 p1, const char* label,
                          bool screws = true,
                          ImU32 col_fill   = IM_COL32(10,10,10,255),
                          ImU32 col_border = IM_COL32(240,240,240,170),
                          ImU32 col_text   = IM_COL32(240,240,240,220),
                          float rounding = 10.0f,
                          float border_th = 1.0f)
{
  // Shadow
  dl->AddRectFilled(p0 + ImVec2(2,2), p1 + ImVec2(2,2), IM_COL32(0,0,0,120), rounding);

  // Body
  dl->AddRectFilled(p0, p1, col_fill, rounding);

  // Border
  dl->AddRect(p0, p1, col_border, rounding, 0, border_th);

  // Screws
  if (screws)
  {
    const float h = (p1.y - p0.y);
    const float sr = h * 0.22f;
    const float inset_x = sr * 1.8f;
    const float inset_y = sr * 1.6f;

    ImVec2 sL(p0.x + inset_x, p0.y + inset_y);
    ImVec2 sR(p1.x - inset_x, p0.y + inset_y);
    DrawScrewHead(dl, sL, sr);
    DrawScrewHead(dl, sR, sr);
  }

  // Text
  if (label && label[0])
  {
    ImVec2 tc((p0.x+p1.x)*0.5f, (p0.y+p1.y)*0.5f);
    AddCenteredTextEx(dl, font, font_size, tc, col_text, label);
  }
}

}

namespace {

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

  const bool has_label = (label && label[0]);
  const float gap   = has_label ? radius * st.label_gap : 0.0f;
  const float lab_h = has_label ? radius * st.label_h   : 0.0f;

  ImVec2 size(radius * 2.0f, radius * 2.0f + gap + lab_h);
  ImGui::InvisibleButton(id, size);

  const ImVec2 c(pos.x + radius, pos.y + radius);

  // ---- interaction: drag sets AoB (recommended: only when clicking inside the dial circle)
  bool changed = false;
  if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
  {
    ImVec2 m = io.MousePos;
    ImVec2 d = ImVec2(m.x - c.x, m.y - c.y);

    const float r2 = radius * radius;
    const float d2 = d.x*d.x + d.y*d.y;

    // Only allow interaction if mouse is within the dial circle (ignores the nameplate area)
    if (d2 <= r2)
    {
      float ang = std::atan2(d.y, d.x); // 0 right, +90 down (screen coords)
      float rel = ang - (-std::numbers::pi_v<float> * 0.5f); // 0 at top
      while (rel <= -std::numbers::pi_v<float>) rel += 2.0f * std::numbers::pi_v<float>;
      while (rel >  std::numbers::pi_v<float>)  rel -= 2.0f * std::numbers::pi_v<float>;
      float deg = rel / std::numbers::pi_v<float> * 180.0f;
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

  // ---- draw nameplate using helper (bottom)
  if (has_label)
  {
    const float plate_pad_x = radius * 0.10f;
    ImVec2 p0(pos.x + plate_pad_x, pos.y + radius * 2.0f + gap);
    ImVec2 p1(pos.x + radius * 2.0f - plate_pad_x, p0.y + lab_h);

    const float rounding = radius * st.label_rounding;
    const float border_th = std::max(1.0f, radius * st.label_border_th);

    DrawNamePlate(dl, label_font, label_font_size,
      p0, p1, label,
      st.label_screws,
      st.col_plate_fill,
      st.col_plate_border,
      st.col_plate_text,
      rounding,
      border_th);
  }

  // Layout advance
  ImGui::Dummy(size);
  return changed;
}

namespace {

float Wrap360(float deg) {
  deg = std::fmod(deg, 360.0f);
  if (deg < 0.0f) deg += 360.0f;
  return deg;
}

// 0° at top, 90° right, 180° bottom, 270° left
float BearingFromMouse(ImVec2 c, ImVec2 m) {
  float ang = std::atan2(m.y - c.y, m.x - c.x);              // 0 at +X, CCW (screen Y-down)
  float deg = (ang + std::numbers::pi_v<float> * 0.5f) * (180.0f / std::numbers::pi_v<float>);
  return Wrap360(deg);
}

float AngleFromBearing(float deg) {
  return -std::numbers::pi_v<float> * 0.5f + (deg * (std::numbers::pi_v<float> / 180.0f));
}

static void DrawBezel(ImDrawList* dl, ImVec2 c, float r,
                      ImU32 col_outer, ImU32 col_inner, ImU32 col_face)
{
  dl->AddCircleFilled(c, r, col_inner, 128);
  dl->AddCircleFilled(c, r * 0.98f, col_outer, 128);
  dl->AddCircleFilled(c, r * 0.90f, col_face, 128);
}

// tick: deg in bearing degrees (0..360)
void DrawTick(ImDrawList* dl, ImVec2 c, float r_outer, float len, float thick, float deg, ImU32 col)
{
  float a = AngleFromBearing(deg);
  ImVec2 p0(c.x + std::cos(a) * r_outer,        c.y + std::sin(a) * r_outer);
  ImVec2 p1(c.x + std::cos(a) * (r_outer-len),  c.y + std::sin(a) * (r_outer-len));
  dl->AddLine(p0, p1, col, thick);
}

// Top dial: full circle = 10°, minor = 0.1° (100 ticks), major = 1° (10 ticks, labeled 0..9).
void DrawFine10DegFace(ImDrawList* dl, ImVec2 c, float r,
                       ImFont* font, float font_size,
                       ImU32 col_tick, ImU32 col_text
) {
  const float r_ticks = r * 0.88f;
  const float r_text  = r * 0.62f;

  // 100 minor ticks (0.1° each over a 10° revolution)
  for (int i = 0; i < 100; ++i)
  {
    const bool major = (i % 10) == 0; // 1.0°
    const bool mid   = (i % 5)  == 0; // 0.5°

    const float len   = major ? r * 0.18f : (mid ? r * 0.12f : r * 0.08f);
    const float thick = major ? r * 0.030f : (mid ? r * 0.022f : r * 0.016f);

    // i ranges 0..99 => angle fraction i/100 around the dial.
    const float deg = (i / 100.0f) * 360.0f;
    DrawTick(dl, c, r_ticks, len, thick, deg, col_tick);

    if (major)
    {
      // label 0..9 at each 1.0° (every 36° on the dial)
      const int digit = (i / 10); // 0..9
      char buf[2] = { (char)('0' + digit), 0 };

      const float a = AngleFromBearing(deg);
      ImVec2 pt(c.x + std::cos(a) * r_text, c.y + std::sin(a) * r_text);
      AddCenteredTextEx(dl, font, font_size, pt, col_text, buf);
    }
  }
}

static float PseudoBearingForFine10(float bearing_deg)
{
  float b = Wrap360(bearing_deg);
  float fine = std::fmod(b, 10.0f);        // 0..10
  if (fine < 0.0f) fine += 10.0f;
  return fine * 36.0f;                     // 0..360 "pseudo-bearing" for the 10° dial
}

static void DrawNeedleSimple(ImDrawList* dl, ImVec2 c, float r, float bearing_deg,
                             ImU32 col_needle, ImU32 col_shadow
) {
  float ang = AngleFromBearing(bearing_deg);
  ImVec2 dir(std::cos(ang), std::sin(ang));
  ImVec2 nrm(-dir.y, dir.x);

  ImVec2 tip  = c + dir * (r * 0.78f);
  ImVec2 base = c - dir * (r * 0.10f);

  float w = r * 0.05f;

  // shadow
  ImVec2 sh(1.5f, 1.5f);
  dl->AddTriangleFilled(base + nrm*w + sh, base - nrm*w + sh, tip + sh, col_shadow);

  // body
  dl->AddTriangleFilled(base + nrm*w, base - nrm*w, tip, col_needle);

  // center hub
  dl->AddCircleFilled(c, r * 0.10f, IM_COL32(20,20,20,255), 48);
  dl->AddCircle(c, r * 0.10f, IM_COL32(255,255,255,70), 48, std::max(1.0f, r * 0.01f));

  // optional “fork” at the tip (like UBOAT’s red prongs)
  {
    ImVec2 f0 = tip - dir * (r * 0.05f);
    float fw = r * 0.04f;
    ImVec2 l = f0 + nrm * fw;
    ImVec2 rr = f0 - nrm * fw;
    ImVec2 ft = tip + dir * (r * 0.02f);
    dl->AddTriangleFilled(l, rr, ft, IM_COL32(150, 60, 60, 255));
  }
}

// ===== faces =====

// Bottom dial: 0..360, label every 30°, major ticks every 10°, minor every 2°
static void DrawFineBearingFace(ImDrawList* dl, ImVec2 c, float r,
                                ImFont* font, float font_size,
                                ImU32 col_tick, ImU32 col_text
) {
  float r_ticks = r * 0.88f;
  float r_text  = r * 0.62f;

  for (int d = 0; d < 360; d += 2)
  {
    bool major10 = (d % 10) == 0;
    bool label30 = (d % 30) == 0;

    float len   = major10 ? r * 0.16f : r * 0.08f;
    float thick = major10 ? r * 0.028f : r * 0.018f;

    DrawTick(dl, c, r_ticks, len, thick, (float)d, col_tick);

    if (label30)
    {
      char buf[8];
      ImFormatString(buf, IM_ARRAYSIZE(buf), "%d", d);
      float a = AngleFromBearing((float)d);
      ImVec2 pt(c.x + std::cos(a) * r_text, c.y + std::sin(a) * r_text);
      AddCenteredTextEx(dl, font, font_size, pt, col_text, buf);
    }
  }
}

// Top dial: 0..9 around the circle (coarse sectors). Same needle angle as bearing.
static void DrawCoarseBearingFace_0to9(ImDrawList* dl, ImVec2 c, float r,
                                      ImFont* font, float font_size,
                                      ImU32 col_tick, ImU32 col_text
) {
  float r_ticks = r * 0.88f;
  float r_text  = r * 0.60f;

  // Many fine ticks, but only 10 big sectors.
  // Make “big” ticks at every 36°, “mid” at 18°, “minor” at 6°.
  for (int d = 0; d < 360; d += 6)
  {
    bool big = (d % 36) == 0;
    bool mid = (d % 18) == 0;

    float len   = big ? r * 0.18f : (mid ? r * 0.13f : r * 0.08f);
    float thick = big ? r * 0.030f : (mid ? r * 0.024f : r * 0.016f);

    DrawTick(dl, c, r_ticks, len, thick, (float)d, col_tick);
  }

  // Digits 0..9 at 36° steps
  for (int k = 0; k < 10; ++k)
  {
    float deg = k * 36.0f;
    float a = AngleFromBearing(deg);
    ImVec2 pt(c.x + std::cos(a) * r_text, c.y + std::sin(a) * r_text);

    char buf[2] = { (char)('0' + k), 0 };
    AddCenteredTextEx(dl, font, font_size, pt, col_text, buf);
  }

  // little inner dots (pure vibes, matches screenshot a bit)
  for (int k = 0; k < 10; ++k)
  {
    float deg = k * 36.0f + 18.0f;
    float a = AngleFromBearing(deg);
    ImVec2 pt(c.x + std::cos(a) * (r * 0.42f), c.y + std::sin(a) * (r * 0.42f));
    dl->AddCircleFilled(pt, r * 0.015f, IM_COL32(240,240,240,120), 12);
  }
}

}

bool BearingDialStacked(
  const char* id,
  const char* bottom_label,
  float r_bot,
  float* bearing_deg_io,
  const AoBDialStyle& st,
  ImFont* font,
  float font_size)
{
  if (!font) font = ImGui::GetFont();
  if (font_size <= 0.0f) font_size = ImGui::GetFontSize();

  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImGuiIO& io = ImGui::GetIO();

  const float r_top = r_bot * 0.72f;
  const float gap_y = r_bot * 0.18f;
  const float pad = r_bot * 0.12f;

  const float plate_gap = r_bot * 0.10f;
  const float plate_h = r_bot * 0.38f;

  const float panel_w = r_bot * 2.0f + pad * 2.0f;
  const float panel_h = pad + (r_top * 2.0f) + gap_y + (r_bot * 2.0f) + plate_gap + plate_h + pad;

  ImVec2 pos = ImGui::GetCursorScreenPos();
  ImVec2 size(panel_w, panel_h);

  ImGui::PushID(id);

#if 0
  // Backing panel (black module plate)
  dl->AddRectFilled(pos, pos + size, IM_COL32(0, 0, 0, 255), r_bot * 0.10f);
#endif

  // Dial centers
  ImVec2 c_top(pos.x + panel_w * 0.5f, pos.y + pad + r_top);
  ImVec2 c_bot(pos.x + panel_w * 0.5f, c_top.y + r_top + gap_y + r_bot);

  // Interaction: drag either dial
  bool changed = false;
  float bearing = Wrap360(*bearing_deg_io);

  // Coarse dial hitbox (bottom)
  ImGui::SetCursorScreenPos(c_bot - ImVec2(r_bot, r_bot));
  ImGui::InvisibleButton("coarse360", ImVec2(r_bot * 2, r_bot * 2));
  if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
  {
    bearing = BearingFromMouse(c_bot, io.MousePos);
    changed = true;
  }

  // Fine dial hitbox (top): adjusts only within current 10° decade
  ImGui::SetCursorScreenPos(c_top - ImVec2(r_top, r_top));
  ImGui::InvisibleButton("fine10", ImVec2(r_top * 2, r_top * 2));
  if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
  {
    float pseudo = BearingFromMouse(c_top, io.MousePos); // 0..360
    float fine10 = pseudo / 36.0f;                       // 0..10
    float decade_base = std::floor(bearing / 10.0f) * 10.0f;
    bearing = Wrap360(decade_base + fine10);
    changed = true;
  }

  // Render
  const ImU32 col_tick = IM_COL32(230, 220, 200, 255);
  const ImU32 col_text = IM_COL32(240, 240, 240, 220);

  // Top (fine 10°)
  DrawBezel(dl, c_top, r_top, st.col_bezel_out, st.col_bezel_in, st.col_face);
  DrawFine10DegFace(dl, c_top, r_top * 0.92f, font, font_size * 0.90f, col_tick, col_text);
  DrawNeedleSimple(dl, c_top, r_top, PseudoBearingForFine10(bearing), st.col_needle, st.col_shadow);

  // Bottom (coarse 360°)
  DrawBezel(dl, c_bot, r_bot, st.col_bezel_out, st.col_bezel_in, st.col_face);
  DrawFineBearingFace(dl, c_bot, r_bot * 0.92f, font, font_size * 0.90f, col_tick, col_text);
  DrawNeedleSimple(dl, c_bot, r_bot, bearing, st.col_needle, st.col_shadow);

  // Bottom nameplate
  if (bottom_label && bottom_label[0])
  {
    const float plate_pad_x = r_bot * 0.18f;
    ImVec2 p0(pos.x + plate_pad_x, pos.y + panel_h - pad - plate_h);
    ImVec2 p1(pos.x + panel_w - plate_pad_x, p0.y + plate_h);

    float rounding = r_bot * 0.08f;
    float border_th = std::max(1.0f, r_bot * 0.012f);
    DrawNamePlate(dl, font, font_size,
      p0, p1, bottom_label,
      /*screws=*/true,
      IM_COL32(10, 10, 10, 255),
      IM_COL32(240, 240, 240, 170),
      IM_COL32(240, 240, 240, 220),
      rounding, border_th);
  }

  // Layout consume
  ImGui::SetCursorScreenPos(pos);
  ImGui::Dummy(size);

  ImGui::PopID();

  if (changed) *bearing_deg_io = Wrap360(bearing);
  return changed;
}

namespace {

float AngleFromBearingDeg(float bearing_deg) {
  // returns screen angle radians for drawing (0 at top)
  return -std::numbers::pi_v<float> * 0.5f + (bearing_deg * (std::numbers::pi_v<float> / 180.0f));
}

float WrapPi(float a) {
  while (a <= -std::numbers::pi_v<float>) a += 2.0f * std::numbers::pi_v<float>;
  while (a >  +std::numbers::pi_v<float>) a -= 2.0f * std::numbers::pi_v<float>;
  return a;
}

void DrawBezel3(ImDrawList* dl, ImVec2 c, float r, ImU32 outer, ImU32 mid, ImU32 face) {
  dl->AddCircleFilled(c, r, mid, 128);
  dl->AddCircleFilled(c, r * 0.98f, outer, 128);
  dl->AddCircleFilled(c, r * 0.90f, face, 128);
}

void DrawNeedle(ImDrawList* dl, ImVec2 c, float r, float ang, ImU32 col, ImU32 shadow) {
  ImVec2 dir(std::cos(ang), std::sin(ang));
  ImVec2 nrm(-dir.y, dir.x);

  ImVec2 tip  = c + dir * (r * 0.78f);
  ImVec2 base = c - dir * (r * 0.10f);

  float w = r * 0.05f;

  ImVec2 sh(1.5f, 1.5f);
  dl->AddTriangleFilled(base + nrm*w + sh, base - nrm*w + sh, tip + sh, shadow);
  dl->AddTriangleFilled(base + nrm*w, base - nrm*w, tip, col);

  dl->AddCircleFilled(c, r * 0.10f, IM_COL32(20,20,20,255), 48);
  dl->AddCircle(c, r * 0.10f, IM_COL32(255,255,255,70), 48, std::max(1.0f, r * 0.01f));
}

void DrawDownTriangle(ImDrawList* dl, ImVec2 tip, float w, float h, ImU32 col) {
  dl->AddTriangleFilled(
    tip,
    ImVec2(tip.x - w, tip.y - h),
    ImVec2(tip.x + w, tip.y - h),
    col
  );
}


}

bool TorpGeschwUndGegnerfahrtDial(
  const char* id,
  const char* label,
  float radius,
  float* target_knots_io,
  float* torp_knots_io,
  const AoBDialStyle& st,
  ImFont* font,
  float font_size
) {
  if (!font) font = ImGui::GetFont();
  if (font_size <= 0.0f) font_size = ImGui::GetFontSize();

  ImGuiIO& io = ImGui::GetIO();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // --- ranges ---
  constexpr float kTargetMin = 0.0f;
  constexpr float kTargetMax = 55.0f; // outer needle settable
  constexpr float kTorpMin   = 0.0f;
  constexpr float kTorpMax   = 60.0f; // inner drum settable (60 is “real”, wraps to 0 angle)

  // --- widget sizing (dial + bottom nameplate) ---
  const bool has_label = (label && label[0]);
  const float gap   = has_label ? radius * st.label_gap : 0.0f;
  const float lab_h = has_label ? radius * st.label_h   : 0.0f;

  ImVec2 pos  = ImGui::GetCursorScreenPos();
  ImVec2 size(radius * 2.0f, radius * 2.0f + gap + lab_h);

  ImGui::PushID(id);
  ImGui::InvisibleButton("##dial", size);

  const ImVec2 c(pos.x + radius, pos.y + radius);

  // Colors (swap to st.col_tick/st.col_text if you have them)
  const ImU32 col_tick = IM_COL32(235, 225, 205, 255); // warm ivory
  const ImU32 col_text = IM_COL32(240, 240, 240, 220);

  // --- geometry: outer scale ---
  // Outer is effectively 0..60 mapped onto full circle; labels stop at 55.
  auto SpeedToBearingDeg_0to60 = [](float v) -> float {
    // 0..60 -> 0..360 (60 == 0)
    float t = (v / 60.0f) * 360.0f;
    return Wrap360(t);
  };

  // --- geometry: inner arc window band (upper half) ---
  const float marker_bearing = 0.0f;                        // 12 o'clock
  const float marker_ang     = AngleFromBearingDeg(marker_bearing);

  const float win_ang0 = marker_ang - std::numbers::pi_v<float> * 0.5f; // left
  const float win_ang1 = marker_ang + std::numbers::pi_v<float> * 0.5f; // right

  const float r_win_mid = radius * 0.48f;
  const float win_band_w = radius * 0.18f;
  const float r_win_out = r_win_mid + win_band_w * 0.5f;
  const float r_win_in  = r_win_mid - win_band_w * 0.5f;

  // Hit test: inside band + within upper half angles
  auto MouseInWindowBand = [&]() -> bool {
    ImVec2 d(io.MousePos.x - c.x, io.MousePos.y - c.y);
    float rr = std::sqrt(d.x*d.x + d.y*d.y);
    if (rr < r_win_in || rr > r_win_out) return false;

    float ang = std::atan2(d.y, d.x);
    // Convert to our “0 at top” drawing angle space: same as AngleFromBearingDeg
    // We want relative to marker_ang in (-pi..pi]
    float rel = WrapPi(ang - marker_ang);
    return std::fabs(rel) <= std::numbers::pi_v<float> * 0.5f;
  };

  // --- interaction state (for turning the torp drum like a knob) ---
  ImGuiStorage* store = ImGui::GetStateStorage();
  ImGuiID key_ang0 = ImGui::GetID("torp_drag_ang0");
  ImGuiID key_val0 = ImGui::GetID("torp_drag_val0");

  bool changed = false;
  bool in_window = MouseInWindowBand();

  // Clamp inputs
  *target_knots_io = std::clamp(*target_knots_io, kTargetMin, kTargetMax);
  *torp_knots_io   = std::clamp(*torp_knots_io,   kTorpMin,   kTorpMax);

  if (ImGui::IsItemActivated() && in_window)
  {
    // Start “turn drum” gesture
    ImVec2 d(io.MousePos.x - c.x, io.MousePos.y - c.y);
    float ang = std::atan2(d.y, d.x);
    store->SetFloat(key_ang0, ang);
    store->SetFloat(key_val0, *torp_knots_io);
  }

  if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
  {
    ImVec2 d(io.MousePos.x - c.x, io.MousePos.y - c.y);
    float rr2 = d.x*d.x + d.y*d.y;

    if (in_window)
    {
      // Turning drum: delta angle -> delta knots (60 knots per revolution)
      float ang0 = store->GetFloat(key_ang0, 0.0f);
      float v0   = store->GetFloat(key_val0, *torp_knots_io);

      float ang  = std::atan2(d.y, d.x);
      float delta = WrapPi(ang - ang0);                // + when mouse moves CW in screen angles
      float delta_kn = -(delta / (2.0f * std::numbers::pi_v<float>)) * 60.0f; // choose sign that feels right

      float new_torp = std::clamp(v0 + delta_kn, kTorpMin, kTorpMax);
      if (new_torp != *torp_knots_io) { *torp_knots_io = new_torp; changed = true; }
    }
    else
    {
      // Outer dial: set target speed by angle, but ignore clicks below the dial circle
      if (rr2 <= radius * radius)
      {
        float bdeg = BearingFromMouse(c, io.MousePos);    // 0..360
        float v = (bdeg / 360.0f) * 60.0f;                // 0..60
        // snap / clamp as desired:
        float new_target = std::clamp(v, kTargetMin, kTargetMax);
        if (new_target != *target_knots_io) { *target_knots_io = new_target; changed = true; }
      }
    }
  }

  // --- render dial face ---
  DrawBezel3(dl, c, radius, st.col_bezel_out, st.col_bezel_in, st.col_face);

  // Outer ticks & labels (0..55 shown; scale is 0..60 wrapped)
  {
    const float r_ticks = radius * 0.86f;
    const float r_text  = radius * 0.70f;

    for (int v = 0; v <= 60; ++v)
    {
      bool minor = true;
      bool major5  = (v % 5)  == 0;
      bool major10 = (v % 10) == 0;

      float len   = major10 ? radius * 0.14f : (major5 ? radius * 0.10f : radius * 0.07f);
      float thick = major10 ? radius * 0.028f : (major5 ? radius * 0.020f : radius * 0.014f);

      float bdeg = SpeedToBearingDeg_0to60((float)v);
      float a = AngleFromBearingDeg(bdeg);

      ImVec2 p0(c.x + std::cos(a) * r_ticks,         c.y + std::sin(a) * r_ticks);
      ImVec2 p1(c.x + std::cos(a) * (r_ticks - len), c.y + std::sin(a) * (r_ticks - len));
      dl->AddLine(p0, p1, col_tick, thick);

      // Label every 5 (or every 10). The photo labels 0,5,10...55.
      if (major5 && v <= 55)
      {
        char buf[8];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "%d", v);

        ImVec2 pt(c.x + std::cos(a) * r_text, c.y + std::sin(a) * r_text);
        AddCenteredTextEx(dl, font, font_size * 0.80f, pt, col_text, buf);
      }
    }
  }

  // Inner arc “window” band (upper semicircle)
  {
    // Band fill via thick arc stroke (simple + looks right enough)
    dl->PathClear();
    dl->PathArcTo(c, r_win_mid, win_ang0, win_ang1, 64);
    dl->PathStroke(IM_COL32(25,25,25,255), 0, win_band_w);

    // White border (outer + inner arc)
    dl->PathClear();
    dl->PathArcTo(c, r_win_out, win_ang0, win_ang1, 64);
    dl->PathStroke(IM_COL32(230,230,230,220), 0, std::max(1.0f, radius * 0.012f));

    dl->PathClear();
    dl->PathArcTo(c, r_win_in, win_ang0, win_ang1, 64);
    dl->PathStroke(IM_COL32(230,230,230,220), 0, std::max(1.0f, radius * 0.012f));

    // End caps (make it feel like a “window” rather than a pure arc)
    for (float end_ang : { win_ang0, win_ang1 })
    {
      ImVec2 dir(std::cos(end_ang), std::sin(end_ang));
      ImVec2 p0 = c + dir * r_win_in;
      ImVec2 p1 = c + dir * r_win_out;
      dl->AddLine(p0, p1, IM_COL32(230,230,230,220), std::max(1.0f, radius * 0.012f));
    }

    // Fixed down-facing triangle marker at top center
    {
      ImVec2 tip = c + ImVec2(0.0f, -r_win_out - radius * 0.02f);
      DrawDownTriangle(dl, tip, radius * 0.04f, radius * 0.06f, IM_COL32(240,240,240,220));
    }

    // Window labels (simple upright text like the photos)
    AddCenteredTextEx(dl, font, font_size * 0.65f,
                      ImVec2(c.x - radius * 0.24f, c.y - radius * 0.02f),
                      IM_COL32(240,240,240,160), "Vt");

    AddCenteredTextEx(dl, font, font_size * 0.60f,
                      ImVec2(c.x + radius * 0.24f, c.y - radius * 0.02f),
                      IM_COL32(240,240,240,160), "sm/Std");

    // Inner drum ticks/labels: 0..60 mapped to full circle, rotated so torp_knots sits under marker
    const float torp_bdeg = SpeedToBearingDeg_0to60(*torp_knots_io);
    const float torp_ang_at_marker = AngleFromBearingDeg(torp_bdeg);
    const float drum_offset = marker_ang - torp_ang_at_marker;

    auto VisibleInWindow = [&](float ang) -> bool {
      float rel = WrapPi(ang - marker_ang);
      return std::fabs(rel) <= std::numbers::pi_v<float> * 0.5f;
    };

    const float tick_r0 = r_win_out - radius * 0.012f;
    const float tick_r1_minor = r_win_mid;
    const float tick_r1_major = r_win_in + radius * 0.010f;

    for (int v = 0; v <= 60; ++v)
    {
      bool major5  = (v % 5) == 0;
      bool major10 = (v % 10) == 0;

      float bdeg = SpeedToBearingDeg_0to60((float)v);
      float ang  = AngleFromBearingDeg(bdeg) + drum_offset;

      if (!VisibleInWindow(ang))
        continue;

      ImVec2 dir(std::cos(ang), std::sin(ang));

      float thick = major10 ? radius * 0.012f : (major5 ? radius * 0.010f : radius * 0.008f);
      thick = std::max(1.0f, thick);

      float r1 = major5 ? tick_r1_major : tick_r1_minor;

      dl->AddLine(c + dir * tick_r0, c + dir * r1, IM_COL32(240,240,240,170), thick);

      // Labels every 5 (Wolfpack shows 40/45 etc), keep it modest
      if (major5)
      {
        char buf[8];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "%d", v);

        ImVec2 pt = c + dir * (r_win_mid - radius * 0.02f);
        AddCenteredTextEx(dl, font, font_size * 0.60f, pt, IM_COL32(240,240,240,170), buf);
      }
    }
  }

  // Outer needle: target speed (0..55 on the 0..60 ring)
  {
    float bdeg = SpeedToBearingDeg_0to60(*target_knots_io);
    float ang  = AngleFromBearingDeg(bdeg);
    DrawNeedle(dl, c, radius, ang, st.col_needle, st.col_shadow);
  }

  // Nameplate at bottom (your convention)
  if (has_label)
  {
    const float plate_pad_x = radius * 0.10f;
    ImVec2 p0(pos.x + plate_pad_x, pos.y + radius * 2.0f + gap);
    ImVec2 p1(pos.x + radius * 2.0f - plate_pad_x, p0.y + lab_h);

    float rounding  = radius * st.label_rounding;
    float border_th = std::max(1.0f, radius * st.label_border_th);

    DrawNamePlate(dl, font, font_size,
                  p0, p1, label,
                  st.label_screws,
                  st.col_plate_fill,
                  st.col_plate_border,
                  st.col_plate_text,
                  rounding,
                  border_th);
  }

  ImGui::Dummy(size);
  ImGui::PopID();
  return changed;
}
