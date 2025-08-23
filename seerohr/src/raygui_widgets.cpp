// TU header --------------------------------------------
#include "raygui_widgets.h"

#if CONFIG_USE_RAYGUI

// c++ headers ------------------------------------------
#include <cassert>

#include <unordered_map>
#include <string_view>

// external headers -------------------------------------
#include "raygui.h"

// project headers --------------------------------------
#include "mbase/container.h"

namespace {

template <class T>
inline void hash_combine(size_t& inout_seed, T const & v) {
  std::hash<T> hasher;
  inout_seed ^= hasher(v) + 0x9e3779b9 + (inout_seed << 6) + (inout_seed >> 2);
}

} // namespace

// ------------------------------------------------------------------
// Widget State Management

#define PP_VA_ARGS(...) , ##__VA_ARGS__

#define PP_CONCAT_IMPL(x, y) x##y
#define PP_CONCAT(x, y) PP_CONCAT_IMPL(x, y)

namespace {

struct WidgetState final {
  bool edit_mode = false;
};

struct WidgetStateHashStack final {
  mbase::SmallVector<uint64_t, 4> hash_values;

  friend bool operator==(WidgetStateHashStack const& lhs, WidgetStateHashStack const& rhs) {
    return lhs.hash_values == rhs.hash_values;
  }

  struct Hasher final {
    size_t operator()(WidgetStateHashStack const& v) const {
      size_t result_hash = 0;
      for (auto hash_value : v.hash_values) {
        hash_combine(result_hash, hash_value);
      }
      return result_hash;
    }
  };
};

std::unordered_map<WidgetStateHashStack, WidgetState, WidgetStateHashStack::Hasher> s_widget_states;

WidgetStateHashStack s_widget_state_hash_stack;

WidgetState& GetWidgetState() {
  return s_widget_states[s_widget_state_hash_stack];
}

WidgetState& PushHashString(std::string_view label) {
  std::hash<std::string_view> hasher;
  s_widget_state_hash_stack.hash_values.emplace_back(hasher(label));
  return GetWidgetState();
}

void PopHash() {
  assert(!s_widget_state_hash_stack.hash_values.empty() && "PopHash called on empty hash stack");
  s_widget_state_hash_stack.hash_values.pop_back();
}

class ScopedHash final {
public:
  explicit ScopedHash(std::string_view str) {
    PushHashString(str);
  }

  ~ScopedHash() {
    PopHash();
  }

  ScopedHash(ScopedHash const&) = delete;
  ScopedHash& operator=(ScopedHash const&) = delete;
  ScopedHash(ScopedHash&&) = delete;
  ScopedHash& operator=(ScopedHash&&) = delete;
};

#define SCOPED_HASH(value)                                \
  ScopedHash PP_CONCAT(scoped_hash_, __COUNTER__)(value);
#define SCOPED_HASH_WS(state_var_name, value)             \
  ScopedHash PP_CONCAT(scoped_hash_, __COUNTER__)(value); \
  WidgetState& state_var_name = GetWidgetState();

} // namespace

// ------------------------------------------------------------------
// Public API

bool SrGuiPanel(
  Rectangle const& bounds,
  char const* label,
  SrPanelContent const& content
) {
  SCOPED_HASH(label);

  int const result = GuiPanel(bounds, label);

  SrPanelContentContext ctx = [&bounds](Rectangle const& content_bounds) {
    Rectangle adjusted_bounds = content_bounds;
    adjusted_bounds.x += 5 + bounds.x;
    adjusted_bounds.y += 5 + bounds.y;
    return adjusted_bounds;
  };

  content(ctx);

  return result != 0;
}

bool SrGuiValueBoxI32(
  Rectangle const& bounds,
  char const* label,
  int32_t* value,
  int32_t min_value,
  int32_t max_value
) {
  SCOPED_HASH_WS(ws, label);

  if (GuiValueBox(bounds, label, value, min_value, max_value, ws.edit_mode)) {
    ws.edit_mode = !ws.edit_mode;
    return true;
  }
  return false;
}

bool SrGuiValueBoxU32(
  Rectangle const& bounds,
  char const* label,
  uint32_t* value,
  uint32_t min_value,
  uint32_t max_value
) {
  return SrGuiValueBoxI32(bounds, label, reinterpret_cast<int32_t*>(value), int32_t(min_value), int32_t(max_value));
}

bool SrGuiSliderF32(
  Rectangle const& bounds,
  char const* label,
  char const* value_text,
  float* value,
  float min_value,
  float max_value
) {
  SCOPED_HASH(label);

  int const result = GuiSlider(bounds, label, value_text, value, min_value, max_value);
  return result != 0;
}

bool SrGuiSliderI32(
  Rectangle const& bounds,
  char const* label,
  char const* value_text,
  int32_t* value,
  int32_t min_value,
  int32_t max_value
) {
  float value_f32 = float(*value);

  int const result = SrGuiSliderF32(bounds, label, value_text, &value_f32, float(min_value), float(max_value));
  if (result != 0) *value = int32_t(value_f32);
  return result != 0;
}

#endif
