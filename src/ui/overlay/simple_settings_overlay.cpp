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

constexpr ImU32 kColSelFill = IM_COL32(243, 246, 247, 255);
constexpr ImU32 kColSelText = IM_COL32(13, 16, 18, 255);
constexpr ImU32 kColPanel = IM_COL32(15, 19, 22, 216);
constexpr ImU32 kColPanelHover = IM_COL32(30, 37, 41, 230);
constexpr ImU32 kColPanelBorder = IM_COL32(255, 255, 255, 14);
constexpr ImU32 kColText = IM_COL32(228, 233, 235, 255);
constexpr ImU32 kColTextDim = IM_COL32(148, 157, 161, 255);
constexpr ImU32 kColTextFaint = IM_COL32(105, 114, 118, 255);
constexpr ImU32 kColAccent = IM_COL32(62, 219, 190, 255);
constexpr ImU32 kColAccentDark = IM_COL32(9, 24, 21, 255);
constexpr ImU32 kColDanger = IM_COL32(233, 88, 76, 255);
constexpr ImU32 kColWarn = IM_COL32(245, 197, 87, 255);
constexpr ImU32 kColDescPanel = IM_COL32(11, 15, 17, 220);
constexpr ImU32 kColRule = IM_COL32(255, 255, 255, 26);

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
  dl->AddText(font, size, ImVec2(center.x - extent.x * 0.5f, center.y - extent.y * 0.5f), col,
              text);
}

void AddTextVCentered(ImDrawList* dl, ImFont* font, float size, float x, float center_y,
                      ImU32 col, const char* text) {
  ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
  dl->AddText(font, size, ImVec2(x, center_y - extent.y * 0.5f), col, text);
}

// Stepper chevron: a triangle in a circle.
void DrawChevron(ImDrawList* dl, ImVec2 center, float radius, bool points_left, ImU32 circle_col,
                 ImU32 tri_col, bool filled) {
  if (filled) {
    dl->AddCircleFilled(center, radius, circle_col, 24);
  } else {
    dl->AddCircle(center, radius, circle_col, 24, 1.5f);
  }
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

  auto header = [&rows](const char* label) {
    RowSpec row;
    row.kind = RowSpec::kHeader;
    row.label = label;
    rows.push_back(std::move(row));
  };

  switch (category) {
    case 0: {  // Video
      header("DISPLAY");
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
        header("GAMEPLAY");
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
      header("MOUSE & KEYBOARD");
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
      header("LOCAL PROFILE");
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
      header("SESSION");
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

  // Uniform scale: design metrics are authored for 1080p.
  const float s = std::clamp(io.DisplaySize.y / 1080.0f, 0.6f, 3.0f);

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
  auto step_row = [&](RowSpec& row, int dir) {
    if (row.kind == RowSpec::kEnum) {
      int count = static_cast<int>(row.options.size());
      if (count <= 0) {
        return;
      }
      int value = ((enum_value(row) % count) + count) % count;
      value = ((value + dir) % count + count) % count;
      set_enum_value(row, value);
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
      case RowSpec::kEnum:
        step_row(row, 1);
        break;
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
  const float margin_x = std::max(56.0f * s, io.DisplaySize.x * 0.05f);
  const float title_y = io.DisplaySize.y * 0.085f;
  const float title_size = 42.0f * s;
  const float columns_y = title_y + 74.0f * s;
  const float rail_w = 250.0f * s;
  const float desc_w = 330.0f * s;
  const float col_gap = 14.0f * s;
  const float footer_h = 96.0f * s;
  const float content_bottom = io.DisplaySize.y - footer_h - 18.0f * s;
  float content_w = io.DisplaySize.x - 2.0f * margin_x - rail_w - desc_w - 2.0f * col_gap;
  content_w = std::clamp(content_w, 300.0f * s, 800.0f * s);
  // The column widths are capped, so on wide displays the block would hug the
  // left margin - center the whole menu instead.
  const float menu_total_w = rail_w + content_w + desc_w + 2.0f * col_gap;
  const float rail_x = std::max(margin_x, (io.DisplaySize.x - menu_total_w) * 0.5f);
  const float content_x = rail_x + rail_w + col_gap;
  const float desc_x = content_x + content_w + col_gap;
  const float menu_right = desc_x + desc_w;

  const float row_h = 52.0f * s;
  const float header_h = 36.0f * s;
  const float row_gap = 6.0f * s;
  const float rail_item_h = 52.0f * s;
  const float label_size = 19.0f * s;
  const float value_size = 19.0f * s;
  const float header_size = 14.5f * s;
  const float desc_size = 16.5f * s;

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

  // ---- Backdrop scrim: even dim plus a stronger wash behind the columns ----
  dl->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(6, 9, 11, 150),
                              IM_COL32(6, 9, 11, 150), IM_COL32(3, 5, 7, 215),
                              IM_COL32(3, 5, 7, 215));
  {
    // Extra wash behind the menu block, fading out symmetrically at both
    // sides so the (centered) block reads against busy game scenes.
    const float wash_pad = 110.0f * s;
    const ImU32 wash = IM_COL32(4, 6, 8, 140);
    const ImU32 wash_clear = IM_COL32(4, 6, 8, 0);
    dl->AddRectFilledMultiColor(ImVec2(rail_x - wash_pad, 0.0f),
                                ImVec2(rail_x, io.DisplaySize.y), wash_clear, wash, wash,
                                wash_clear);
    dl->AddRectFilled(ImVec2(rail_x, 0.0f), ImVec2(menu_right, io.DisplaySize.y), wash);
    dl->AddRectFilledMultiColor(ImVec2(menu_right, 0.0f),
                                ImVec2(menu_right + wash_pad, io.DisplaySize.y), wash,
                                wash_clear, wash_clear, wash);
  }

  // ---- Title ----
  dl->AddRectFilled(ImVec2(rail_x, title_y + 3.0f * s),
                    ImVec2(rail_x + 6.0f * s, title_y + title_size - 3.0f * s), kColAccent);
  dl->AddText(bold, title_size, ImVec2(rail_x + 18.0f * s, title_y), kColText, "Settings");
  {
    const char* cat_name = kCategories[category_].name;
    char crumb[64];
    std::snprintf(crumb, sizeof(crumb), "/  %s", cat_name);
    ImVec2 title_extent = bold->CalcTextSizeA(title_size, FLT_MAX, 0.0f, "Settings");
    dl->AddText(font, 20.0f * s,
                ImVec2(rail_x + 18.0f * s + title_extent.x + 16.0f * s,
                       title_y + title_size - 26.0f * s),
                kColTextDim, crumb);
  }
  if (pending) {
    const char* chip_text = "RESTART REQUIRED TO APPLY";
    float chip_size = 14.0f * s;
    ImVec2 extent = bold->CalcTextSizeA(chip_size, FLT_MAX, 0.0f, chip_text);
    float chip_pad = 10.0f * s;
    float chip_x1 = menu_right;
    float chip_x0 = chip_x1 - extent.x - 2.0f * chip_pad;
    float chip_y0 = title_y + title_size * 0.5f - extent.y * 0.5f - 6.0f * s;
    float chip_y1 = chip_y0 + extent.y + 12.0f * s;
    dl->AddRect(ImVec2(chip_x0, chip_y0), ImVec2(chip_x1, chip_y1), kColWarn, 0.0f, 0,
                1.5f);
    dl->AddText(bold, chip_size, ImVec2(chip_x0 + chip_pad, chip_y0 + 6.0f * s), kColWarn,
                chip_text);
  }
  dl->AddRectFilled(ImVec2(rail_x, columns_y - 12.0f * s),
                    ImVec2(menu_right, columns_y - 12.0f * s + 1.0f), kColRule);

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
    ImU32 bg = is_current ? kColPanelHover : (hovered ? kColPanelHover : kColPanel);
    dl->AddRectFilled(ImVec2(rail_x, y0), ImVec2(rail_x + rail_w, y1), bg, 0.0f);
    dl->AddRect(ImVec2(rail_x, y0), ImVec2(rail_x + rail_w, y1), kColPanelBorder, 0.0f);
    // Static accent bar only while focus lives in the content column; while
    // navigating the rail the bar rides the sliding highlight below so both
    // move at the same rate.
    if (is_current && zone_ == FocusZone::kContent) {
      dl->AddRectFilled(ImVec2(rail_x, y0), ImVec2(rail_x + 4.0f * s, y1), kColAccent,
                        0.0f, ImDrawFlags_RoundCornersLeft);
    }
  }
  // Focused rail item: sliding white fill (with the accent bar attached to
  // its left edge) drawn over the panels, then labels.
  if (rail_focus_target >= 0.0f) {
    if (rail_anim_y_ < 0.0f || std::abs(rail_anim_y_ - rail_focus_target) > 160.0f * s) {
      rail_anim_y_ = rail_focus_target;
    }
    rail_anim_y_ += (rail_focus_target - rail_anim_y_) * std::min(1.0f, io.DeltaTime * 22.0f);
    dl->AddRectFilled(ImVec2(rail_x, rail_anim_y_),
                      ImVec2(rail_x + rail_w, rail_anim_y_ + rail_item_h), kColSelFill,
                      0.0f);
    dl->AddRectFilled(ImVec2(rail_x, rail_anim_y_),
                      ImVec2(rail_x + 4.0f * s, rail_anim_y_ + rail_item_h), kColAccent,
                      0.0f, ImDrawFlags_RoundCornersLeft);
  } else {
    rail_anim_y_ = -1.0f;
  }
  for (int i = 0; i < category_count; ++i) {
    float y0 = columns_y + i * (rail_item_h + row_gap);
    bool is_current = category_ == i;
    bool focused = is_current && zone_ == FocusZone::kRail && rail_sel_ == i;
    ImU32 text_col = focused ? kColSelText : (is_current ? kColText : kColTextDim);
    AddTextVCentered(dl, focused || is_current ? bold : font, label_size, rail_x + 20.0f * s,
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
    float y0 = columns_y + category_count * (rail_item_h + row_gap) + 26.0f * s;
    float y1 = y0 + rail_item_h;
    bool focused = zone_ == FocusZone::kRail && rail_sel_ == quit_rail_index;
    bool hovered = mouse_in(rail_x, y0, rail_x + rail_w, y1);
    if (hovered && clicked && close_game_) {
      Hide();
      close_game_();
    }
    ImU32 bg = focused ? kColDanger : (hovered ? kColPanelHover : kColPanel);
    dl->AddRectFilled(ImVec2(rail_x, y0), ImVec2(rail_x + rail_w, y1), bg, 0.0f);
    dl->AddRect(ImVec2(rail_x, y0), ImVec2(rail_x + rail_w, y1),
                focused ? kColDanger : kColPanelBorder, 0.0f);
    if (!focused) {
      dl->AddRectFilled(ImVec2(rail_x, y0), ImVec2(rail_x + 4.0f * s, y1), kColDanger,
                        0.0f, ImDrawFlags_RoundCornersLeft);
    }
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

  dl->PushClipRect(ImVec2(content_x, columns_y - 2.0f), ImVec2(content_x + content_w + 1.0f,
                                                               content_bottom),
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

  // Backgrounds first (so the sliding highlight can sit above them), then
  // per-row content.
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const RowSpec& row = rows[i];
    if (row.kind == RowSpec::kHeader) {
      continue;
    }
    float y0 = columns_y + row_y[i] - content_scroll_;
    float y1 = y0 + row_hgt[i];
    if (y1 < columns_y || y0 > content_bottom) {
      continue;
    }
    bool hovered = mouse_in(content_x, y0, content_x + content_w, y1);
    if (hovered && mouse_moved && row.enabled) {
      zone_ = FocusZone::kContent;
      row_index_ = i;
    }
    ImU32 bg = hovered && row.enabled ? kColPanelHover : kColPanel;
    dl->AddRectFilled(ImVec2(content_x, y0), ImVec2(content_x + content_w, y1), bg, 0.0f);
    dl->AddRect(ImVec2(content_x, y0), ImVec2(content_x + content_w, y1), kColPanelBorder,
                0.0f);
  }
  if (content_focus && highlight_anim_y_ >= 0.0f) {
    float y0 = columns_y + highlight_anim_y_ - content_scroll_;
    dl->AddRectFilled(ImVec2(content_x, y0), ImVec2(content_x + content_w, y0 + row_h),
                      kColSelFill, 0.0f);
    dl->AddRect(ImVec2(content_x - 1.0f, y0 - 1.0f),
                ImVec2(content_x + content_w + 1.0f, y0 + row_h + 1.0f), kColAccent, 0.0f,
                0, 2.0f * s);
  }

  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    RowSpec& row = rows[i];
    float y0 = columns_y + row_y[i] - content_scroll_;
    float y1 = y0 + row_hgt[i];
    if (y1 < columns_y || y0 > content_bottom) {
      continue;
    }
    float cy = (y0 + y1) * 0.5f;

    if (row.kind == RowSpec::kHeader) {
      dl->AddText(bold, header_size, ImVec2(content_x + 2.0f * s, y0 + 8.0f * s), kColAccent,
                  row.label);
      ImVec2 extent = bold->CalcTextSizeA(header_size, FLT_MAX, 0.0f, row.label);
      dl->AddRectFilled(ImVec2(content_x + extent.x + 14.0f * s, y0 + 8.0f * s + extent.y * 0.5f),
                        ImVec2(content_x + content_w, y0 + 8.0f * s + extent.y * 0.5f + 1.0f),
                        kColRule);
      continue;
    }

    const bool focused = content_focus && row_index_ == i;
    const bool hovered = mouse_in(content_x, y0, content_x + content_w, y1);
    ImU32 label_col = focused ? kColSelText
                              : (!row.enabled ? kColTextFaint
                                              : (row.danger ? kColDanger : kColText));
    ImU32 value_col = focused ? kColSelText : (row.enabled ? kColText : kColTextFaint);
    ImU32 dim_col = focused ? IM_COL32(60, 70, 74, 255) : kColTextDim;

    // Label.
    AddTextVCentered(dl, font, label_size, content_x + 18.0f * s, cy, label_col, row.label);

    // Value area on the right side of the row.
    float vx1 = content_x + content_w - 12.0f * s;
    float vx0 = content_x + content_w - value_w;

    switch (row.kind) {
      case RowSpec::kEnum: {
        int count = static_cast<int>(row.options.size());
        int value = count > 0 ? std::clamp(enum_value(row), 0, count - 1) : 0;
        ImVec2 left_center(vx0 + chevron_r, cy);
        ImVec2 right_center(vx1 - chevron_r, cy);
        bool left_hover = row.enabled &&
                          mouse_in(left_center.x - chevron_r - 4.0f, cy - chevron_r - 4.0f,
                                   left_center.x + chevron_r + 4.0f, cy + chevron_r + 4.0f);
        bool right_hover = row.enabled &&
                           mouse_in(right_center.x - chevron_r - 4.0f, cy - chevron_r - 4.0f,
                                    right_center.x + chevron_r + 4.0f, cy + chevron_r + 4.0f);
        ImU32 chev_circle = focused ? kColAccent : IM_COL32(255, 255, 255, 36);
        ImU32 chev_tri = focused ? kColAccentDark : kColTextDim;
        DrawChevron(dl, left_center, chevron_r, true,
                    left_hover ? kColAccent : chev_circle,
                    left_hover ? kColAccentDark : chev_tri, focused || left_hover);
        DrawChevron(dl, right_center, chevron_r, false,
                    right_hover ? kColAccent : chev_circle,
                    right_hover ? kColAccentDark : chev_tri, focused || right_hover);
        if (count > 0) {
          float text_max_w = (right_center.x - chevron_r) - (left_center.x + chevron_r) -
                             16.0f * s;
          std::string text =
              TruncateToWidth(bold, value_size, text_max_w, row.options[value]);
          AddTextCentered(dl, bold, value_size,
                          ImVec2((left_center.x + right_center.x) * 0.5f, cy), value_col,
                          text.c_str());
        }
        if (clicked && row.enabled) {
          if (left_hover) {
            step_row(row, -1);
          } else if (right_hover) {
            step_row(row, 1);
          } else if (hovered) {
            step_row(row, 1);
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
        ImU32 track_col = focused ? IM_COL32(0, 0, 0, 56) : IM_COL32(255, 255, 255, 40);
        dl->AddRectFilled(ImVec2(track_x0, cy - 2.0f * s), ImVec2(track_x1, cy + 2.0f * s),
                          track_col, 0.0f);
        dl->AddRectFilled(ImVec2(track_x0, cy - 2.0f * s), ImVec2(knob_x, cy + 2.0f * s),
                          kColAccent, 0.0f);
        dl->AddCircleFilled(ImVec2(knob_x, cy), 8.0f * s,
                            focused ? kColSelText : IM_COL32(240, 244, 245, 255), 24);
        char value_text[32];
        std::snprintf(value_text, sizeof(value_text), row.fmt, double(*row.value));
        ImVec2 extent = bold->CalcTextSizeA(value_size, FLT_MAX, 0.0f, value_text);
        dl->AddText(bold, value_size, ImVec2(vx1 - extent.x, cy - extent.y * 0.5f), value_col,
                    value_text);
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
        const char* hint = row.enabled ? "Select" : "-";
        ImVec2 extent = font->CalcTextSizeA(15.0f * s, FLT_MAX, 0.0f, hint);
        dl->AddText(font, 15.0f * s, ImVec2(vx1 - extent.x, cy - extent.y * 0.5f), dim_col,
                    hint);
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
          ImGui::PushFont(font, value_size);
          ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0.12f));
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(0.05f, 0.06f, 0.07f, 1.0f));
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
          ImVec2 extent = bold->CalcTextSizeA(value_size, FLT_MAX, 0.0f, text);
          dl->AddText(bold, value_size, ImVec2(vx1 - extent.x, cy - extent.y * 0.5f),
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
    float panel_h = std::min(content_view_h, 320.0f * s);
    float header_bar_h = 38.0f * s;
    dl->AddRectFilled(ImVec2(desc_x, columns_y), ImVec2(desc_x + desc_w, columns_y + header_bar_h),
                      kColAccent, 0.0f, ImDrawFlags_RoundCornersTop);
    AddTextVCentered(dl, bold, 16.0f * s, desc_x + 14.0f * s, columns_y + header_bar_h * 0.5f,
                     kColAccentDark, "Description");
    dl->AddRectFilled(ImVec2(desc_x, columns_y + header_bar_h),
                      ImVec2(desc_x + desc_w, columns_y + panel_h), kColDescPanel, 0.0f,
                      ImDrawFlags_RoundCornersBottom);
    dl->AddRect(ImVec2(desc_x, columns_y), ImVec2(desc_x + desc_w, columns_y + panel_h),
                kColPanelBorder, 0.0f);

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
    float text_x = desc_x + 14.0f * s;
    float text_y = columns_y + header_bar_h + 14.0f * s;
    float wrap_w = desc_w - 28.0f * s;
    dl->AddText(font, desc_size, ImVec2(text_x, text_y), kColText, desc_text, nullptr, wrap_w);
    ImVec2 used = font->CalcTextSizeA(desc_size, FLT_MAX, wrap_w, desc_text);
    text_y += used.y + 10.0f * s;
    if (extra_text) {
      dl->AddText(font, desc_size, ImVec2(text_x, text_y), kColTextDim, extra_text->c_str(),
                  nullptr, wrap_w);
      ImVec2 extra_used =
          font->CalcTextSizeA(desc_size, FLT_MAX, wrap_w, extra_text->c_str());
      text_y += extra_used.y + 10.0f * s;
    }
    if (note_text) {
      dl->AddText(bold, desc_size, ImVec2(text_x, text_y), kColWarn, note_text, nullptr,
                  wrap_w);
      ImVec2 note_used = font->CalcTextSizeA(desc_size, FLT_MAX, wrap_w, note_text);
      text_y += note_used.y + 10.0f * s;
    }
    if (pending) {
      dl->AddText(font, desc_size, ImVec2(text_x, text_y), kColWarn,
                  "Changes are applied after a restart. Use Apply & Restart when ready.",
                  nullptr, wrap_w);
    }
  }

  // ---- Footer button legend ----
  {
    float legend_y = io.DisplaySize.y - footer_h + 14.0f * s;
    dl->AddRectFilled(ImVec2(rail_x, legend_y - 12.0f * s),
                      ImVec2(menu_right, legend_y - 12.0f * s + 1.0f), kColRule);
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
    float glyph_size = 15.0f * s;
    float label_text_size = 16.0f * s;
    float chip_h = 26.0f * s;
    for (const LegendGlyph& glyph : glyphs) {
      ImVec2 glyph_extent = bold->CalcTextSizeA(glyph_size, FLT_MAX, 0.0f, glyph.glyph);
      float cy = legend_y + chip_h * 0.5f;
      if (glyph.circle) {
        float r = chip_h * 0.5f;
        dl->AddCircleFilled(ImVec2(x + r, cy), r, IM_COL32(232, 236, 237, 235), 28);
        AddTextCentered(dl, bold, glyph_size, ImVec2(x + r, cy), IM_COL32(15, 18, 20, 255),
                        glyph.glyph);
        x += 2.0f * r + 8.0f * s;
      } else {
        float chip_w = glyph_extent.x + 18.0f * s;
        dl->AddRectFilled(ImVec2(x, legend_y), ImVec2(x + chip_w, legend_y + chip_h),
                          IM_COL32(232, 236, 237, 235), 4.0f * s);
        AddTextCentered(dl, bold, glyph_size, ImVec2(x + chip_w * 0.5f, cy),
                        IM_COL32(15, 18, 20, 255), glyph.glyph);
        x += chip_w + 8.0f * s;
      }
      ImVec2 label_extent = font->CalcTextSizeA(label_text_size, FLT_MAX, 0.0f, glyph.label);
      dl->AddText(font, label_text_size, ImVec2(x, cy - label_extent.y * 0.5f), kColTextDim,
                  glyph.label);
      x += label_extent.x + 26.0f * s;
    }
  }

  ImGui::End();
  ImGui::PopStyleVar(2);
}

}  // namespace rex::ui
