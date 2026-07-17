/**
 * @file        ui/overlay/simple_settings_overlay.cpp
 *
 * @brief       Curated user-facing settings overlay.
 *
 * Fullscreen, console-style settings screen: category rail on the left, a
 * column of setting rows with left/right value steppers in the middle, a
 * description panel on the right and a button legend along the bottom.
 * Fully navigable with a controller (dpad/left stick, A/B/X/Y, LB/RB),
 * keyboard (arrows/Enter/Esc/Q/E/R/F) and mouse.
 */
#include <rex/ui/overlay/simple_settings_overlay.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <toml++/toml.hpp>

namespace rex::ui {
namespace {

constexpr std::array<int32_t, 3> kResolutionScales = {1, 2, 3};
constexpr std::array<const char*, 3> kResolutionLabels = {"720p (1x)", "1440p (2x)",
                                                          "2160p (3x)"};
constexpr std::array<const char*, 2> kAspectRatioLabels = {"16:9", "21:9 (Experimental)"};
constexpr std::array<double, 7> kFrameCapRates = {60.0,  90.0,  120.0, 140.0,
                                                  144.0, 165.0, 240.0};
constexpr std::array<const char*, 8> kFrameCapLabels = {"Unlimited", "60 FPS",  "90 FPS",
                                                        "120 FPS",   "140 FPS", "144 FPS",
                                                        "165 FPS",   "240 FPS"};
constexpr std::array<std::string_view, 7> kCoreSimpleSettingsCvars = {
    "resolution_scale",
    "draw_resolution_scale_x",
    "draw_resolution_scale_y",
    "fullscreen",
    "vsync",
    "mnk_mode",
    "mnk_capture_mouse"};

// X_INPUT_GAMEPAD_* button bits, mirrored locally to keep the UI overlay
// decoupled from the kernel input headers.
constexpr uint16_t kPadDpadUp = 0x0001;
constexpr uint16_t kPadDpadDown = 0x0002;
constexpr uint16_t kPadDpadLeft = 0x0004;
constexpr uint16_t kPadDpadRight = 0x0008;
constexpr uint16_t kPadLShoulder = 0x0100;
constexpr uint16_t kPadRShoulder = 0x0200;
constexpr uint16_t kPadA = 0x1000;
constexpr uint16_t kPadB = 0x2000;
constexpr uint16_t kPadX = 0x4000;
constexpr uint16_t kPadY = 0x8000;

// Categories shown in the left rail.
struct CategoryInfo {
  const char* name;
  const char* desc;
};
constexpr std::array<CategoryInfo, 4> kCategories = {{
    {"Video", "Display, resolution and framerate settings."},
    {"Controls", "Mouse and keyboard input settings."},
    {"Profile", "Local player profile and sign-in."},
    {"System", "Apply or revert pending changes and close the settings."},
}};

// Navigation repeat pacing (seconds).
constexpr float kRepeatDelay = 0.42f;
constexpr float kRepeatRate = 0.11f;

// ---- Palette -------------------------------------------------------------
// Frosted-panel color scheme (iterated against
// real gameplay captures): teal frosted side panels with white text over the
// blurred scene, WHITE setting rows with near-black text, black focused row
// with a lime ring, lime section/description bars, magenta steppers/slider.

constexpr ImU32 kColSelFill = IM_COL32(10, 12, 12, 255);
constexpr ImU32 kColSelText = IM_COL32(250, 252, 252, 255);
// Rows are OPAQUE (no scene bleed-through) but tuned to the tone the
// raw white would render (~235 vs 252) so they don't read as
// too bright or too contrasty.
constexpr ImU32 kColPanel = IM_COL32(235, 236, 236, 255);
constexpr ImU32 kColPanelHover = IM_COL32(248, 250, 250, 255);
constexpr ImU32 kColPanelBorder = IM_COL32(0, 0, 0, 26);
constexpr ImU32 kColRowText = IM_COL32(15, 17, 18, 255);
constexpr ImU32 kColRailPanel = IM_COL32(14, 56, 53, 140);
constexpr ImU32 kColRailPanelHover = IM_COL32(24, 90, 86, 166);
constexpr ImU32 kColRailBorder = IM_COL32(255, 255, 255, 26);
constexpr ImU32 kColText = IM_COL32(235, 242, 241, 255);
constexpr ImU32 kColTextDim = IM_COL32(224, 235, 234, 191);
constexpr ImU32 kColTextFaint = IM_COL32(150, 158, 158, 255);
constexpr ImU32 kColAccent = IM_COL32(213, 235, 10, 255);
constexpr ImU32 kColAccentDark = IM_COL32(13, 15, 5, 255);
constexpr ImU32 kColInteract = IM_COL32(230, 0, 120, 255);
constexpr ImU32 kColInteractHover = IM_COL32(255, 25, 145, 255);
constexpr ImU32 kColDanger = IM_COL32(233, 88, 76, 255);
constexpr ImU32 kColWarn = IM_COL32(245, 197, 87, 255);
constexpr ImU32 kColDescPanel = IM_COL32(11, 46, 43, 140);
constexpr ImU32 kColChevDisabled = IM_COL32(0, 0, 0, 56);
// The disabled chevron on the FOCUSED (black) row needs a light variant -
// black-alpha disappears there.
constexpr ImU32 kColChevDisabledOnDark = IM_COL32(255, 255, 255, 64);
constexpr ImU32 kColLegendChip = IM_COL32(238, 240, 240, 255);
constexpr ImU32 kColLegendText = IM_COL32(15, 18, 20, 255);
constexpr ImU32 kColLegendLabel = IM_COL32(228, 236, 235, 255);
constexpr ImU32 kColWhite = IM_COL32(255, 255, 255, 255);

bool HasCvar(std::string_view name) {
  return rex::cvar::GetFlagInfo(name) != nullptr;
}

std::vector<std::string_view> GetSimpleSettingsCvars() {
  std::vector<std::string_view> cvars;
  cvars.reserve(15);
  cvars.insert(cvars.end(), kCoreSimpleSettingsCvars.begin(), kCoreSimpleSettingsCvars.end());
  // Literal names rather than the registry's string to keep the string_views
  // pointing at static storage.
  const std::string device_cvar = GetGraphicsDeviceList().cvar_name;
  if (device_cvar == "d3d12_adapter" && HasCvar("d3d12_adapter")) {
    cvars.push_back("d3d12_adapter");
  } else if (device_cvar == "vulkan_device" && HasCvar("vulkan_device")) {
    cvars.push_back("vulkan_device");
  }
  if (HasCvar("skate3_ultrawide")) {
    cvars.push_back("skate3_ultrawide");
  }
  if (HasCvar("skate3_field_of_view")) {
    cvars.push_back("skate3_field_of_view");
  }
  if (HasCvar("skate3_guest_fps_cap")) {
    cvars.push_back("skate3_guest_fps_cap");
  }
  if (HasCvar("d3d12_present_frame_limiter")) {
    cvars.push_back("d3d12_present_frame_limiter");
  }
  if (HasCvar("d3d12_present_frame_limiter_fps")) {
    cvars.push_back("d3d12_present_frame_limiter_fps");
  }
  if (HasCvar("d3d12_allow_variable_refresh_rate_and_tearing")) {
    cvars.push_back("d3d12_allow_variable_refresh_rate_and_tearing");
  }
  if (HasCvar("vulkan_allow_present_mode_immediate")) {
    cvars.push_back("vulkan_allow_present_mode_immediate");
  }
  if (HasCvar("vulkan_allow_present_mode_mailbox")) {
    cvars.push_back("vulkan_allow_present_mode_mailbox");
  }
  if (HasCvar("vulkan_allow_present_mode_fifo_relaxed")) {
    cvars.push_back("vulkan_allow_present_mode_fifo_relaxed");
  }
  return cvars;
}

// 0 = automatic selection (cvar -1); i+1 = the list's device i.
int DeviceIndexFromCvar(const GraphicsDeviceList& device_list) {
  if (device_list.device_names.empty() || !HasCvar(device_list.cvar_name)) {
    return 0;
  }
  int32_t current = rex::cvar::Query<int32_t>(device_list.cvar_name);
  if (current >= 0 && current < static_cast<int32_t>(device_list.device_names.size())) {
    return current + 1;
  }
  return 0;
}

int ResolutionIndexFromScale(int32_t scale) {
  int best = 0;
  int32_t best_delta = 1000;
  for (int i = 0; i < static_cast<int>(kResolutionScales.size()); ++i) {
    int32_t delta = kResolutionScales[i] > scale ? kResolutionScales[i] - scale
                                                 : scale - kResolutionScales[i];
    if (delta < best_delta) {
      best = i;
      best_delta = delta;
    }
  }
  return best;
}

int ResolutionIndexFromCvar() {
  return ResolutionIndexFromScale(rex::cvar::Query<int32_t>("resolution_scale"));
}

bool HasHostFrameCapCvars() {
  return HasCvar("d3d12_present_frame_limiter") && HasCvar("d3d12_present_frame_limiter_fps");
}

// The guest-side cap paces the game's render loop at the swap boundary, so
// both content production and present cadence land on an even beat (the VRR
// case the host present limiter can't fix: it only delays presents of frames
// whose content was already produced on an irregular schedule). It works on
// any backend, but only when the native-render hook layer that implements it
// is active; otherwise fall back to the host present limiter.
bool UseGuestFrameCap() {
  return HasCvar("skate3_guest_fps_cap") && HasCvar("skate3_native_render") &&
         rex::cvar::Query<bool>("skate3_native_render");
}

bool HasFrameCapControl() {
  return UseGuestFrameCap() || HasHostFrameCapCvars();
}

int FrameCapIndexFromRate(double rate) {
  if (rate < 1.0) {
    return 0;
  }
  int best = 1;
  double best_delta = 1000.0;
  for (int i = 0; i < static_cast<int>(kFrameCapRates.size()); ++i) {
    double delta = std::abs(kFrameCapRates[i] - rate);
    if (delta < best_delta) {
      best = i + 1;
      best_delta = delta;
    }
  }
  return best;
}

int FrameCapIndexFromCvar() {
  double current = 0.0;
  if (UseGuestFrameCap()) {
    current = rex::cvar::Query<double>("skate3_guest_fps_cap");
  } else if (HasHostFrameCapCvars() && rex::cvar::Query<bool>("d3d12_present_frame_limiter")) {
    current = rex::cvar::Query<double>("d3d12_present_frame_limiter_fps");
  }
  return FrameCapIndexFromRate(current);
}

bool HasFieldOfViewCvar() {
  return HasCvar("skate3_field_of_view");
}

float FieldOfViewFromCvar() {
  if (!HasFieldOfViewCvar()) {
    return 60.0f;
  }
  return float(std::clamp(rex::cvar::Query<double>("skate3_field_of_view"), 40.0, 120.0));
}

// ---- CVar default parsing (Y / R "reset to default") ----------------------

std::optional<std::string> CvarDefault(std::string_view name) {
  const auto* info = rex::cvar::GetFlagInfo(name);
  if (!info) {
    return std::nullopt;
  }
  return info->default_value;
}

bool CvarDefaultBool(std::string_view name, bool fallback) {
  auto value = CvarDefault(name);
  if (!value) {
    return fallback;
  }
  return *value == "true" || *value == "1";
}

double CvarDefaultDouble(std::string_view name, double fallback) {
  auto value = CvarDefault(name);
  if (!value || value->empty()) {
    return fallback;
  }
  return std::strtod(value->c_str(), nullptr);
}

void CopyToBuffer(char* buffer, size_t buffer_size, const std::string& value) {
  size_t count = std::min(value.size(), buffer_size - 1);
  std::copy_n(value.data(), count, buffer);
  buffer[count] = '\0';
}

void SetBoolCvar(std::string_view name, bool value) {
  rex::cvar::SetFlagByName(name, value ? "true" : "false");
}

bool TearingFromCvar() {
  if (HasCvar("d3d12_allow_variable_refresh_rate_and_tearing")) {
    return rex::cvar::Query<bool>("d3d12_allow_variable_refresh_rate_and_tearing");
  }
  if (HasCvar("vulkan_allow_present_mode_immediate")) {
    return rex::cvar::Query<bool>("vulkan_allow_present_mode_immediate");
  }
  return false;
}

bool TearingDefault() {
  if (HasCvar("d3d12_allow_variable_refresh_rate_and_tearing")) {
    return CvarDefaultBool("d3d12_allow_variable_refresh_rate_and_tearing", true);
  }
  return CvarDefaultBool("vulkan_allow_present_mode_immediate", true);
}

void SetTearingCvars(bool value) {
  if (HasCvar("d3d12_allow_variable_refresh_rate_and_tearing")) {
    SetBoolCvar("d3d12_allow_variable_refresh_rate_and_tearing", value);
  }
  if (HasCvar("vulkan_allow_present_mode_immediate")) {
    SetBoolCvar("vulkan_allow_present_mode_immediate", value);
  }
  if (HasCvar("vulkan_allow_present_mode_mailbox")) {
    SetBoolCvar("vulkan_allow_present_mode_mailbox", value);
  }
  if (HasCvar("vulkan_allow_present_mode_fifo_relaxed")) {
    SetBoolCvar("vulkan_allow_present_mode_fifo_relaxed", value);
  }
}

// ---- Small draw helpers ----------------------------------------------------

// Snap to whole pixels. Fractional origins under bilinear filtering smear
// glyph edges and turn 1px rules into soft 2px lines - browsers pixel-snap
// text and borders, so snapping to whole pixels keeps them crisp.
float Snap(float value) {
  return std::floor(value + 0.5f);
}

std::string TruncateToWidth(ImFont* font, float size, float max_width, std::string text) {
  if (font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x <= max_width) {
    return text;
  }
  while (!text.empty() &&
         font->CalcTextSizeA(size, FLT_MAX, 0.0f, (text + "...").c_str()).x > max_width) {
    text.pop_back();
  }
  return text + "...";
}

void AddTextCentered(ImDrawList* dl, ImFont* font, float size, ImVec2 center, ImU32 col,
                     const char* text) {
  ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
  dl->AddText(font, size,
              ImVec2(Snap(center.x - extent.x * 0.5f), Snap(center.y - extent.y * 0.5f)), col,
              text);
}

// Optical centering for short glyph labels (pad button circles, key chips):
// AddTextCentered centers the line box, but a capital's visual ink sits
// asymmetrically inside it (descender slack below, bearings on the sides), so
// the letter lands ~1px down-right of a small circle's center. Vertical
// centers the CAP-HEIGHT BAND taken from 'H'
// rather than each glyph's own ink: per-glyph raster boxes round
// independently (A has a pointed-apex overshoot, Y doesn't), which lands
// adjacent buttons on different subpixel rows - the eye catches that
// inconsistency more than any absolute offset. One shared band = identical
// placement for every label, overshoots hanging evenly, which is also what
// the browser's hinted rasterizer converges to. Horizontal centers the ink
// for single glyphs (circles) or the advance box when ink_x is false -
// multi-glyph chip labels match the browser's text-box centering that way.
// ASCII labels only.
void AddTextCenteredCap(ImDrawList* dl, ImFont* font, float size, ImVec2 center, ImU32 col,
                        const char* text, bool ink_x) {
  ImFontBaked* baked = font->GetFontBaked(size);
  float pen = 0.0f;
  float min_x = FLT_MAX, max_x = -FLT_MAX;
  for (const char* p = text; *p; ++p) {
    const ImFontGlyph* glyph = baked->FindGlyph(ImWchar(uint8_t(*p)));
    if (!glyph) {
      continue;
    }
    if (glyph->Visible) {
      min_x = std::min(min_x, pen + glyph->X0);
      max_x = std::max(max_x, pen + glyph->X1);
    }
    pen += glyph->AdvanceX;
  }
  if (min_x > max_x) {
    return;  // no visible ink
  }
  float band_top = 0.0f;
  float band_bottom = size;
  if (const ImFontGlyph* cap = baked->FindGlyphNoFallback(ImWchar('H'))) {
    band_top = cap->Y0;
    band_bottom = cap->Y1;
  }
  const float x = ink_x ? center.x - (min_x + max_x) * 0.5f : center.x - pen * 0.5f;
  // Round-half-DOWN for the vertical position: the cap band regularly has a
  // half-integer center (e.g. 18px band in a 52px circle), and Snap's
  // round-half-up resolved every tie downward - measured exactly 1px low
  // versus the browser, which resolves the same tie upward (the typographic
  // convention: when a glyph can't center exactly, err high).
  const float y = center.y - (band_top + band_bottom) * 0.5f;
  dl->AddText(font, size, ImVec2(Snap(x), std::ceil(y - 0.5f)), col, text);
}

void AddTextVCentered(ImDrawList* dl, ImFont* font, float size, float x, float center_y,
                      ImU32 col, const char* text) {
  ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
  dl->AddText(font, size, ImVec2(Snap(x), Snap(center_y - extent.y * 0.5f)), col, text);
}

// Justified paragraph (browser text-align: justify): wraps words to wrap_w
// and stretches the word gaps of every full line so both edges align; the
// last line of the paragraph stays ragged-left. Returns the height consumed.
float AddTextJustified(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, float wrap_w,
                       ImU32 col, const char* text) {
  std::vector<std::string_view> words;
  for (const char* p = text; *p;) {
    while (*p == ' ') {
      ++p;
    }
    const char* start = p;
    while (*p && *p != ' ') {
      ++p;
    }
    if (p > start) {
      words.emplace_back(start, size_t(p - start));
    }
  }
  if (words.empty()) {
    return 0.0f;
  }
  const float space_w = font->CalcTextSizeA(size, FLT_MAX, 0.0f, " ").x;
  auto word_w = [&](std::string_view w) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, w.data(), w.data() + w.size()).x;
  };
  float y = pos.y;
  size_t i = 0;
  while (i < words.size()) {
    float line_w = word_w(words[i]);
    size_t j = i + 1;
    while (j < words.size()) {
      const float w = word_w(words[j]);
      if (line_w + space_w + w > wrap_w) {
        break;
      }
      line_w += space_w + w;
      ++j;
    }
    const bool last_line = j == words.size();
    float gap = space_w;
    if (!last_line && j - i > 1) {
      const float words_only = line_w - space_w * float(j - i - 1);
      const float stretched = (wrap_w - words_only) / float(j - i - 1);
      // Conditional justify: only stretch when the gaps stay modest -
      // beyond ~1.6x a normal space the stretched line reads worse than a
      // ragged one (the "big spaces" objection to naive justification).
      if (stretched <= space_w * 1.6f) {
        gap = stretched;
      }
    }
    float x = pos.x;
    for (size_t k = i; k < j; ++k) {
      dl->AddText(font, size, ImVec2(Snap(x), Snap(y)), col, words[k].data(),
                  words[k].data() + words[k].size());
      x += word_w(words[k]) + gap;
    }
    y += size;
    i = j;
  }
  return y - pos.y;
}

// Focused-item highlight: rounded black fill with a lime ring and a thicker
// black outer edge, both drawn OUTSIDE the box (stacked
// box-shadows). Total ring = 6*s beyond the rect on every side; callers keep
// a matching margin in their clip rects.
void DrawFocusHighlight(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float s) {
  const float radius = 6.0f * s;
  dl->AddRectFilled(p_min, p_max, kColSelFill, radius);
  // Lime band: box edge -> +2*s (stroke centered at +1*s).
  dl->AddRect(ImVec2(p_min.x - 1.0f * s, p_min.y - 1.0f * s),
              ImVec2(p_max.x + 1.0f * s, p_max.y + 1.0f * s), kColAccent, radius + 1.0f * s, 0,
              2.0f * s);
  // Black outer edge: +2*s -> +6*s (stroke centered at +4*s).
  dl->AddRect(ImVec2(p_min.x - 4.0f * s, p_min.y - 4.0f * s),
              ImVec2(p_max.x + 4.0f * s, p_max.y + 4.0f * s), kColSelFill, radius + 4.0f * s, 0,
              4.0f * s);
}

// Stepper chevron: a triangle in a circle.
void DrawChevron(ImDrawList* dl, ImVec2 center, float radius, bool points_left, ImU32 circle_col,
                 ImU32 tri_col, bool filled) {
  center = ImVec2(Snap(center.x), Snap(center.y));
  if (filled) {
    dl->AddCircleFilled(center, radius, circle_col, 24);
  } else {
    dl->AddCircle(center, radius, circle_col, 24, 1.5f);
  }
  // Tip-anchored triangle (tip reaches further from center than the back
  // edge). A bbox-centered variant was tried and looked worse in playtest -
  // keep this geometry.
  float t = radius * 0.42f;
  float dir = points_left ? -1.0f : 1.0f;
  ImVec2 tip(center.x + dir * t, center.y);
  ImVec2 a(center.x - dir * t * 0.6f, center.y - t);
  ImVec2 b(center.x - dir * t * 0.6f, center.y + t);
  dl->AddTriangleFilled(tip, a, b, tri_col);
}

struct LegendGlyph {
  const char* glyph;
  const char* label;
  bool circle;  // pad face button -> circle, everything else -> key chip
};

}  // namespace

void SaveSimpleSettingsConfig(const std::filesystem::path& config_path) {
  rex::cvar::SaveConfigValues(config_path, GetSimpleSettingsCvars());
}

void EnsureSimpleSettingsConfig(const std::filesystem::path& config_path) {
  bool should_save = !std::filesystem::exists(config_path);
  if (!should_save) {
    try {
      auto config = toml::parse_file(config_path.string());
      for (std::string_view name : GetSimpleSettingsCvars()) {
        if (!config.contains(name)) {
          should_save = true;
          break;
        }
      }
    } catch (const toml::parse_error&) {
      should_save = true;
    }
  }

  if (should_save) {
    SaveSimpleSettingsConfig(config_path);
  }
}

// One setting row (or section header) in the content column.
struct SimpleSettingsDialog::RowSpec {
  enum Kind { kHeader, kEnum, kSlider, kAction, kText };
  Kind kind = kEnum;
  const char* label = nullptr;
  const char* desc = nullptr;
  std::string desc_extra;       // dynamic addendum shown under desc
  const char* value_note = nullptr;  // short warning shown in the description
  bool enabled = true;
  bool danger = false;
  // kEnum (options step left/right; either index or flag backs the value)
  std::vector<std::string> options;
  int* index = nullptr;
  bool* flag = nullptr;
  std::function<void(int)> on_enum_change;
  // kSlider
  float* value = nullptr;
  float min = 0.0f;
  float max = 1.0f;
  float step = 1.0f;
  const char* fmt = "%.0f";
  std::function<void()> on_value_change;
  // kAction
  std::function<void()> action;
  // kText
  char* text_buf = nullptr;
  size_t text_buf_size = 0;
  // Reset-to-default handler (Y / R), unset when the row has no default.
  std::function<void()> reset;
};

// Per-frame navigation intents merged from pad and keyboard.
struct SimpleSettingsDialog::NavIntents {
  int move_x = 0;
  int move_y = 0;
  bool select = false;
  bool back = false;
  bool category_prev = false;
  bool category_next = false;
  bool reset_default = false;
  bool apply_restart = false;
};

SimpleSettingsDialog::SimpleSettingsDialog(ImGuiDrawer* drawer, std::filesystem::path config_path,
                                           LoadProfilesCallback load_profiles,
                                           SaveProfileCallback save_profile,
                                           CloseSettingsCallback close_settings,
                                           CloseGameCallback close_game,
                                           RestartGameCallback restart_game,
                                           PollGamepadCallback poll_gamepad)
    : ImGuiDialog(drawer),
      config_path_(std::move(config_path)),
      load_profiles_(std::move(load_profiles)),
      save_profile_(std::move(save_profile)),
      close_settings_(std::move(close_settings)),
      close_game_(std::move(close_game)),
      restart_game_(std::move(restart_game)),
      poll_gamepad_(std::move(poll_gamepad)) {
  ReloadProfiles();
  LoadSettingsFromCvars();
  SetDrawActive(false);
}

SimpleSettingsDialog::~SimpleSettingsDialog() = default;

void SimpleSettingsDialog::Show() {
  visible_ = true;
  SetDrawActive(true);
  ReloadProfiles();
  LoadSettingsFromCvars();
  zone_ = FocusZone::kRail;
  rail_sel_ = category_;
  row_index_ = 0;
  content_scroll_ = 0.0f;
  editing_text_ = false;
  highlight_anim_y_ = -1.0f;
  rail_anim_y_ = -1.0f;
  prev_pad_buttons_ = 0xFFFF;  // swallow buttons already held at open
  just_shown_ = true;          // swallow the stale cursor delta too
}

void SimpleSettingsDialog::LoadSettingsFromCvars() {
  device_list_ = GetGraphicsDeviceList();
  device_index_ = DeviceIndexFromCvar(device_list_);
  resolution_scale_index_ = ResolutionIndexFromCvar();
  frame_cap_index_ = FrameCapIndexFromCvar();
  aspect_ratio_index_ =
      HasCvar("skate3_ultrawide") && rex::cvar::Query<bool>("skate3_ultrawide") ? 1 : 0;
  field_of_view_ = FieldOfViewFromCvar();
  fullscreen_ = rex::cvar::Query<bool>("fullscreen");
  vsync_ = rex::cvar::Query<bool>("vsync");
  tearing_ = TearingFromCvar();
  mnk_mode_ = rex::cvar::Query<bool>("mnk_mode");
  mnk_capture_mouse_ = rex::cvar::Query<bool>("mnk_capture_mouse");
}

bool SimpleSettingsDialog::HasSettingsChanges() const {
  return device_index_ != DeviceIndexFromCvar(device_list_) ||
         resolution_scale_index_ != ResolutionIndexFromCvar() ||
         frame_cap_index_ != FrameCapIndexFromCvar() ||
         (HasCvar("skate3_ultrawide") &&
          (aspect_ratio_index_ != 0) != rex::cvar::Query<bool>("skate3_ultrawide")) ||
         fullscreen_ != rex::cvar::Query<bool>("fullscreen") ||
         vsync_ != rex::cvar::Query<bool>("vsync") ||
         tearing_ != TearingFromCvar() ||
         mnk_mode_ != rex::cvar::Query<bool>("mnk_mode") ||
         mnk_capture_mouse_ != rex::cvar::Query<bool>("mnk_capture_mouse");
}

void SimpleSettingsDialog::Toggle() {
  if (visible_) {
    Hide();
    return;
  }
  Show();
}

void SimpleSettingsDialog::Hide() {
  if (!visible_) {
    return;
  }
  visible_ = false;
  editing_text_ = false;
  SetDrawActive(false);
  if (close_settings_) {
    close_settings_();
  }
}

void SimpleSettingsDialog::NavigateBack() {
  if (!visible_) {
    return;
  }
  if (editing_text_) {
    editing_text_ = false;
    return;
  }
  if (zone_ == FocusZone::kContent) {
    zone_ = FocusZone::kRail;
    return;
  }
  Hide();
}

void SimpleSettingsDialog::ReloadProfiles() {
  profiles_ = load_profiles_ ? load_profiles_() : SimpleProfileState{};
  if (profiles_.profiles.empty()) {
    profiles_.profiles.push_back({"default", "Player"});
    profiles_.selected_index = 0;
  }
  profiles_.selected_index =
      std::clamp(profiles_.selected_index, 0, static_cast<int>(profiles_.profiles.size()) - 1);
  CopyToBuffer(gamertag_buf_, sizeof(gamertag_buf_),
               profiles_.profiles[profiles_.selected_index].gamertag);
  profile_signed_in_ = profiles_.profiles[profiles_.selected_index].signed_in;
}

void SimpleSettingsDialog::SaveVideo() {
  resolution_scale_index_ =
      std::clamp(resolution_scale_index_, 0, static_cast<int>(kResolutionScales.size()) - 1);
  frame_cap_index_ =
      std::clamp(frame_cap_index_, 0, static_cast<int>(kFrameCapLabels.size()) - 1);
  aspect_ratio_index_ =
      std::clamp(aspect_ratio_index_, 0, static_cast<int>(kAspectRatioLabels.size()) - 1);
  field_of_view_ = std::clamp(field_of_view_, 40.0f, 120.0f);
  if (!device_list_.device_names.empty() && HasCvar(device_list_.cvar_name)) {
    device_index_ =
        std::clamp(device_index_, 0, static_cast<int>(device_list_.device_names.size()));
    rex::cvar::SetFlagByName(device_list_.cvar_name,
                             device_index_ == 0 ? "-1" : std::to_string(device_index_ - 1));
  }
  const auto scale = std::to_string(kResolutionScales[resolution_scale_index_]);
  rex::cvar::SetFlagByName("resolution_scale", scale);
  rex::cvar::SetFlagByName("draw_resolution_scale_x", scale);
  rex::cvar::SetFlagByName("draw_resolution_scale_y", scale);
  if (UseGuestFrameCap()) {
    const double cap_fps = frame_cap_index_ != 0 ? kFrameCapRates[frame_cap_index_ - 1] : 0.0;
    rex::cvar::SetFlagByName("skate3_guest_fps_cap", std::to_string(cap_fps));
    // Never run both pacers: the host limiter waits on the paint thread
    // without backpressuring the guest, so presents drop frames on an
    // irregular beat: the judder the guest cap exists to remove.
    if (HasHostFrameCapCvars()) {
      SetBoolCvar("d3d12_present_frame_limiter", false);
    }
  } else if (HasHostFrameCapCvars()) {
    SetBoolCvar("d3d12_present_frame_limiter", frame_cap_index_ != 0);
    if (frame_cap_index_ != 0) {
      rex::cvar::SetFlagByName("d3d12_present_frame_limiter_fps",
                               std::to_string(kFrameCapRates[frame_cap_index_ - 1]));
    }
  }
  SetBoolCvar("fullscreen", fullscreen_);
  if (HasCvar("skate3_ultrawide")) {
    SetBoolCvar("skate3_ultrawide", aspect_ratio_index_ != 0);
  }
  if (HasFieldOfViewCvar()) {
    rex::cvar::SetFlagByName("skate3_field_of_view", std::to_string(field_of_view_));
  }
  SetBoolCvar("vsync", vsync_);
  SetTearingCvars(tearing_);
  SetBoolCvar("mnk_mode", mnk_mode_);
  SetBoolCvar("mnk_capture_mouse", mnk_capture_mouse_);
  SaveSimpleSettingsConfig(config_path_);
}

void SimpleSettingsDialog::SaveProfile() {
  if (save_profile_) {
    save_profile_(profiles_.selected_index, gamertag_buf_, profile_signed_in_);
  }
  ReloadProfiles();
}

void SimpleSettingsDialog::ApplyAndRestart() {
  SaveVideo();
  if (restart_game_) {
    restart_game_();
  }
}

void SimpleSettingsDialog::BuildRows(std::vector<RowSpec>& rows, int category) {
  rows.clear();
  const bool pending = HasSettingsChanges();

  // Section bars only SEPARATE groups - a category's first group
  // starts directly with its rows, so a leading header is dropped.
  auto header = [&rows](const char* label) {
    if (rows.empty()) {
      return;
    }
    RowSpec row;
    row.kind = RowSpec::kHeader;
    row.label = label;
    rows.push_back(std::move(row));
  };

  switch (category) {
    case 0: {  // Video
      header("Display");
      if (!device_list_.device_names.empty() && HasCvar(device_list_.cvar_name)) {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Graphics Device";
        row.desc =
            "Select which GPU renders the game. Auto picks the highest-performance adapter.";
        // Index-prefixed labels: virtual display drivers (Parsec etc.) make
        // the same physical GPU enumerate several times, and the index is
        // what the device cvar and the startup log speak in.
        row.options.push_back("Auto (recommended)");
        for (size_t i = 0; i < device_list_.device_names.size(); ++i) {
          row.options.push_back(std::to_string(i) + ": " + device_list_.device_names[i]);
        }
        row.index = &device_index_;
        if (device_index_ > 0 && device_index_ <= int(device_list_.device_names.size())) {
          row.desc_extra = "Current: " + row.options[device_index_];
        }
        row.reset = [this] { device_index_ = 0; };
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Resolution Scale";
        row.desc =
            "Render resolution as a multiple of the game's native 720p. Higher values are "
            "sharper but cost GPU time.";
        for (const char* label : kResolutionLabels) {
          row.options.push_back(label);
        }
        row.index = &resolution_scale_index_;
        row.reset = [this] {
          resolution_scale_index_ = ResolutionIndexFromScale(
              int32_t(CvarDefaultDouble("resolution_scale", 3.0)));
        };
        rows.push_back(std::move(row));
      }
      if (HasFrameCapControl()) {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Framerate Cap";
        row.desc =
            "Limit how fast the game runs. A steady cap slightly below your display's "
            "refresh rate gives the smoothest pacing on variable-refresh displays.";
        for (const char* label : kFrameCapLabels) {
          row.options.push_back(label);
        }
        row.index = &frame_cap_index_;
        row.reset = [this] {
          double rate = 0.0;
          if (UseGuestFrameCap()) {
            rate = CvarDefaultDouble("skate3_guest_fps_cap", 0.0);
          } else if (CvarDefaultBool("d3d12_present_frame_limiter", false)) {
            rate = CvarDefaultDouble("d3d12_present_frame_limiter_fps", 0.0);
          }
          frame_cap_index_ = FrameCapIndexFromRate(rate);
        };
        rows.push_back(std::move(row));
      }
      if (HasCvar("skate3_ultrawide")) {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Aspect Ratio";
        row.desc = "21:9 widens the world rendering for ultrawide displays.";
        row.value_note = "21:9 is experimental";
        for (const char* label : kAspectRatioLabels) {
          row.options.push_back(label);
        }
        row.index = &aspect_ratio_index_;
        row.reset = [this] {
          aspect_ratio_index_ = CvarDefaultBool("skate3_ultrawide", false) ? 1 : 0;
        };
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Fullscreen";
        row.desc = "Borderless fullscreen presentation instead of a window.";
        row.options = {"Off", "On"};
        row.flag = &fullscreen_;
        row.reset = [this] { fullscreen_ = CvarDefaultBool("fullscreen", true); };
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Vertical Synchronisation";
        row.desc =
            "Waits for the display before presenting frames. Adds latency and can cause "
            "judder with the frame pacing this port uses.";
        row.value_note = "Not recommended";
        row.options = {"Off", "On"};
        row.flag = &vsync_;
        row.reset = [this] { vsync_ = CvarDefaultBool("vsync", false); };
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Variable Refresh / Tearing";
        row.desc =
            "Allows uncapped presentation with variable-refresh displays (G-Sync / "
            "FreeSync). Recommended on.";
        row.options = {"Off", "On"};
        row.flag = &tearing_;
        row.reset = [this] { tearing_ = TearingDefault(); };
        rows.push_back(std::move(row));
      }
      if (HasFieldOfViewCvar()) {
        header("Gameplay");
        RowSpec row;
        row.kind = RowSpec::kSlider;
        row.label = "Field of View";
        row.desc = "Camera field of view in degrees. Applies immediately, no restart needed.";
        row.value = &field_of_view_;
        row.min = 40.0f;
        row.max = 120.0f;
        row.step = 2.0f;
        row.fmt = "%.0f";
        row.on_value_change = [this] {
          field_of_view_ = std::clamp(field_of_view_, 40.0f, 120.0f);
          rex::cvar::SetFlagByName("skate3_field_of_view", std::to_string(field_of_view_));
        };
        row.reset = [this] {
          field_of_view_ = float(CvarDefaultDouble("skate3_field_of_view", 60.0));
          rex::cvar::SetFlagByName("skate3_field_of_view", std::to_string(field_of_view_));
          SaveSimpleSettingsConfig(config_path_);
        };
        rows.push_back(std::move(row));
      }
      break;
    }
    case 1: {  // Controls
      header("Mouse & Keyboard");
      {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Mouse & Keyboard Mode";
        row.desc = "Emulates a controller from mouse and keyboard input.";
        row.options = {"Off", "On"};
        row.flag = &mnk_mode_;
        row.reset = [this] { mnk_mode_ = CvarDefaultBool("mnk_mode", false); };
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Capture Mouse";
        row.desc =
            "Locks the mouse cursor to the game window while Mouse & Keyboard Mode is "
            "active.";
        row.options = {"Off", "On"};
        row.flag = &mnk_capture_mouse_;
        row.reset = [this] {
          mnk_capture_mouse_ = CvarDefaultBool("mnk_capture_mouse", true);
        };
        rows.push_back(std::move(row));
      }
      break;
    }
    case 2: {  // Profile
      header("Local Profile");
      {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Profile";
        row.desc = "Select the local player profile used for saves.";
        for (const auto& profile : profiles_.profiles) {
          row.options.push_back(profile.gamertag);
        }
        row.index = &profiles_.selected_index;
        row.on_enum_change = [this](int index) {
          index = std::clamp(index, 0, static_cast<int>(profiles_.profiles.size()) - 1);
          CopyToBuffer(gamertag_buf_, sizeof(gamertag_buf_),
                       profiles_.profiles[index].gamertag);
          profile_signed_in_ = profiles_.profiles[index].signed_in;
        };
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kText;
        row.label = "Gamertag";
        row.desc = "Display name for this profile. Select to edit with the keyboard.";
        row.text_buf = gamertag_buf_;
        row.text_buf_size = sizeof(gamertag_buf_);
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kEnum;
        row.label = "Local Sign-in";
        row.desc = "Whether this profile is signed in when the game boots.";
        row.options = {"Signed out", "Signed in"};
        row.flag = &profile_signed_in_;
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kAction;
        row.label = "Save Profile";
        row.desc = "Save the profile changes above and apply them to the running game.";
        row.action = [this] { SaveProfile(); };
        rows.push_back(std::move(row));
      }
      break;
    }
    case 3: {  // System
      header("Session");
      {
        RowSpec row;
        row.kind = RowSpec::kAction;
        row.label = "Apply & Restart";
        row.desc = pending
                       ? "Save the pending changes and restart the game to apply them."
                       : "No pending changes. Adjust a setting first, then apply it here.";
        row.enabled = pending;
        row.action = [this] { ApplyAndRestart(); };
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kAction;
        row.label = "Revert Changes";
        row.desc = "Discard the pending changes and go back to the current settings.";
        row.enabled = pending;
        row.action = [this] { LoadSettingsFromCvars(); };
        rows.push_back(std::move(row));
      }
      {
        RowSpec row;
        row.kind = RowSpec::kAction;
        row.label = "Close Settings";
        row.desc = "Return to the game.";
        row.action = [this] { Hide(); };
        rows.push_back(std::move(row));
      }
      break;
    }
    default:
      break;
  }
}

SimpleSettingsDialog::NavIntents SimpleSettingsDialog::GatherInput(ImGuiIO& io) {
  NavIntents in;

  // ---- Gamepad ----
  SimpleSettingsGamepad pad;
  if (poll_gamepad_) {
    pad = poll_gamepad_();
  }
  // prev_pad_buttons_ starts 0xFFFF on Show so buttons held while opening
  // (e.g. the Back+Start chord) don't fire actions on the first frame.
  const uint16_t pressed = pad.buttons & ~prev_pad_buttons_;
  prev_pad_buttons_ = pad.buttons;

  int dir_x = 0;
  int dir_y = 0;
  if (pad.connected) {
    if (pad.buttons & kPadDpadLeft) {
      dir_x -= 1;
    }
    if (pad.buttons & kPadDpadRight) {
      dir_x += 1;
    }
    if (pad.buttons & kPadDpadUp) {
      dir_y -= 1;
    }
    if (pad.buttons & kPadDpadDown) {
      dir_y += 1;
    }
    constexpr int16_t kDeadzone = 12000;
    if (dir_x == 0) {
      dir_x = pad.thumb_lx <= -kDeadzone ? -1 : (pad.thumb_lx >= kDeadzone ? 1 : 0);
    }
    if (dir_y == 0) {
      // Stick up is positive Y in XInput; menu "up" is -1.
      dir_y = pad.thumb_ly >= kDeadzone ? -1 : (pad.thumb_ly <= -kDeadzone ? 1 : 0);
    }
  }

  auto repeat_axis = [&io](int dir, int& held, float& timer) -> int {
    if (dir == 0) {
      held = 0;
      timer = 0.0f;
      return 0;
    }
    if (dir != held) {
      held = dir;
      timer = 0.0f;
      return dir;  // initial press
    }
    timer += io.DeltaTime;
    if (timer >= kRepeatDelay) {
      timer -= kRepeatRate;
      return dir;
    }
    return 0;
  };
  in.move_x = repeat_axis(dir_x, held_dir_x_, repeat_timer_x_);
  in.move_y = repeat_axis(dir_y, held_dir_y_, repeat_timer_y_);

  if (pressed & kPadA) {
    in.select = true;
  }
  if (pressed & kPadB) {
    in.back = true;
  }
  if (pressed & kPadY) {
    in.reset_default = true;
  }
  if (pressed & kPadX) {
    in.apply_restart = true;
  }
  if (pressed & kPadLShoulder) {
    in.category_prev = true;
  }
  if (pressed & kPadRShoulder) {
    in.category_next = true;
  }
  if (pressed != 0 || dir_x != 0 || dir_y != 0) {
    pad_active_ = true;
  }

  // ---- Keyboard (Escape arrives via the keybind -> NavigateBack) ----
  if (!editing_text_) {
    bool kb = false;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
      in.move_y -= 1;
      kb = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
      in.move_y += 1;
      kb = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) {
      in.move_x -= 1;
      kb = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) {
      in.move_x += 1;
      kb = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
        ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
      in.select = true;
      kb = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
      in.category_prev = true;
      kb = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
      in.category_next = true;
      kb = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
      in.reset_default = true;
      kb = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
      in.apply_restart = true;
      kb = true;
    }
    if (kb) {
      pad_active_ = false;
    }
  }

  // Mouse motion switches the legend back to keyboard/mouse.
  if (std::abs(io.MousePos.x - mouse_x_) > 2.0f || std::abs(io.MousePos.y - mouse_y_) > 2.0f) {
    if (mouse_x_ >= 0.0f) {
      pad_active_ = false;
    }
  }

  return in;
}

void SimpleSettingsDialog::OnDraw(ImGuiIO& io) {
  if (!visible_) {
    return;
  }

  ImFont* font = imgui_drawer()->ui_font() ? imgui_drawer()->ui_font() : ImGui::GetFont();
  ImFont* bold = imgui_drawer()->ui_font_semibold() ? imgui_drawer()->ui_font_semibold() : font;
  // Coverage-thinned variants for dark text on light panels (rows, section
  // bars, legend chips) - matches the browser's per-polarity text gamma.
  ImFont* bold_ol = imgui_drawer()->ui_font_semibold_on_light()
                        ? imgui_drawer()->ui_font_semibold_on_light()
                        : bold;

  // Uniform scale: design metrics are authored for 1080p.
  const float s = std::clamp(io.DisplaySize.y / 1080.0f, 0.6f, 3.0f);

  // Quantize font sizes so the EM (size * upm / (hhea ascent - descent) -
  // the browser's font-size) lands on whole pixels. A fractional em renders
  // measurably wider letter spacing than the browser reference, which snaps
  // css font sizes to the device grid (+1.3% run width at 4K, 370px vs
  // 365px on "Vertical Synchronisation").
  constexpr float kEmPerSize = 2048.0f / 2478.0f;  // Inter upm / (asc - desc)
  auto font_px = [](float size) {
    return std::round(size * kEmPerSize) / kEmPerSize;
  };

  if (just_shown_) {
    // Ignore the pre-open cursor position delta (opening warps/reveals the
    // cursor): it otherwise reads as motion and steals focus from the rail
    // to whatever row happens to sit under the cursor.
    mouse_x_ = io.MousePos.x;
    mouse_y_ = io.MousePos.y;
    just_shown_ = false;
  }

  // ---- Input ----
  NavIntents in = GatherInput(io);
  if (editing_text_) {
    // The text field owns navigation; pad B cancels the edit.
    if (in.back) {
      editing_text_ = false;
    }
    in = NavIntents{};
  }

  const int category_count = static_cast<int>(kCategories.size());
  // The rail holds the categories plus a pinned Close Game entry at the
  // bottom (rail_sel_ == category_count).
  const int quit_rail_index = category_count;
  if (in.category_prev || in.category_next) {
    category_ = (category_ + (in.category_next ? 1 : category_count - 1)) % category_count;
    rail_sel_ = category_;
    row_index_ = 0;
    content_scroll_ = 0.0f;
    highlight_anim_y_ = -1.0f;
  }
  if (zone_ == FocusZone::kRail && in.move_y != 0) {
    rail_sel_ = std::clamp(rail_sel_ + in.move_y, 0, quit_rail_index);
    if (rail_sel_ < category_count && rail_sel_ != category_) {
      category_ = rail_sel_;
      row_index_ = 0;
      content_scroll_ = 0.0f;
      highlight_anim_y_ = -1.0f;
    }
  }

  std::vector<RowSpec> rows;
  BuildRows(rows, category_);

  // Selectable row indices (headers and disabled rows are skipped by nav).
  std::vector<int> selectable;
  selectable.reserve(rows.size());
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    if (rows[i].kind != RowSpec::kHeader && rows[i].enabled) {
      selectable.push_back(i);
    }
  }
  auto clamp_focus_to_selectable = [&]() {
    if (selectable.empty()) {
      row_index_ = -1;
      return;
    }
    int best = selectable[0];
    int best_delta = 1 << 30;
    for (int idx : selectable) {
      int delta = std::abs(idx - row_index_);
      if (delta < best_delta) {
        best = idx;
        best_delta = delta;
      }
    }
    row_index_ = best;
  };
  clamp_focus_to_selectable();

  auto enum_value = [](const RowSpec& row) -> int {
    return row.flag ? (*row.flag ? 1 : 0) : (row.index ? *row.index : 0);
  };
  auto set_enum_value = [](RowSpec& row, int value) {
    if (row.flag) {
      *row.flag = value != 0;
    } else if (row.index) {
      *row.index = value;
    }
    if (row.on_enum_change) {
      row.on_enum_change(value);
    }
  };
  // Range semantics: directional stepping (dpad/arrows/chevrons)
  // CLAMPS at the ends - the matching chevron greys out there - while
  // activation (A/Enter/row click) cycles with wrap-around.
  auto step_row = [&](RowSpec& row, int dir) {
    if (row.kind == RowSpec::kEnum) {
      int count = static_cast<int>(row.options.size());
      if (count <= 0) {
        return;
      }
      int value = std::clamp(enum_value(row), 0, count - 1);
      set_enum_value(row, std::clamp(value + dir, 0, count - 1));
    } else if (row.kind == RowSpec::kSlider && row.value) {
      *row.value = std::clamp(*row.value + dir * row.step, row.min, row.max);
      if (row.on_value_change) {
        row.on_value_change();
      }
      SaveSimpleSettingsConfig(config_path_);
    }
  };
  auto activate_row = [&](RowSpec& row) {
    if (!row.enabled) {
      return;
    }
    switch (row.kind) {
      case RowSpec::kEnum: {
        int count = static_cast<int>(row.options.size());
        if (count > 0) {
          int value = std::clamp(enum_value(row), 0, count - 1);
          set_enum_value(row, (value + 1) % count);
        }
        break;
      }
      case RowSpec::kAction:
        if (row.action) {
          row.action();
        }
        break;
      case RowSpec::kText:
        editing_text_ = true;
        text_edit_focus_pending_ = true;
        break;
      default:
        break;
    }
  };

  // ---- Apply navigation intents ----
  bool row_activated_this_frame = false;
  if (zone_ == FocusZone::kRail) {
    if (rail_sel_ == quit_rail_index) {
      if (in.select && close_game_) {
        Hide();
        close_game_();
      }
    } else if ((in.select || in.move_x > 0) && !selectable.empty()) {
      zone_ = FocusZone::kContent;
      row_index_ = selectable[0];
    }
    if (in.back) {
      Hide();
    }
  } else {
    if (in.move_y != 0 && !selectable.empty()) {
      int pos = 0;
      for (int i = 0; i < static_cast<int>(selectable.size()); ++i) {
        if (selectable[i] == row_index_) {
          pos = i;
          break;
        }
      }
      pos = std::clamp(pos + in.move_y, 0, static_cast<int>(selectable.size()) - 1);
      row_index_ = selectable[pos];
    }
    if (in.move_x != 0 && row_index_ >= 0) {
      step_row(rows[row_index_], in.move_x);
    }
    if (in.select && row_index_ >= 0) {
      activate_row(rows[row_index_]);
      row_activated_this_frame = true;
    }
    if (in.back) {
      zone_ = FocusZone::kRail;
    }
  }
  if (in.reset_default && zone_ == FocusZone::kContent && row_index_ >= 0 &&
      rows[row_index_].reset) {
    rows[row_index_].reset();
  }
  if (in.apply_restart && HasSettingsChanges()) {
    ApplyAndRestart();
  }
  if (!visible_) {
    return;  // an action above closed the dialog
  }
  // Actions can invalidate row layout (pending-state rows); rebuild.
  if (row_activated_this_frame || in.reset_default || in.apply_restart) {
    BuildRows(rows, category_);
    selectable.clear();
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      if (rows[i].kind != RowSpec::kHeader && rows[i].enabled) {
        selectable.push_back(i);
      }
    }
    clamp_focus_to_selectable();
  }

  const bool pending = HasSettingsChanges();

  // ---- Layout ----
  // Positional metrics are snapped to whole pixels (see Snap).
  const float margin_x = Snap(std::max(56.0f * s, io.DisplaySize.x * 0.05f));
  const float title_y = Snap(io.DisplaySize.y * 0.13f);
  const float title_size = font_px(42.0f * s);
  const float columns_y = Snap(title_y + 74.0f * s);
  // Rail and description columns are equal-width.
  const float rail_w = Snap(300.0f * s);
  const float desc_w = Snap(300.0f * s);
  const float col_gap = Snap(14.0f * s);
  const float footer_h = Snap(96.0f * s);
  const float content_bottom = Snap(io.DisplaySize.y - footer_h - 18.0f * s);
  float content_w = io.DisplaySize.x - 2.0f * margin_x - rail_w - desc_w - 2.0f * col_gap;
  content_w = Snap(std::clamp(content_w, 300.0f * s, 800.0f * s));
  // The column widths are capped, so on wide displays the block would hug the
  // left margin - center the whole menu instead.
  const float menu_total_w = rail_w + content_w + desc_w + 2.0f * col_gap;
  const float rail_x = Snap(std::max(margin_x, (io.DisplaySize.x - menu_total_w) * 0.5f));
  const float content_x = rail_x + rail_w + col_gap;
  const float desc_x = content_x + content_w + col_gap;
  const float menu_right = desc_x + desc_w;

  const float row_h = Snap(52.0f * s);
  const float header_h = row_h;  // section bars match setting rows
  // Gap is 1 design px tighter than the 6*s focus ring, so the ring slightly
  // overlaps neighbours - it draws above them.
  const float row_gap = Snap(5.0f * s);
  const float rail_item_h = Snap(52.0f * s);
  const float label_size = font_px(19.0f * s);
  const float value_size = font_px(19.0f * s);
  const float desc_size = font_px(18.0f * s);
  // Description panel height - also anchors Close Game's bottom edge.
  const float desc_panel_h =
      Snap(std::min(content_bottom - columns_y, 320.0f * s));

  const ImVec2 mouse = io.MousePos;
  const bool mouse_moved =
      std::abs(mouse.x - mouse_x_) > 2.0f || std::abs(mouse.y - mouse_y_) > 2.0f;
  mouse_x_ = mouse.x;
  mouse_y_ = mouse.y;
  const bool clicked = ImGui::IsMouseClicked(0);
  auto mouse_in = [&mouse](float x0, float y0, float x1, float y1) {
    return mouse.x >= x0 && mouse.x < x1 && mouse.y >= y0 && mouse.y < y1;
  };

  // ---- Window ----
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  if (!ImGui::Begin("##skate3_settings_overlay", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
    ImGui::End();
    ImGui::PopStyleVar(2);
    return;
  }
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // ---- Backdrop: uniform slightly-dark tint over the whole screen. The
  // native renderer blurs the finished frame while the menu is open
  // (SetSettingsMenuBlur), so the tint lands on a frosted scene.
  dl->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(6, 9, 11, 133));

  // ---- Title ----
  dl->AddText(bold, title_size, ImVec2(Snap(rail_x), title_y), kColText, "Settings");
  if (pending) {
    const char* chip_text = "RESTART REQUIRED TO APPLY";
    float chip_size = font_px(14.0f * s);
    ImVec2 extent = bold->CalcTextSizeA(chip_size, FLT_MAX, 0.0f, chip_text);
    float chip_pad = 10.0f * s;
    float chip_x1 = menu_right;
    float chip_x0 = Snap(chip_x1 - extent.x - 2.0f * chip_pad);
    float chip_y0 = Snap(title_y + title_size * 0.5f - extent.y * 0.5f - 6.0f * s);
    float chip_y1 = Snap(chip_y0 + extent.y + 12.0f * s);
    dl->AddRect(ImVec2(chip_x0, chip_y0), ImVec2(chip_x1, chip_y1), kColWarn, 0.0f, 0,
                1.5f);
    dl->AddText(bold, chip_size, ImVec2(Snap(chip_x0 + chip_pad), Snap(chip_y0 + 6.0f * s)),
                kColWarn, chip_text);
  }
  // ---- Left rail ----
  float rail_focus_target = -1.0f;
  for (int i = 0; i < category_count; ++i) {
    float y0 = columns_y + i * (rail_item_h + row_gap);
    float y1 = y0 + rail_item_h;
    bool is_current = category_ == i;
    bool hovered = mouse_in(rail_x, y0, rail_x + rail_w, y1);
    if (hovered && clicked) {
      category_ = i;
      rail_sel_ = i;
      zone_ = FocusZone::kRail;
      row_index_ = 0;
      content_scroll_ = 0.0f;
      is_current = true;
    }
    if (is_current && zone_ == FocusZone::kRail && rail_sel_ == i) {
      rail_focus_target = y0;
    }
    // The rounded focus fill covers this item while rail-focused; skip the
    // square panel underneath so its corners don't poke past the radius.
    bool rail_focused = zone_ == FocusZone::kRail && rail_sel_ == i;
    if (!rail_focused) {
      ImU32 bg = is_current ? kColSelFill : (hovered ? kColRailPanelHover : kColRailPanel);
      dl->AddRectFilled(ImVec2(rail_x, y0), ImVec2(rail_x + rail_w, y1), bg, 0.0f);
      dl->AddRect(ImVec2(rail_x, y0), ImVec2(rail_x + rail_w, y1), kColRailBorder, 0.0f);
    }
  }
  // Focused rail item: sliding black fill with the lime focus ring, drawn
  // over the panels, then labels.
  if (rail_focus_target >= 0.0f) {
    if (rail_anim_y_ < 0.0f || std::abs(rail_anim_y_ - rail_focus_target) > 160.0f * s) {
      rail_anim_y_ = rail_focus_target;
    }
    rail_anim_y_ += (rail_focus_target - rail_anim_y_) * std::min(1.0f, io.DeltaTime * 22.0f);
    const float fill_y = Snap(rail_anim_y_);
    DrawFocusHighlight(dl, ImVec2(rail_x, fill_y),
                       ImVec2(rail_x + rail_w, fill_y + rail_item_h), s);
  } else {
    rail_anim_y_ = -1.0f;
  }
  for (int i = 0; i < category_count; ++i) {
    float y0 = columns_y + i * (rail_item_h + row_gap);
    bool is_current = category_ == i;
    bool focused = is_current && zone_ == FocusZone::kRail && rail_sel_ == i;
    ImU32 text_col = focused ? kColSelText : (is_current ? kColText : kColTextDim);
    AddTextVCentered(dl, bold, label_size, rail_x + 20.0f * s,
                     y0 + rail_item_h * 0.5f, text_col, kCategories[i].name);
    if (is_current && zone_ == FocusZone::kContent) {
      // Arrow hinting that focus lives in the content column.
      float ax = rail_x + rail_w - 18.0f * s;
      float ay = y0 + rail_item_h * 0.5f;
      dl->AddTriangleFilled(ImVec2(ax, ay - 6.0f * s), ImVec2(ax, ay + 6.0f * s),
                            ImVec2(ax + 8.0f * s, ay), kColAccent);
    }
  }

  // Close Game entry: below the categories, set apart by an extra gap.
  {
    // Bottom edge aligned with the description panel's bottom.
    float y0 = Snap(columns_y + desc_panel_h - rail_item_h);
    float y1 = y0 + rail_item_h;
    bool focused = zone_ == FocusZone::kRail && rail_sel_ == quit_rail_index;
    bool hovered = mouse_in(rail_x, y0, rail_x + rail_w, y1);
    if (hovered && clicked && close_game_) {
      Hide();
      close_game_();
    }
    ImU32 bg = focused ? kColDanger : (hovered ? kColRailPanelHover : kColRailPanel);
    dl->AddRectFilled(ImVec2(rail_x, y0), ImVec2(rail_x + rail_w, y1), bg, 0.0f);
    dl->AddRect(ImVec2(rail_x, y0), ImVec2(rail_x + rail_w, y1),
                focused ? kColDanger : kColRailBorder, 0.0f);
    AddTextVCentered(dl, bold, label_size, rail_x + 20.0f * s, (y0 + y1) * 0.5f,
                     focused ? IM_COL32(255, 250, 249, 255) : kColDanger, "Close Game");
  }

  // ---- Content column ----
  // Row geometry (in unscrolled space).
  std::vector<float> row_y(rows.size());
  std::vector<float> row_hgt(rows.size());
  {
    float y = 0.0f;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      row_y[i] = y;
      row_hgt[i] = rows[i].kind == RowSpec::kHeader ? header_h : row_h;
      y += row_hgt[i] + row_gap;
    }
  }
  const float content_view_h = content_bottom - columns_y;
  const float rows_total_h = rows.empty() ? 0.0f : row_y.back() + row_hgt.back();
  const float max_scroll = std::max(0.0f, rows_total_h - content_view_h);

  // Wheel scroll while over the content column.
  if (io.MouseWheel != 0.0f &&
      mouse_in(content_x, columns_y, content_x + content_w, content_bottom)) {
    content_scroll_ -= io.MouseWheel * 60.0f * s;
  }
  // Keep the focused row in view when navigating.
  if (zone_ == FocusZone::kContent && row_index_ >= 0 && (in.move_y != 0 || in.select)) {
    float y0 = row_y[row_index_];
    float y1 = y0 + row_hgt[row_index_];
    if (y0 - content_scroll_ < 0.0f) {
      content_scroll_ = y0;
    } else if (y1 - content_scroll_ > content_view_h) {
      content_scroll_ = y1 - content_view_h;
    }
  }
  content_scroll_ = std::clamp(content_scroll_, 0.0f, max_scroll);
  const float scroll = Snap(content_scroll_);

  // Clip window padded by the focus-ring extent (6*s) so the ring can sit
  // fully OUTSIDE the focused row without being cut off at the edges.
  const float ring_pad = 6.0f * s;
  dl->PushClipRect(ImVec2(content_x - ring_pad, columns_y - ring_pad),
                   ImVec2(content_x + content_w + ring_pad + 1.0f, content_bottom + ring_pad),
                   true);

  // Sliding selection highlight (white fill under the focused row).
  const bool content_focus = zone_ == FocusZone::kContent && row_index_ >= 0;
  if (content_focus) {
    float target = row_y[row_index_];
    if (highlight_anim_y_ < 0.0f || std::abs(highlight_anim_y_ - target) > 160.0f * s) {
      highlight_anim_y_ = target;
    }
    highlight_anim_y_ += (target - highlight_anim_y_) * std::min(1.0f, io.DeltaTime * 22.0f);
  } else {
    highlight_anim_y_ = -1.0f;
  }

  const float value_w = std::min(330.0f * s, content_w * 0.46f);
  const float chevron_r = 13.0f * s;

  // Backgrounds first, then per-row content in TWO phases around the sliding
  // highlight: non-focused content below it, the focused row's content above
  // it - the ring overlaps neighbours (gap is 1 design px tighter than the
  // ring) and must read as the forefront element.
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const RowSpec& row = rows[i];
    if (row.kind == RowSpec::kHeader) {
      continue;
    }
    float y0 = columns_y + Snap(row_y[i]) - scroll;
    float y1 = y0 + row_hgt[i];
    if (y1 < columns_y || y0 > content_bottom) {
      continue;
    }
    bool hovered = mouse_in(content_x, y0, content_x + content_w, y1);
    if (hovered && mouse_moved && row.enabled) {
      zone_ = FocusZone::kContent;
      row_index_ = i;
    }
    // The rounded highlight covers the focused row's panel entirely; skip it
    // so its square corners don't poke past the radius.
    if (content_focus && row_index_ == i) {
      continue;
    }
    ImU32 bg = hovered && row.enabled ? kColPanelHover : kColPanel;
    dl->AddRectFilled(ImVec2(content_x, y0), ImVec2(content_x + content_w, y1), bg, 0.0f);
    dl->AddRect(ImVec2(content_x, y0), ImVec2(content_x + content_w, y1), kColPanelBorder,
                0.0f);
  }

  auto draw_row_content = [&](int i) {
    RowSpec& row = rows[i];
    float y0 = columns_y + Snap(row_y[i]) - scroll;
    float y1 = y0 + row_hgt[i];
    if (y1 < columns_y || y0 > content_bottom) {
      return;
    }
    float cy = (y0 + y1) * 0.5f;

    if (row.kind == RowSpec::kHeader) {
      // Section header: solid accent bar spanning the content
      // column, dark bold text, label column-aligned with the row labels.
      dl->AddRectFilled(ImVec2(content_x, y0), ImVec2(content_x + content_w, y1), kColAccent,
                        0.0f);
      AddTextVCentered(dl, bold_ol, label_size, content_x + 18.0f * s, cy, kColAccentDark,
                       row.label);
      return;
    }

    const bool focused = content_focus && row_index_ == i;
    const bool hovered = mouse_in(content_x, y0, content_x + content_w, y1);
    ImFont* row_bold = focused ? bold : bold_ol;  // polarity-matched weight
    ImU32 label_col = focused ? kColSelText
                              : (!row.enabled ? kColTextFaint
                                              : (row.danger ? kColDanger : kColRowText));
    ImU32 value_col = focused ? kColSelText : (row.enabled ? kColRowText : kColTextFaint);

    // Label.
    AddTextVCentered(dl, row_bold, label_size, content_x + 18.0f * s, cy, label_col, row.label);

    // Value area on the right side of the row.
    float vx1 = content_x + content_w - 12.0f * s;
    float vx0 = content_x + content_w - value_w;

    switch (row.kind) {
      case RowSpec::kEnum: {
        int count = static_cast<int>(row.options.size());
        int value = count > 0 ? std::clamp(enum_value(row), 0, count - 1) : 0;
        ImVec2 left_center(vx0 + chevron_r, cy);
        ImVec2 right_center(vx1 - chevron_r, cy);
        // Magenta filled stepper circles with white triangles;
        // a circle greys out (and stops responding) at its end of the range.
        const bool can_left = row.enabled && value > 0;
        const bool can_right = row.enabled && value < count - 1;
        bool left_hover = can_left &&
                          mouse_in(left_center.x - chevron_r - 4.0f, cy - chevron_r - 4.0f,
                                   left_center.x + chevron_r + 4.0f, cy + chevron_r + 4.0f);
        bool right_hover = can_right &&
                           mouse_in(right_center.x - chevron_r - 4.0f, cy - chevron_r - 4.0f,
                                    right_center.x + chevron_r + 4.0f, cy + chevron_r + 4.0f);
        const ImU32 chev_disabled = focused ? kColChevDisabledOnDark : kColChevDisabled;
        DrawChevron(dl, left_center, chevron_r, true,
                    can_left ? (left_hover ? kColInteractHover : kColInteract)
                             : chev_disabled,
                    kColWhite, true);
        DrawChevron(dl, right_center, chevron_r, false,
                    can_right ? (right_hover ? kColInteractHover : kColInteract)
                              : chev_disabled,
                    kColWhite, true);
        if (count > 0) {
          float text_max_w = (right_center.x - chevron_r) - (left_center.x + chevron_r) -
                             16.0f * s;
          std::string text =
              TruncateToWidth(row_bold, value_size, text_max_w, row.options[value]);
          AddTextCentered(dl, row_bold, value_size,
                          ImVec2((left_center.x + right_center.x) * 0.5f, cy), value_col,
                          text.c_str());
        }
        if (clicked && row.enabled) {
          if (left_hover) {
            step_row(row, -1);
          } else if (right_hover) {
            step_row(row, 1);
          } else if (hovered) {
            activate_row(row);  // row-body click cycles with wrap-around
          }
        }
        break;
      }
      case RowSpec::kSlider: {
        float number_w = 52.0f * s;
        float track_x0 = vx0 + 6.0f * s;
        float track_x1 = vx1 - number_w;
        float t = row.max > row.min ? (*row.value - row.min) / (row.max - row.min) : 0.0f;
        float knob_x = track_x0 + t * (track_x1 - track_x0);
        ImU32 track_col = focused ? IM_COL32(255, 255, 255, 77) : IM_COL32(0, 0, 0, 56);
        dl->AddRectFilled(ImVec2(track_x0, cy - 2.0f * s), ImVec2(track_x1, cy + 2.0f * s),
                          track_col, 0.0f);
        dl->AddRectFilled(ImVec2(track_x0, cy - 2.0f * s), ImVec2(knob_x, cy + 2.0f * s),
                          kColInteract, 0.0f);
        dl->AddCircleFilled(ImVec2(Snap(knob_x), Snap(cy)), 8.0f * s, kColInteract, 24);
        char value_text[32];
        std::snprintf(value_text, sizeof(value_text), row.fmt, double(*row.value));
        ImVec2 extent = row_bold->CalcTextSizeA(value_size, FLT_MAX, 0.0f, value_text);
        dl->AddText(row_bold, value_size, ImVec2(Snap(vx1 - extent.x), Snap(cy - extent.y * 0.5f)),
                    value_col, value_text);
        // Mouse drag on the track.
        ImGui::SetCursorScreenPos(ImVec2(track_x0 - 10.0f * s, cy - 14.0f * s));
        char slider_id[48];
        std::snprintf(slider_id, sizeof(slider_id), "##slider_%d_%d", category_, i);
        ImGui::InvisibleButton(slider_id,
                               ImVec2(track_x1 - track_x0 + 20.0f * s, 28.0f * s));
        if (ImGui::IsItemActive() && row.enabled) {
          float nt = std::clamp((mouse.x - track_x0) / std::max(1.0f, track_x1 - track_x0),
                                0.0f, 1.0f);
          float new_value = row.min + nt * (row.max - row.min);
          // Quantize to step for tidy values.
          new_value = row.min + std::round((new_value - row.min) / row.step) * row.step;
          new_value = std::clamp(new_value, row.min, row.max);
          if (new_value != *row.value) {
            *row.value = new_value;
            if (row.on_value_change) {
              row.on_value_change();
            }
          }
          zone_ = FocusZone::kContent;
          row_index_ = i;
        }
        if (ImGui::IsItemDeactivated()) {
          SaveSimpleSettingsConfig(config_path_);
        }
        break;
      }
      case RowSpec::kAction: {
        // No right-side 'Select' / '-' hint: the footer legend covers
        // activation, and disabled rows read from their dim label alone.
        if (clicked && hovered && row.enabled && !row_activated_this_frame) {
          activate_row(row);
          row_activated_this_frame = true;
        }
        break;
      }
      case RowSpec::kText: {
        const bool editing_this = editing_text_ && focused;
        if (editing_this) {
          ImGui::SetCursorScreenPos(ImVec2(vx0 + 6.0f * s, cy - 14.0f * s));
          ImGui::SetNextItemWidth(vx1 - vx0 - 12.0f * s);
          // value_size is already physical; divide out the global widget-font
          // DPI scale (it exists for logical-unit dialogs, see ImGuiDrawer).
          ImGui::PushFont(font, value_size / std::max(0.01f, ImGui::GetStyle().FontScaleDpi));
          // Edit field sits on the BLACK focused row: light text, light frame.
          ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, 0.15f));
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(0.98f, 0.99f, 0.99f, 1.0f));
          if (text_edit_focus_pending_) {
            ImGui::SetKeyboardFocusHere();
            text_edit_focus_pending_ = false;
          }
          bool committed = ImGui::InputText("##settings_text_edit", row.text_buf,
                                            row.text_buf_size,
                                            ImGuiInputTextFlags_EnterReturnsTrue);
          if (committed || (!ImGui::IsItemActive() && !text_edit_focus_pending_ &&
                            ImGui::IsItemDeactivated())) {
            editing_text_ = false;
          }
          ImGui::PopStyleColor(2);
          ImGui::PopFont();
        } else {
          const char* text = row.text_buf && row.text_buf[0] ? row.text_buf : "-";
          ImVec2 extent = row_bold->CalcTextSizeA(value_size, FLT_MAX, 0.0f, text);
          dl->AddText(row_bold, value_size, ImVec2(Snap(vx1 - extent.x), Snap(cy - extent.y * 0.5f)),
                      value_col, text);
          if (clicked && hovered && row.enabled) {
            zone_ = FocusZone::kContent;
            row_index_ = i;
            editing_text_ = true;
            text_edit_focus_pending_ = true;
          }
        }
        break;
      }
      default:
        break;
    }
  };
  // Phase 1: everything except the focused row.
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    if (!(content_focus && row_index_ == i)) {
      draw_row_content(i);
    }
  }
  // Phase 2: the sliding highlight above neighbours...
  if (content_focus && highlight_anim_y_ >= 0.0f) {
    float y0 = columns_y + Snap(highlight_anim_y_) - scroll;
    DrawFocusHighlight(dl, ImVec2(content_x, y0), ImVec2(content_x + content_w, y0 + row_h),
                       s);
  }
  // ...phase 3: the focused row's own content above the highlight.
  if (content_focus && row_index_ >= 0 && row_index_ < static_cast<int>(rows.size())) {
    draw_row_content(row_index_);
  }
  dl->PopClipRect();

  // Scrollbar hint when the list overflows.
  if (max_scroll > 0.0f) {
    float bar_x = content_x + content_w + 4.0f * s;
    float track_h = content_view_h;
    float bar_h = std::max(24.0f * s, track_h * (content_view_h / rows_total_h));
    float bar_y = columns_y + (track_h - bar_h) * (content_scroll_ / max_scroll);
    dl->AddRectFilled(ImVec2(bar_x, columns_y), ImVec2(bar_x + 3.0f * s, columns_y + track_h),
                      IM_COL32(255, 255, 255, 18), 0.0f);
    dl->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + 3.0f * s, bar_y + bar_h),
                      IM_COL32(255, 255, 255, 90), 0.0f);
  }

  // ---- Description panel ----
  {
    const float panel_h = desc_panel_h;
    const float header_bar_h = row_h;  // header bar matches a settings row
    dl->AddRectFilled(ImVec2(desc_x, columns_y), ImVec2(desc_x + desc_w, columns_y + header_bar_h),
                      kColAccent, 0.0f);
    AddTextVCentered(dl, bold_ol, label_size, desc_x + 18.0f * s, columns_y + header_bar_h * 0.5f,
                     kColAccentDark, "Description");
    dl->AddRectFilled(ImVec2(desc_x, columns_y + header_bar_h),
                      ImVec2(desc_x + desc_w, columns_y + panel_h), kColDescPanel, 0.0f);
    dl->AddRect(ImVec2(desc_x, columns_y), ImVec2(desc_x + desc_w, columns_y + panel_h),
                kColRailBorder, 0.0f);

    const char* desc_text = kCategories[category_].desc;
    if (zone_ == FocusZone::kRail && rail_sel_ == quit_rail_index) {
      desc_text = "Quit to the desktop. Unsaved game progress is lost.";
    }
    const char* note_text = nullptr;
    const std::string* extra_text = nullptr;
    if (zone_ == FocusZone::kContent && row_index_ >= 0 &&
        row_index_ < static_cast<int>(rows.size())) {
      const RowSpec& row = rows[row_index_];
      if (row.desc) {
        desc_text = row.desc;
      }
      note_text = row.value_note;
      if (!row.desc_extra.empty()) {
        extra_text = &row.desc_extra;
      }
    }
    float text_x = Snap(desc_x + 14.0f * s);
    float text_y = Snap(columns_y + header_bar_h + 14.0f * s);
    float wrap_w = desc_w - 28.0f * s;
    text_y = Snap(text_y +
                  AddTextJustified(dl, font, desc_size, ImVec2(text_x, text_y), wrap_w,
                                   kColText, desc_text) +
                  10.0f * s);
    if (extra_text) {
      text_y = Snap(text_y +
                    AddTextJustified(dl, font, desc_size, ImVec2(text_x, text_y), wrap_w,
                                     kColTextDim, extra_text->c_str()) +
                    10.0f * s);
    }
    if (note_text) {
      text_y = Snap(text_y +
                    AddTextJustified(dl, bold, desc_size, ImVec2(text_x, text_y), wrap_w,
                                     kColWarn, note_text) +
                    10.0f * s);
    }
    if (pending) {
      AddTextJustified(dl, font, desc_size, ImVec2(text_x, text_y), wrap_w, kColWarn,
                       "Changes are applied after a restart. Use Apply & Restart when ready.");
    }
  }

  // ---- Footer button legend ----
  {
    float legend_y = Snap(io.DisplaySize.y - footer_h + 14.0f * s);
    std::vector<LegendGlyph> glyphs;
    if (pad_active_) {
      glyphs.push_back({"A", "Select", true});
      glyphs.push_back({"B", "Back", true});
      glyphs.push_back({"Y", "Reset to Default", true});
      glyphs.push_back({"LB / RB", "Category", false});
      if (pending) {
        glyphs.push_back({"X", "Apply & Restart", true});
      }
    } else {
      glyphs.push_back({"Enter", "Select", false});
      glyphs.push_back({"Esc", "Back", false});
      glyphs.push_back({"R", "Reset to Default", false});
      glyphs.push_back({"Q / E", "Category", false});
      if (pending) {
        glyphs.push_back({"F", "Apply & Restart", false});
      }
    }
    float x = rail_x;
    float glyph_size = font_px(15.0f * s);
    float label_text_size = font_px(16.0f * s);
    float chip_h = Snap(26.0f * s);
    for (const LegendGlyph& glyph : glyphs) {
      ImVec2 glyph_extent = bold->CalcTextSizeA(glyph_size, FLT_MAX, 0.0f, glyph.glyph);
      float cy = legend_y + chip_h * 0.5f;
      x = Snap(x);
      if (glyph.circle) {
        float r = chip_h * 0.5f;
        dl->AddCircleFilled(ImVec2(x + r, cy), r, kColLegendChip, 28);
        AddTextCenteredCap(dl, bold_ol, glyph_size, ImVec2(x + r, cy), kColLegendText,
                           glyph.glyph, /*ink_x=*/true);
        x += 2.0f * r + 8.0f * s;
      } else {
        float chip_w = Snap(glyph_extent.x + 18.0f * s);
        dl->AddRectFilled(ImVec2(x, legend_y), ImVec2(x + chip_w, legend_y + chip_h),
                          kColLegendChip, 4.0f * s);
        AddTextCenteredCap(dl, bold_ol, glyph_size, ImVec2(x + chip_w * 0.5f, cy),
                           kColLegendText, glyph.glyph, /*ink_x=*/false);
        x += chip_w + 8.0f * s;
      }
      ImVec2 label_extent = bold->CalcTextSizeA(label_text_size, FLT_MAX, 0.0f, glyph.label);
      dl->AddText(bold, label_text_size, ImVec2(Snap(x), Snap(cy - label_extent.y * 0.5f)),
                  kColLegendLabel, glyph.label);
      x += label_extent.x + 26.0f * s;
    }
  }

  ImGui::End();
  ImGui::PopStyleVar(2);
}

}  // namespace rex::ui
