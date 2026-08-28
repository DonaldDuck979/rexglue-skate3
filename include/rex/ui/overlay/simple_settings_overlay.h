/**
 * @file        rex/ui/overlay/simple_settings_overlay.h
 *
 * @brief       Curated user-facing settings overlay.
 */
#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <rex/ui/graphics_device_list.h>
#include <rex/ui/imgui_dialog.h>

namespace rex::ui {

void EnsureSimpleSettingsConfig(const std::filesystem::path& config_path);
void SaveSimpleSettingsConfig(const std::filesystem::path& config_path);

struct SimpleProfileInfo {
  std::string id;
  std::string gamertag;
  bool signed_in = true;
};

struct SimpleProfileState {
  std::vector<SimpleProfileInfo> profiles;
  int selected_index = 0;
};

// Raw pad snapshot for overlay navigation (host-side, already merged across
// pads). Poll callback runs on the UI thread every drawn frame.
struct SimpleSettingsGamepad {
  bool connected = false;
  uint16_t buttons = 0;  // X_INPUT_GAMEPAD_* bits
  int16_t thumb_lx = 0;
  int16_t thumb_ly = 0;
};

// Optional online-play integration. When an online-connect callback is supplied
// to the dialog, an "Online" category appears in the rail; the dialog edits the
// game's own net cvars (skate3_net_mode / _host / _port / _name) directly, and
// calls the game hooks to actually start/stop the session and to read live
// status. Games without online play leave the hooks null and the category shows
// as unavailable. This snapshot is polled every frame the category is drawn.
struct SimpleSettingsOnlineStatus {
  bool active = false;        // a session is currently running
  std::string state_line;     // short state, e.g. "Hosting on port 34643"
  std::string detail_line;    // secondary line, e.g. "1 player connected" / error
};

// Live party snapshot for the Party menu tab. Empty when solo. The game's
// online layer fills this; the SDK reads it every frame the tab is drawn.
struct SimpleSettingsPartyMember {
  std::string name;
  bool leader = false;
  bool local = false;
};
struct SimpleSettingsPartyView {
  bool in_party = false;
  bool is_private = false;
  bool you_are_leader = false;
  std::string leader_name;
  std::vector<SimpleSettingsPartyMember> members;
  std::vector<std::string> invites;    // names of players inviting you.
};

class SimpleSettingsDialog final : public ImGuiDialog {
 public:
  using LoadProfilesCallback = std::function<SimpleProfileState()>;
  using SaveProfileCallback =
      std::function<void(int selected_index, std::string gamertag, bool signed_in)>;
  using CloseSettingsCallback = std::function<void()>;
  using CloseGameCallback = std::function<void()>;
  using RestartGameCallback = std::function<void()>;
  using PollGamepadCallback = std::function<SimpleSettingsGamepad()>;
  // Online play (optional). Status is polled for the live readout; connect/
  // disconnect start and stop the session after the dialog has written the net
  // cvars. See SimpleSettingsOnlineStatus.
  using OnlineStatusCallback = std::function<SimpleSettingsOnlineStatus()>;
  using OnlineActionCallback = std::function<void()>;
  using PartyStatusCallback = std::function<SimpleSettingsPartyView()>;

  SimpleSettingsDialog(ImGuiDrawer* drawer, std::filesystem::path config_path,
                       LoadProfilesCallback load_profiles, SaveProfileCallback save_profile,
                       CloseSettingsCallback close_settings, CloseGameCallback close_game,
                       RestartGameCallback restart_game,
                       PollGamepadCallback poll_gamepad = nullptr,
                       OnlineStatusCallback online_status = nullptr,
                       OnlineActionCallback online_connect = nullptr,
                       OnlineActionCallback online_disconnect = nullptr,
                       PartyStatusCallback party_status = nullptr);
  ~SimpleSettingsDialog();

  void Show();
  void Toggle();
  void Hide();
  // One "back" step (Escape / pad B): text edit -> row focus -> category rail
  // -> closed. The Escape keybind routes here so Escape backs out level by
  // level instead of instantly closing.
  void NavigateBack();
  bool visible() const { return visible_; }

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  enum class FocusZone { kRail, kContent };

  struct RowSpec;
  struct NavIntents;

  void LoadSettingsFromCvars();
  bool HasSettingsChanges() const;
  void ReloadProfiles();
  void SaveVideo();
  void SaveProfile();
  void ApplyAndRestart();

  void BuildRows(std::vector<RowSpec>& rows, int category);
  NavIntents GatherInput(ImGuiIO& io);

  std::filesystem::path config_path_;
  LoadProfilesCallback load_profiles_;
  SaveProfileCallback save_profile_;
  CloseSettingsCallback close_settings_;
  CloseGameCallback close_game_;
  RestartGameCallback restart_game_;
  PollGamepadCallback poll_gamepad_;
  OnlineStatusCallback online_status_;
  OnlineActionCallback online_connect_;
  OnlineActionCallback online_disconnect_;
  PartyStatusCallback party_status_;
  SimpleProfileState profiles_;
  bool visible_ = false;

  // Staged setting values (committed by SaveVideo / SaveProfile).
  GraphicsDeviceList device_list_;
  int graphics_api_index_ = 0;
  int device_index_ = 0;
  int resolution_scale_index_ = 0;
  int frame_cap_index_ = 0;
  int aspect_ratio_index_ = 0;
  int msaa_index_ = 2;
  int shadow_quality_index_ = 2;
  int static_shadow_res_index_ = 2;
  int monitor_index_ = 0;
  int audio_buffer_index_ = 0;
  int language_index_ = 0;
  float field_of_view_ = 60.0f;
  bool fullscreen_ = true;
  bool vsync_ = false;
  bool tearing_ = true;
  bool mnk_mode_ = false;
  bool mnk_capture_mouse_ = false;
  bool profile_signed_in_ = true;
  char gamertag_buf_[32] = {};
  // Online category staged values, committed to the net cvars when the player
  // activates Host / Join. Seeded from the cvars in LoadSettingsFromCvars.
  char net_host_buf_[64] = {};
  char net_port_buf_[8] = {};
  char net_name_buf_[32] = {};
  bool net_is_client_ = false;   // false = Host, true = Join
  std::string net_state_line_;   // stable backing store for dynamic status labels
  std::string net_detail_line_;
  // Game Modes tab (started via the game's skate3_mode_start/_rounds/
  // skate3_skate_start cvars). Indices into the option lists in the .cpp.
  int spot_rounds_index_ = 0;    // Spot Battle rounds 1-6 (index 0 = 1 round).
  int spot_time_index_ = 3;      // Spot Battle seconds/round (default 60s).
  int skate_rounds_index_ = 0;   // S.K.A.T.E. rounds 1-3 (index 0 = 1 round).
  // Party tab (v4).
  char party_invite_buf_[32] = {};   // name of the player to invite.
  // Stable-address string storage for per-frame RowSpec label pointers built
  // from live party data (accept-invite rows + member rows). Deque so
  // push_back never invalidates existing c_str() pointers.
  std::deque<std::string> party_label_pool_;
  // Bool the "Private Party" enum row's flag pointer points at. Refreshed to
  // the live pv.is_private each frame the row is built.
  bool party_private_index_ = false;
  // Live setting values (hot cvars, applied and saved on change).
  bool renderer_native_ = true;
  bool ssao_ = true;
  bool static_shadows_ = true;
  bool shadow_pcss_ = true;
  bool bloom_ = true;
  bool volumetrics_ = true;
  int draw_distance_index_ = 1;
  int stream_probe_index_ = 0;
  bool mode_indicator_ = true;
  bool fps_counter_ = false;
  bool net_hud_ = false;
  bool audio_mute_ = false;
  bool rumble_ = true;
  float mnk_sensitivity_ = 1.0f;
  int chord_index_ = 0;
  int input_backend_index_ = 0;
  std::string chord_custom_;

  // Navigation state.
  FocusZone zone_ = FocusZone::kRail;
  int category_ = 0;
  int rail_sel_ = 0;  // 0..category count-1 = categories; count = the Close Game item
  int row_index_ = 0;
  bool editing_text_ = false;
  bool text_edit_focus_pending_ = false;
  bool pad_active_ = false;      // last nav input came from a pad -> pad legend
  uint16_t prev_pad_buttons_ = 0;
  int held_dir_x_ = 0;           // current held direction (-1/0/1), for repeat
  int held_dir_y_ = 0;
  float repeat_timer_x_ = 0.0f;
  float repeat_timer_y_ = 0.0f;
  float highlight_anim_y_ = -1.0f;  // smoothed selection highlight position
  float rail_anim_y_ = -1.0f;
  float content_scroll_ = 0.0f;       // scroll TARGET, locked to row boundaries
  float content_scroll_anim_ = -1.0f;  // drawn scroll, chases the target
  float wheel_accum_ = 0.0f;           // fractional wheel deltas -> whole notches
  // Swallow the first frame's cursor delta after Show: the pre-open cursor
  // position (or a cursor-mode warp) otherwise reads as mouse motion and
  // steals focus from the rail to whatever row it lands on.
  bool just_shown_ = false;
  float mouse_x_ = -1.0f;        // last mouse position, to detect real motion
  float mouse_y_ = -1.0f;
};

}  // namespace rex::ui
