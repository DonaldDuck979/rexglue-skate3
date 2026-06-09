#include <rex/ui/overlay/ultrawide_targets_overlay.h>

#include <rex/graphics/ultrawide_debug.h>
#include <rex/cvar.h>
#include <rex/logging.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <imgui.h>

namespace rex::ui {

namespace {

std::string Hex(uint64_t value, uint32_t width) {
  std::ostringstream stream;
  stream << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << value;
  return stream.str();
}

std::string TargetGroupLabel(const graphics::ultrawide_debug::TargetEntry& entry) {
  const auto& key = entry.key;
  std::ostringstream stream;
  stream << "RT0 " << Hex(key.color_info[0], 8) << " / D " << Hex(key.depth_info, 8)
         << " / " << key.viewport_width << "x" << key.viewport_height;
  return stream.str();
}

const char* ScreenCallbackKindLabel(graphics::ultrawide_debug::ScreenCallbackKind kind) {
  switch (kind) {
    case graphics::ultrawide_debug::ScreenCallbackKind::kWidth:
      return "Width";
    case graphics::ultrawide_debug::ScreenCallbackKind::kHeight:
      return "Height";
    case graphics::ultrawide_debug::ScreenCallbackKind::kScreenFlag:
      return "Screen flag";
    case graphics::ultrawide_debug::ScreenCallbackKind::kWide:
      return "Wide";
  }
  return "Unknown";
}

const char* DrawBucketLabel(graphics::ultrawide_debug::DrawBucket bucket) {
  switch (bucket) {
    case graphics::ultrawide_debug::DrawBucket::kMainColorDepth:
      return "Main";
    case graphics::ultrawide_debug::DrawBucket::kDepthOnly:
      return "Depth";
    case graphics::ultrawide_debug::DrawBucket::kCopyResolve:
      return "Copy";
    case graphics::ultrawide_debug::DrawBucket::kMemexport:
      return "Memexport";
    case graphics::ultrawide_debug::DrawBucket::kNoPixelShader:
      return "No PS";
  }
  return "Unknown";
}

struct DrawDiffEntry {
  graphics::ultrawide_debug::DrawFingerprint fingerprint;
  uint64_t visible_count = 0;
  uint64_t popped_count = 0;
};

enum class DrawGroupMode : uint32_t {
  kShader = 0,
  kObject = 1,
  kTexture = 2,
};

struct DrawGroupKey {
  DrawGroupMode mode = DrawGroupMode::kObject;
  graphics::ultrawide_debug::DrawBucket bucket =
      graphics::ultrawide_debug::DrawBucket::kMainColorDepth;
  uint32_t guest_render_object = 0;
  uint32_t guest_render_vtable = 0;
  uint32_t guest_render_target = 0;
  uint64_t vertex_shader_hash = 0;
  uint64_t pixel_shader_hash = 0;
  uint32_t primitive_type = 0;
  uint32_t index_guest_base = 0;
  uint32_t index_length = 0;
  std::array<uint32_t, graphics::ultrawide_debug::DrawFingerprint::kVertexFetchCount>
      vertex_fetch_address{};
  std::array<uint32_t, graphics::ultrawide_debug::DrawFingerprint::kVertexFetchCount>
      vertex_fetch_size{};
  std::array<uint64_t, graphics::ultrawide_debug::DrawFingerprint::kTextureFetchCount>
      texture_key_hash{};

  bool operator==(const DrawGroupKey& other) const {
    return mode == other.mode && bucket == other.bucket &&
           guest_render_object == other.guest_render_object &&
           guest_render_vtable == other.guest_render_vtable &&
           guest_render_target == other.guest_render_target &&
           vertex_shader_hash == other.vertex_shader_hash &&
           pixel_shader_hash == other.pixel_shader_hash && primitive_type == other.primitive_type &&
           index_guest_base == other.index_guest_base && index_length == other.index_length &&
           vertex_fetch_address == other.vertex_fetch_address &&
           vertex_fetch_size == other.vertex_fetch_size &&
           texture_key_hash == other.texture_key_hash;
  }
};

struct DrawGroupKeyHash {
  size_t operator()(const DrawGroupKey& key) const {
    uint64_t hash = 1469598103934665603ull;
    auto hash_add = [&hash](uint64_t value) {
      for (size_t i = 0; i < sizeof(value); ++i) {
        hash ^= uint8_t(value >> (i * 8));
        hash *= 1099511628211ull;
      }
    };
    hash_add(uint32_t(key.mode));
    hash_add(uint32_t(key.bucket));
    hash_add(key.guest_render_object);
    hash_add(key.guest_render_vtable);
    hash_add(key.guest_render_target);
    hash_add(key.vertex_shader_hash);
    hash_add(key.pixel_shader_hash);
    hash_add(key.primitive_type);
    hash_add(key.index_guest_base);
    hash_add(key.index_length);
    for (uint32_t fetch_address : key.vertex_fetch_address) {
      hash_add(fetch_address);
    }
    for (uint32_t fetch_size : key.vertex_fetch_size) {
      hash_add(fetch_size);
    }
    for (uint64_t texture_hash : key.texture_key_hash) {
      hash_add(texture_hash);
    }
    return size_t(hash);
  }
};

struct DrawGroupEntry {
  DrawGroupKey key;
  graphics::ultrawide_debug::DrawFingerprint representative;
  graphics::ultrawide_debug::DrawFingerprint textured_representative;
  graphics::ultrawide_debug::DrawFingerprint main_textured_representative;
  uint64_t group_hash = 0;
  uint64_t draw_count = 0;
  uint64_t textured_draw_count = 0;
  uint64_t main_textured_draw_count = 0;
  uint64_t submitted_count = 0;
  uint64_t skipped_count = 0;
  uint64_t vertices = 0;
  uint64_t primitives = 0;
  uint32_t min_vertex_count = 0;
  uint32_t max_vertex_count = 0;
  uint32_t min_primitive_count = 0;
  uint32_t max_primitive_count = 0;
  uint32_t exact_count = 0;
  uint32_t enabled_count = 0;
  uint64_t last_seen_order = 0;
  uint64_t last_textured_seen_order = 0;
  uint64_t last_main_textured_seen_order = 0;
  uint64_t last_seen_ms = 0;
  uint64_t last_textured_seen_ms = 0;
  uint64_t last_main_textured_seen_ms = 0;
  std::vector<uint64_t> exact_hashes;
};

struct DrawGroupDiffEntry {
  DrawGroupKey key;
  graphics::ultrawide_debug::DrawFingerprint representative;
  uint64_t group_hash = 0;
  uint64_t visible_count = 0;
  uint64_t popped_count = 0;
};

struct DrawWatchEvent {
  uint64_t sequence = 0;
  bool present = false;
  uint64_t age = 0;
  uint32_t packet_ptr = 0;
  uint32_t index_guest_base = 0;
  uint32_t vertex_fetch_address = 0;
  uint64_t group_hash = 0;
};

std::vector<graphics::ultrawide_debug::DrawFingerprintEntry> g_visible_draw_capture;
std::vector<graphics::ultrawide_debug::DrawFingerprintEntry> g_popped_draw_capture;
std::optional<DrawGroupKey> g_watched_draw_group;
uint64_t g_watched_draw_group_hash = 0;
std::deque<DrawWatchEvent> g_draw_watch_events;
uint64_t g_draw_watch_event_sequence = 0;
bool g_draw_watch_state_valid = false;
bool g_draw_watch_present = false;
int g_draw_watch_age_threshold = 120;

uint64_t NowMilliseconds() {
  return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count());
}

bool DrawHasTexture(const graphics::ultrawide_debug::DrawFingerprint& fingerprint) {
  for (uint64_t texture_hash : fingerprint.texture_key_hash) {
    if (texture_hash != 0) {
      return true;
    }
  }
  return false;
}

DrawGroupKey MakeDrawGroupKey(const graphics::ultrawide_debug::DrawFingerprint& fingerprint,
                              DrawGroupMode mode) {
  DrawGroupKey key;
  key.mode = mode;
  key.guest_render_object = fingerprint.guest_render_object;
  key.guest_render_vtable = fingerprint.guest_render_vtable;
  key.guest_render_target = fingerprint.guest_render_target;

  if (mode == DrawGroupMode::kShader) {
    key.bucket = fingerprint.bucket;
    key.vertex_shader_hash = fingerprint.vertex_shader_hash;
    key.pixel_shader_hash = fingerprint.pixel_shader_hash;
    key.primitive_type = fingerprint.primitive_type;
    std::copy(std::begin(fingerprint.vertex_fetch_size), std::end(fingerprint.vertex_fetch_size),
              key.vertex_fetch_size.begin());
    return key;
  }

  if (mode == DrawGroupMode::kTexture) {
    size_t texture_count = 0;
    for (uint64_t texture_hash : fingerprint.texture_key_hash) {
      if (texture_hash != 0 && texture_count < key.texture_key_hash.size()) {
        key.texture_key_hash[texture_count++] = texture_hash;
      }
    }
    std::sort(key.texture_key_hash.begin(), key.texture_key_hash.end());
    if (texture_count != 0) {
      return key;
    }
  }

  if (mode == DrawGroupMode::kObject && fingerprint.guest_render_object != 0) {
    key.bucket = fingerprint.bucket;
    return key;
  }

  key.index_guest_base = fingerprint.index_guest_base;
  key.index_length = fingerprint.index_length;
  std::copy(std::begin(fingerprint.vertex_fetch_address),
            std::end(fingerprint.vertex_fetch_address), key.vertex_fetch_address.begin());
  std::copy(std::begin(fingerprint.vertex_fetch_size), std::end(fingerprint.vertex_fetch_size),
            key.vertex_fetch_size.begin());
  return key;
}

uint64_t HashDrawGroupKey(const DrawGroupKey& key) {
  return uint64_t(DrawGroupKeyHash{}(key));
}

std::string DrawObjectLabel(const graphics::ultrawide_debug::DrawFingerprint& fingerprint) {
  std::ostringstream stream;
  if (fingerprint.guest_render_object != 0) {
    stream << "Obj " << Hex(fingerprint.guest_render_object, 8);
    if (fingerprint.guest_render_vtable != 0) {
      stream << " VT " << Hex(fingerprint.guest_render_vtable, 8);
    }
    return stream.str();
  }
  if (fingerprint.index_guest_base != 0) {
    stream << "IB " << Hex(fingerprint.index_guest_base, 8);
    if (fingerprint.index_length != 0) {
      stream << "+" << std::uppercase << std::hex << fingerprint.index_length;
    }
  } else {
    stream << "NoIB";
  }
  for (size_t i = 0; i < graphics::ultrawide_debug::DrawFingerprint::kVertexFetchCount; ++i) {
    if (fingerprint.vertex_fetch_address[i] == 0) {
      continue;
    }
    stream << " VF" << i << " " << Hex(fingerprint.vertex_fetch_address[i], 8);
    break;
  }
  return stream.str();
}

std::string DrawTextureLabel(const graphics::ultrawide_debug::DrawFingerprint& fingerprint) {
  for (size_t i = 0; i < graphics::ultrawide_debug::DrawFingerprint::kTextureFetchCount; ++i) {
    if (fingerprint.texture_key_hash[i] == 0) {
      continue;
    }
    std::ostringstream stream;
    stream << "t" << fingerprint.texture_fetch_index[i] << " "
           << Hex(fingerprint.texture_key_hash[i], 16);
    if (fingerprint.texture_width[i] != 0 || fingerprint.texture_height[i] != 0) {
      stream << " " << fingerprint.texture_width[i] << "x" << fingerprint.texture_height[i];
    }
    if (fingerprint.texture_base_address[i] != 0) {
      stream << " @" << Hex(fingerprint.texture_base_address[i], 8);
    }
    return stream.str();
  }
  return "";
}

uint32_t FirstVertexFetchAddress(
    const graphics::ultrawide_debug::DrawFingerprint& fingerprint) {
  for (uint32_t address : fingerprint.vertex_fetch_address) {
    if (address != 0) {
      return address;
    }
  }
  return 0;
}

uint32_t FirstTextureBaseAddress(
    const graphics::ultrawide_debug::DrawFingerprint& fingerprint) {
  for (uint32_t address : fingerprint.texture_base_address) {
    if (address != 0) {
      return address;
    }
  }
  return 0;
}

const graphics::ultrawide_debug::DrawFingerprint& BestWatchFingerprint(
    const DrawGroupEntry* group,
    const graphics::ultrawide_debug::DrawFingerprint& fallback) {
  if (group && group->last_main_textured_seen_order != 0) {
    return group->main_textured_representative;
  }
  if (group && group->last_textured_seen_order != 0) {
    return group->textured_representative;
  }
  return fallback;
}

void SetWatchedDrawGroup(const DrawGroupKey& key, uint64_t group_hash,
                         const graphics::ultrawide_debug::DrawFingerprint& fingerprint) {
  g_watched_draw_group = key;
  g_watched_draw_group_hash = group_hash;
  g_draw_watch_events.clear();
  g_draw_watch_state_valid = false;
  graphics::ultrawide_debug::SetGuestStoreWatchTargets(
      fingerprint.index_guest_base, FirstVertexFetchAddress(fingerprint),
      FirstTextureBaseAddress(fingerprint), group_hash);
}

std::vector<DrawGroupEntry> BuildDrawGroups(
    const std::vector<graphics::ultrawide_debug::DrawFingerprintEntry>& entries,
    DrawGroupMode mode) {
  std::unordered_map<DrawGroupKey, DrawGroupEntry, DrawGroupKeyHash> rows;
  for (const auto& entry : entries) {
    DrawGroupKey key = MakeDrawGroupKey(entry.fingerprint, mode);
    auto [it, inserted] = rows.try_emplace(key);
    DrawGroupEntry& row = it->second;
    if (inserted) {
      row.key = key;
      row.representative = entry.fingerprint;
      row.group_hash = HashDrawGroupKey(key);
      row.min_vertex_count = entry.fingerprint.vertex_count;
      row.max_vertex_count = entry.fingerprint.vertex_count;
      row.min_primitive_count = entry.fingerprint.primitive_count;
      row.max_primitive_count = entry.fingerprint.primitive_count;
    } else {
      row.min_vertex_count = std::min(row.min_vertex_count, entry.fingerprint.vertex_count);
      row.max_vertex_count = std::max(row.max_vertex_count, entry.fingerprint.vertex_count);
      row.min_primitive_count = std::min(row.min_primitive_count, entry.fingerprint.primitive_count);
      row.max_primitive_count = std::max(row.max_primitive_count, entry.fingerprint.primitive_count);
    }
    if (entry.last_seen_order >= row.last_seen_order) {
      row.representative = entry.fingerprint;
      row.last_seen_ms = entry.last_seen_ms;
    }
    const bool has_texture = DrawHasTexture(entry.fingerprint);
    const bool main_textured =
        has_texture &&
        entry.fingerprint.bucket == graphics::ultrawide_debug::DrawBucket::kMainColorDepth;
    if (has_texture) {
      row.textured_draw_count += entry.draw_count;
      if (entry.last_seen_order >= row.last_textured_seen_order) {
        row.textured_representative = entry.fingerprint;
        row.last_textured_seen_order = entry.last_seen_order;
        row.last_textured_seen_ms = entry.last_seen_ms;
      }
    }
    if (main_textured) {
      row.main_textured_draw_count += entry.draw_count;
      if (entry.last_seen_order >= row.last_main_textured_seen_order) {
        row.main_textured_representative = entry.fingerprint;
        row.last_main_textured_seen_order = entry.last_seen_order;
        row.last_main_textured_seen_ms = entry.last_seen_ms;
      }
    }
    row.draw_count += entry.draw_count;
    row.submitted_count += entry.submitted_count;
    row.skipped_count += entry.skipped_count;
    row.vertices += entry.vertices;
    row.primitives += entry.primitives;
    ++row.exact_count;
    if (entry.enabled) {
      ++row.enabled_count;
    }
    row.last_seen_order = std::max(row.last_seen_order, entry.last_seen_order);
    row.last_seen_ms = std::max(row.last_seen_ms, entry.last_seen_ms);
    row.exact_hashes.push_back(entry.hash ? entry.hash
                                          : graphics::ultrawide_debug::HashDrawFingerprint(entry.fingerprint));
  }

  std::vector<DrawGroupEntry> groups;
  groups.reserve(rows.size());
  for (auto& [_, row] : rows) {
    groups.push_back(std::move(row));
  }
  std::sort(groups.begin(), groups.end(), [](const DrawGroupEntry& a, const DrawGroupEntry& b) {
    if (a.key.bucket != b.key.bucket) {
      return uint32_t(a.key.bucket) < uint32_t(b.key.bucket);
    }
    if (a.draw_count != b.draw_count) {
      return a.draw_count > b.draw_count;
    }
    if (a.exact_count != b.exact_count) {
      return a.exact_count > b.exact_count;
    }
    return a.group_hash < b.group_hash;
  });
  return groups;
}

std::vector<DrawDiffEntry> BuildDrawDiff() {
  std::unordered_map<uint64_t, DrawDiffEntry> rows;
  for (const auto& entry : g_visible_draw_capture) {
    auto& row = rows[graphics::ultrawide_debug::HashDrawFingerprint(entry.fingerprint)];
    row.fingerprint = entry.fingerprint;
    row.visible_count += entry.draw_count;
  }
  for (const auto& entry : g_popped_draw_capture) {
    auto& row = rows[graphics::ultrawide_debug::HashDrawFingerprint(entry.fingerprint)];
    if (!row.visible_count) {
      row.fingerprint = entry.fingerprint;
    }
    row.popped_count += entry.draw_count;
  }

  std::vector<DrawDiffEntry> diff;
  for (const auto& [hash, row] : rows) {
    (void)hash;
    if (row.visible_count > row.popped_count) {
      diff.push_back(row);
    }
  }
  std::sort(diff.begin(), diff.end(), [](const DrawDiffEntry& a, const DrawDiffEntry& b) {
    const uint64_t a_delta = a.visible_count - a.popped_count;
    const uint64_t b_delta = b.visible_count - b.popped_count;
    if (a_delta != b_delta) {
      return a_delta > b_delta;
    }
    if (a.fingerprint.vertex_count != b.fingerprint.vertex_count) {
      return a.fingerprint.vertex_count > b.fingerprint.vertex_count;
    }
    return graphics::ultrawide_debug::HashDrawFingerprint(a.fingerprint) <
           graphics::ultrawide_debug::HashDrawFingerprint(b.fingerprint);
  });
  return diff;
}

std::vector<DrawGroupDiffEntry> BuildDrawGroupDiff(DrawGroupMode mode) {
  std::unordered_map<DrawGroupKey, DrawGroupDiffEntry, DrawGroupKeyHash> rows;
  for (const auto& entry : g_visible_draw_capture) {
    DrawGroupKey key = MakeDrawGroupKey(entry.fingerprint, mode);
    auto [it, inserted] = rows.try_emplace(key);
    DrawGroupDiffEntry& row = it->second;
    if (inserted) {
      row.key = key;
      row.representative = entry.fingerprint;
      row.group_hash = HashDrawGroupKey(key);
    }
    row.visible_count += entry.draw_count;
  }
  for (const auto& entry : g_popped_draw_capture) {
    DrawGroupKey key = MakeDrawGroupKey(entry.fingerprint, mode);
    auto [it, inserted] = rows.try_emplace(key);
    DrawGroupDiffEntry& row = it->second;
    if (inserted) {
      row.key = key;
      row.representative = entry.fingerprint;
      row.group_hash = HashDrawGroupKey(key);
    }
    row.popped_count += entry.draw_count;
  }

  std::vector<DrawGroupDiffEntry> diff;
  for (const auto& [_, row] : rows) {
    if (row.visible_count > row.popped_count) {
      diff.push_back(row);
    }
  }
  std::sort(diff.begin(), diff.end(), [](const DrawGroupDiffEntry& a, const DrawGroupDiffEntry& b) {
    const uint64_t a_delta = a.visible_count - a.popped_count;
    const uint64_t b_delta = b.visible_count - b.popped_count;
    if (a_delta != b_delta) {
      return a_delta > b_delta;
    }
    return a.group_hash < b.group_hash;
  });
  return diff;
}

}  // namespace

UltrawideTargetsDialog::UltrawideTargetsDialog(ImGuiDrawer* drawer,
                                               std::filesystem::path export_path,
                                               CloseCallback close_callback)
    : ImGuiDialog(drawer),
      export_path_(std::move(export_path)),
      close_callback_(std::move(close_callback)) {
  graphics::ultrawide_debug::SetGuestStoreWatchLogPath(
      export_path_.parent_path() / "skate3_ultrawide_watch.log");
  graphics::ultrawide_debug::SetTargetRecordingActive(false);
  SetDrawActive(false);
}

UltrawideTargetsDialog::~UltrawideTargetsDialog() = default;

void UltrawideTargetsDialog::Show() {
  visible_ = true;
  graphics::ultrawide_debug::SetTargetRecordingActive(true);
  SetDrawActive(true);
}

void UltrawideTargetsDialog::Toggle() {
  if (visible_) {
    Hide();
  } else {
    Show();
  }
}

void UltrawideTargetsDialog::Hide() {
  if (!visible_) {
    return;
  }
  visible_ = false;
  graphics::ultrawide_debug::SetTargetRecordingActive(false);
  SetDrawActive(false);
  if (close_callback_) {
    close_callback_();
  }
}

void UltrawideTargetsDialog::OnDraw(ImGuiIO& io) {
  (void)io;
  if (!visible_) {
    return;
  }

  bool open = visible_;
  ImGui::SetNextWindowSize(ImVec2(1160.0f, 620.0f), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Ultrawide Render Targets", &open, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    if (!open) {
      Hide();
    }
    return;
  }

  auto targets = graphics::ultrawide_debug::SnapshotTargets();
  auto screen_callbacks = graphics::ultrawide_debug::SnapshotScreenCallbacks();
  auto guest_store_watch_events = graphics::ultrawide_debug::SnapshotGuestStoreWatchEvents();
  std::sort(targets.begin(), targets.end(),
            [](const graphics::ultrawide_debug::TargetEntry& a,
               const graphics::ultrawide_debug::TargetEntry& b) {
              if (a.default_enabled != b.default_enabled) {
                return a.default_enabled > b.default_enabled;
              }
              if (a.key.color_info[0] != b.key.color_info[0]) {
                return a.key.color_info[0] < b.key.color_info[0];
              }
              if (a.key.depth_info != b.key.depth_info) {
                return a.key.depth_info < b.key.depth_info;
              }
              if (a.key.viewport_width != b.key.viewport_width) {
                return a.key.viewport_width > b.key.viewport_width;
              }
              if (a.key.viewport_height != b.key.viewport_height) {
                return a.key.viewport_height > b.key.viewport_height;
              }
              if (a.key.vertex_shader_hash != b.key.vertex_shader_hash) {
                return a.key.vertex_shader_hash < b.key.vertex_shader_hash;
              }
              return a.hash < b.hash;
            });
  std::sort(screen_callbacks.begin(), screen_callbacks.end(),
            [](const graphics::ultrawide_debug::ScreenCallbackEntry& a,
               const graphics::ultrawide_debug::ScreenCallbackEntry& b) {
              if (a.kind != b.kind) {
                return uint32_t(a.kind) < uint32_t(b.kind);
              }
              if (a.caller_lr != b.caller_lr) {
                return a.caller_lr < b.caller_lr;
              }
              return a.hash < b.hash;
            });
  if (ImGui::Button("Enable all")) {
    graphics::ultrawide_debug::SetAllTargetsEnabled(true);
  }
  ImGui::SameLine();
  if (ImGui::Button("Disable all")) {
    graphics::ultrawide_debug::SetAllTargetsEnabled(false);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset defaults")) {
    graphics::ultrawide_debug::ResetTargetOverrides();
  }
  ImGui::SameLine();
  if (ImGui::Button("Save config")) {
    std::error_code ec;
    std::filesystem::create_directories(export_path_.parent_path(), ec);
    const auto saved = graphics::ultrawide_debug::SaveTargets(export_path_);
    if (saved) {
      REXLOG_INFO("Saved ultrawide render target config to {}", saved->string());
    } else {
      REXLOG_WARN("Failed to save ultrawide render target config to {}", export_path_.string());
    }
  }
  ImGui::SameLine();
  ImGui::Checkbox("Seen this session", &show_only_seen_);

  bool skip_shadow_targets = rex::cvar::Query<bool>("skate3_ultrawide_skip_shadow_targets");
  ImGui::SameLine();
  if (ImGui::Checkbox("Skip shadow maps", &skip_shadow_targets)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_skip_shadow_targets",
                             skip_shadow_targets ? "true" : "false");
  }
  int shadow_skip_mode = std::clamp(rex::cvar::Query<int32_t>("skate3_ultrawide_shadow_skip_mode"),
                                    1, 3) -
                         1;
  const char* shadow_mode_labels[] = {"Atlas", "Atlas+large", "Depth-only"};
  ImGui::SameLine();
  ImGui::SetNextItemWidth(110.0f);
  if (ImGui::Combo("Shadow mode", &shadow_skip_mode, shadow_mode_labels,
                   IM_ARRAYSIZE(shadow_mode_labels))) {
    rex::cvar::SetFlagByName("skate3_ultrawide_shadow_skip_mode",
                             std::to_string(shadow_skip_mode + 1));
  }

  int apply_mode_index = static_cast<int>(graphics::ultrawide_debug::GetApplyMode());
  const char* apply_mode_labels[] = {"Target list", "Force all", "Force none", "Saved only"};
  ImGui::SameLine();
  ImGui::SetNextItemWidth(130.0f);
  if (ImGui::Combo("Mode", &apply_mode_index, apply_mode_labels,
                   IM_ARRAYSIZE(apply_mode_labels))) {
    graphics::ultrawide_debug::SetApplyMode(
        static_cast<graphics::ultrawide_debug::ApplyMode>(apply_mode_index));
  }

  bool fake_occlusion_queries =
      rex::cvar::Query<bool>("skate3_ultrawide_fake_occlusion_queries");
  if (ImGui::Checkbox("Fake visibility queries", &fake_occlusion_queries)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_fake_occlusion_queries",
                             fake_occlusion_queries ? "true" : "false");
  }
  ImGui::SameLine();
  int fake_occlusion_sample_count = std::clamp(
      rex::cvar::Query<int32_t>("skate3_ultrawide_fake_occlusion_sample_count"), 1, 1000000);
  ImGui::SetNextItemWidth(110.0f);
  if (ImGui::InputInt("Visibility samples", &fake_occlusion_sample_count, 1000, 10000)) {
    fake_occlusion_sample_count = std::clamp(fake_occlusion_sample_count, 1, 1000000);
    rex::cvar::SetFlagByName("skate3_ultrawide_fake_occlusion_sample_count",
                             std::to_string(fake_occlusion_sample_count));
  }

  bool widen_game_frustum =
      rex::cvar::Query<bool>("skate3_ultrawide_widen_game_frustum");
  if (ImGui::Checkbox("Widen game frustum", &widen_game_frustum)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_widen_game_frustum",
                             widen_game_frustum ? "true" : "false");
  }
  ImGui::SameLine();
  bool force_main_visibility_flags =
      rex::cvar::Query<bool>("skate3_ultrawide_force_main_visibility_flags");
  if (ImGui::Checkbox("Force main visibility", &force_main_visibility_flags)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_force_main_visibility_flags",
                             force_main_visibility_flags ? "true" : "false");
  }
  ImGui::SameLine();
  bool disable_ndc_correction =
      rex::cvar::Query<bool>("skate3_ultrawide_disable_ndc_correction");
  if (ImGui::Checkbox("Disable NDC correction", &disable_ndc_correction)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_disable_ndc_correction",
                             disable_ndc_correction ? "true" : "false");
  }
  ImGui::SameLine();
  if (ImGui::Button("Trace 1k")) {
    rex::cvar::SetFlagByName("skate3_ultrawide_texture_trace_remaining", "1000");
  }
  ImGui::SameLine();
  if (ImGui::Button("Trace 10k")) {
    rex::cvar::SetFlagByName("skate3_ultrawide_texture_trace_remaining", "10000");
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop trace")) {
    rex::cvar::SetFlagByName("skate3_ultrawide_texture_trace_remaining", "0");
  }
  ImGui::SameLine();
  int texture_trace_remaining =
      std::clamp(rex::cvar::Query<int32_t>("skate3_ultrawide_texture_trace_remaining"), 0,
                 1000000);
  ImGui::SetNextItemWidth(105.0f);
  if (ImGui::InputInt("Trace left", &texture_trace_remaining, 100, 1000)) {
    texture_trace_remaining = std::clamp(texture_trace_remaining, 0, 1000000);
    rex::cvar::SetFlagByName("skate3_ultrawide_texture_trace_remaining",
                             std::to_string(texture_trace_remaining));
  }

  bool texture_trace_bind_keys =
      rex::cvar::Query<bool>("skate3_ultrawide_texture_trace_bind_keys");
  if (ImGui::Checkbox("Trace binds", &texture_trace_bind_keys)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_texture_trace_bind_keys",
                             texture_trace_bind_keys ? "true" : "false");
  }
  ImGui::SameLine();
  bool texture_trace_scaled_resolve =
      rex::cvar::Query<bool>("skate3_ultrawide_texture_trace_scaled_resolve");
  if (ImGui::Checkbox("Trace scaled", &texture_trace_scaled_resolve)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_texture_trace_scaled_resolve",
                             texture_trace_scaled_resolve ? "true" : "false");
  }
  bool ignore_streamer_texture_invalidations =
      rex::cvar::Query<bool>("skate3_ultrawide_ignore_streamer_texture_invalidations");
  if (ImGui::Checkbox("Pin streamer textures", &ignore_streamer_texture_invalidations)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_ignore_streamer_texture_invalidations",
                             ignore_streamer_texture_invalidations ? "true" : "false");
  }
  auto draw_fingerprints = graphics::ultrawide_debug::SnapshotDrawFingerprints();
  std::sort(draw_fingerprints.begin(), draw_fingerprints.end(),
            [](const graphics::ultrawide_debug::DrawFingerprintEntry& a,
               const graphics::ultrawide_debug::DrawFingerprintEntry& b) {
              if (a.fingerprint.bucket != b.fingerprint.bucket) {
                return uint32_t(a.fingerprint.bucket) < uint32_t(b.fingerprint.bucket);
              }
              if (a.fingerprint.packet_ptr != b.fingerprint.packet_ptr) {
                return a.fingerprint.packet_ptr < b.fingerprint.packet_ptr;
              }
              if (a.fingerprint.index_guest_base != b.fingerprint.index_guest_base) {
                return a.fingerprint.index_guest_base < b.fingerprint.index_guest_base;
              }
              return graphics::ultrawide_debug::HashDrawFingerprint(a.fingerprint) <
                     graphics::ultrawide_debug::HashDrawFingerprint(b.fingerprint);
            });
  draw_group_mode_ = std::clamp(draw_group_mode_, 0, 2);
  const auto draw_group_mode = static_cast<DrawGroupMode>(draw_group_mode_);
  auto draw_groups = BuildDrawGroups(draw_fingerprints, draw_group_mode);

  ImGui::Separator();
  bool trace_draws = rex::cvar::Query<bool>("skate3_ultrawide_trace_draws");
  if (ImGui::Checkbox("Trace draws", &trace_draws)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_trace_draws", trace_draws ? "true" : "false");
    graphics::ultrawide_debug::ClearDrawFingerprints();
    g_visible_draw_capture.clear();
    g_popped_draw_capture.clear();
  }
  bool object_stage_trace_continuous =
      rex::cvar::Query<bool>("skate3_ultrawide_object_stage_trace_continuous");
  if (ImGui::Checkbox("Object stages", &object_stage_trace_continuous)) {
    rex::cvar::SetFlagByName("skate3_ultrawide_object_stage_trace_continuous",
                             object_stage_trace_continuous ? "true" : "false");
  }
  ImGui::SameLine();
  if (ImGui::Button("Obj trace 300")) {
    rex::cvar::SetFlagByName("skate3_ultrawide_object_stage_trace_frames_remaining", "300");
  }
  ImGui::SameLine();
  if (ImGui::Button("Mark visible")) {
    rex::cvar::SetFlagByName("skate3_ultrawide_object_stage_marker", "1");
    rex::cvar::SetFlagByName("skate3_ultrawide_object_stage_trace_frames_remaining", "60");
  }
  ImGui::SameLine();
  if (ImGui::Button("Mark invisible")) {
    rex::cvar::SetFlagByName("skate3_ultrawide_object_stage_marker", "2");
    rex::cvar::SetFlagByName("skate3_ultrawide_object_stage_trace_frames_remaining", "60");
  }
  ImGui::SameLine();
  int object_stage_trace_frames = std::clamp(
      rex::cvar::Query<int32_t>("skate3_ultrawide_object_stage_trace_frames_remaining"), 0,
      1000000);
  ImGui::SetNextItemWidth(88.0f);
  if (ImGui::InputInt("Obj left", &object_stage_trace_frames, 30, 300)) {
    object_stage_trace_frames = std::clamp(object_stage_trace_frames, 0, 1000000);
    rex::cvar::SetFlagByName("skate3_ultrawide_object_stage_trace_frames_remaining",
                             std::to_string(object_stage_trace_frames));
  }
  ImGui::SameLine();
  ImGui::Text("Draw groups: %zu  Exact: %zu", draw_groups.size(), draw_fingerprints.size());
  ImGui::SameLine();
  const char* draw_group_mode_labels[] = {"Shader", "Object", "Texture"};
  ImGui::SetNextItemWidth(92.0f);
  if (ImGui::Combo("Group", &draw_group_mode_, draw_group_mode_labels,
                   IM_ARRAYSIZE(draw_group_mode_labels))) {
    draw_group_mode_ = std::clamp(draw_group_mode_, 0, 2);
  }
  ImGui::SameLine();
  ImGui::Checkbox("Expand draw section", &draw_section_expanded_);
  ImGui::SameLine();
  ImGui::BeginDisabled(!draw_section_expanded_);
  ImGui::Checkbox("Exact rows", &draw_show_exact_fingerprints_);
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Capture visible")) {
    g_visible_draw_capture = draw_fingerprints;
    graphics::ultrawide_debug::ClearDrawFingerprints();
  }
  ImGui::SameLine();
  if (ImGui::Button("Capture popped")) {
    g_popped_draw_capture = draw_fingerprints;
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear draw captures")) {
    g_visible_draw_capture.clear();
    g_popped_draw_capture.clear();
    graphics::ultrawide_debug::ClearDrawFingerprints();
  }
  ImGui::SameLine();
  if (ImGui::Button("Show all draws")) {
    graphics::ultrawide_debug::SetAllDrawFingerprintsEnabled(true);
  }
  ImGui::SameLine();
  if (ImGui::Button("Hide all draws")) {
    graphics::ultrawide_debug::SetAllDrawFingerprintsEnabled(false);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset draw visibility")) {
    graphics::ultrawide_debug::ResetDrawFingerprintOverrides();
  }

  if (g_watched_draw_group) {
    const DrawGroupEntry* watched_group = nullptr;
    for (const auto& group : draw_groups) {
      if (group.key == *g_watched_draw_group) {
        watched_group = &group;
        break;
      }
    }

    ImGui::Text("Watch: %s", Hex(g_watched_draw_group_hash, 16).c_str());
    ImGui::SameLine();
    if (ImGui::Button("Clear watch")) {
      g_watched_draw_group.reset();
      g_watched_draw_group_hash = 0;
      g_draw_watch_events.clear();
      g_draw_watch_state_valid = false;
      graphics::ultrawide_debug::ClearGuestStoreWatchTargets();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(96.0f);
    if (ImGui::InputInt("Ms threshold", &g_draw_watch_age_threshold, 10, 50)) {
      g_draw_watch_age_threshold = std::clamp(g_draw_watch_age_threshold, 1, 10000);
      g_draw_watch_state_valid = false;
    }
    ImGui::SameLine();
    if (watched_group) {
      const uint64_t now_ms = NowMilliseconds();
      const uint64_t age = watched_group->last_seen_ms == 0
                               ? UINT64_MAX
                               : (now_ms > watched_group->last_seen_ms
                                      ? now_ms - watched_group->last_seen_ms
                                      : 0);
      const uint64_t textured_age =
          watched_group->last_textured_seen_ms == 0
              ? UINT64_MAX
              : (now_ms > watched_group->last_textured_seen_ms
                     ? now_ms - watched_group->last_textured_seen_ms
                     : 0);
      const uint64_t main_textured_age =
          watched_group->last_main_textured_seen_ms == 0
              ? UINT64_MAX
              : (now_ms > watched_group->last_main_textured_seen_ms
                     ? now_ms - watched_group->last_main_textured_seen_ms
                     : 0);
      const auto& watch_fingerprint =
          watched_group->last_main_textured_seen_order != 0
              ? watched_group->main_textured_representative
              : (watched_group->last_textured_seen_order != 0
                     ? watched_group->textured_representative
                     : watched_group->representative);
      const bool watch_present = main_textured_age <= uint64_t(g_draw_watch_age_threshold);
      if (!g_draw_watch_state_valid || g_draw_watch_present != watch_present) {
        DrawWatchEvent event;
        event.sequence = ++g_draw_watch_event_sequence;
        event.present = watch_present;
        event.age = main_textured_age;
        event.packet_ptr = watch_fingerprint.packet_ptr;
        event.index_guest_base = watch_fingerprint.index_guest_base;
        event.vertex_fetch_address = watch_fingerprint.vertex_fetch_address[0];
        event.group_hash = g_watched_draw_group_hash;
        g_draw_watch_events.push_front(event);
        while (g_draw_watch_events.size() > 24) {
          g_draw_watch_events.pop_back();
        }
        graphics::ultrawide_debug::RecordDrawWatchTransition(
            event.present, event.age, event.packet_ptr, event.index_guest_base,
            event.vertex_fetch_address, event.group_hash);
        g_draw_watch_state_valid = true;
        g_draw_watch_present = watch_present;
      }
      ImGui::Text("calls=%llu textured=%llu main_tex=%llu exact=%u shown=%llu hidden=%llu",
                  static_cast<unsigned long long>(watched_group->draw_count),
                  static_cast<unsigned long long>(watched_group->textured_draw_count),
                  static_cast<unsigned long long>(watched_group->main_textured_draw_count),
                  watched_group->exact_count,
                  static_cast<unsigned long long>(watched_group->submitted_count),
                  static_cast<unsigned long long>(watched_group->skipped_count));
      ImGui::Text("any_ms=%s tex_ms=%s main_tex_ms=%s pkt=%s",
                  age == UINT64_MAX
                      ? "n/a"
                      : std::to_string(age).c_str(),
                  textured_age == UINT64_MAX
                      ? "n/a"
                      : std::to_string(textured_age).c_str(),
                  main_textured_age == UINT64_MAX
                      ? "n/a"
                      : std::to_string(main_textured_age).c_str(),
                  Hex(watch_fingerprint.packet_ptr, 8).c_str());
      ImGui::Text("Watch object: %s", DrawObjectLabel(watch_fingerprint).c_str());
      ImGui::SameLine();
      ImGui::Text("texture: %s", DrawTextureLabel(watch_fingerprint).c_str());
    } else {
      ImGui::TextUnformatted("not present in current group mode");
      if (g_draw_watch_state_valid && g_draw_watch_present) {
        DrawWatchEvent event;
        event.sequence = ++g_draw_watch_event_sequence;
        event.present = false;
        event.age = UINT64_MAX;
        event.group_hash = g_watched_draw_group_hash;
        g_draw_watch_events.push_front(event);
        while (g_draw_watch_events.size() > 24) {
          g_draw_watch_events.pop_back();
        }
        graphics::ultrawide_debug::RecordDrawWatchTransition(
            event.present, event.age, event.packet_ptr, event.index_guest_base,
            event.vertex_fetch_address, event.group_hash);
        g_draw_watch_present = false;
      }
    }
    if (!g_draw_watch_events.empty() &&
        ImGui::BeginTable("draw_watch_events", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit,
                          ImVec2(0.0f, 92.0f))) {
      ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 42.0f);
      ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 64.0f);
      ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableSetupColumn("Pkt", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("IB", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("VF0", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 132.0f);
      ImGui::TableHeadersRow();
      for (const DrawWatchEvent& event : g_draw_watch_events) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(event.sequence));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(event.present ? "present" : "missing");
        ImGui::TableNextColumn();
        if (event.age == UINT64_MAX) {
          ImGui::TextUnformatted("n/a");
        } else {
          ImGui::Text("%llu", static_cast<unsigned long long>(event.age));
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(event.packet_ptr ? Hex(event.packet_ptr, 8).c_str() : "");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(event.index_guest_base ? Hex(event.index_guest_base, 8).c_str()
                                                      : "");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(event.vertex_fetch_address
                                   ? Hex(event.vertex_fetch_address, 8).c_str()
                                   : "");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(Hex(event.group_hash, 16).c_str());
      }
      ImGui::EndTable();
    }
    const auto watch_log_path = export_path_.parent_path() / "skate3_ultrawide_watch.log";
    ImGui::Text("Watch log: %s", watch_log_path.string().c_str());
    if (!guest_store_watch_events.empty()) {
      ImGui::Text("Guest stores: %zu", guest_store_watch_events.size());
      ImGui::SameLine();
      if (ImGui::Button("Clear store hits")) {
        graphics::ultrawide_debug::ClearGuestStoreWatchEvents();
        guest_store_watch_events.clear();
      }
      if (ImGui::BeginTable("guest_store_watch_events", 8,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                ImGuiTableFlags_SizingFixedFit,
                            ImVec2(0.0f, 130.0f))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("LR", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableSetupColumn("Stack", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableHeadersRow();
        for (auto it = guest_store_watch_events.rbegin();
             it != guest_store_watch_events.rend(); ++it) {
          const auto& event = *it;
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%llu", static_cast<unsigned long long>(event.sequence));
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(event.target.c_str());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(Hex(event.address, 8).c_str());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(Hex(event.value, 8).c_str());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(event.caller_lr ? Hex(event.caller_lr, 8).c_str() : "");
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(event.function.c_str());
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(event.stack.c_str());
          ImGui::TableNextColumn();
          ImGui::Text("%llu", static_cast<unsigned long long>(event.count));
        }
        ImGui::EndTable();
      }
    }
  }

  const float draw_diff_height = draw_section_expanded_ ? 180.0f : 86.0f;
  const float draw_live_height = draw_section_expanded_ ? 260.0f : 120.0f;
  const auto draw_diff = BuildDrawDiff();
  const auto draw_group_diff = BuildDrawGroupDiff(draw_group_mode);
  ImGui::Text("Visible capture: %zu  Popped capture: %zu  Missing groups: %zu  Exact missing: %zu",
              g_visible_draw_capture.size(), g_popped_draw_capture.size(), draw_group_diff.size(),
              draw_diff.size());
  if (ImGui::BeginTable("draw_group_diff", 13,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0.0f, draw_diff_height))) {
    ImGui::TableSetupColumn("Draw", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Watch", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthFixed, 62.0f);
    ImGui::TableSetupColumn("Vis", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Pop", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Bucket", ImGuiTableColumnFlags_WidthFixed, 78.0f);
    ImGui::TableSetupColumn("Exact", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthFixed, 250.0f);
    ImGui::TableSetupColumn("VS", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableSetupColumn("PS", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableSetupColumn("Prim", ImGuiTableColumnFlags_WidthFixed, 54.0f);
    ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableHeadersRow();
    for (const auto& row : draw_group_diff) {
      const auto& fp = row.representative;
      const uint64_t group_hash = row.group_hash;
      const DrawGroupEntry* live_group = nullptr;
      for (const auto& group : draw_groups) {
        if (group.group_hash == group_hash) {
          live_group = &group;
          break;
        }
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      bool draw_enabled = !live_group || live_group->enabled_count > 0;
      std::string draw_id = "##draw_group_diff_" + Hex(group_hash, 16);
      if (ImGui::Checkbox(draw_id.c_str(), &draw_enabled)) {
        if (live_group) {
          for (uint64_t exact_hash : live_group->exact_hashes) {
            graphics::ultrawide_debug::SetDrawFingerprintEnabled(exact_hash, draw_enabled);
          }
        }
      }
      ImGui::TableNextColumn();
      std::string watch_id = "W##draw_group_diff_watch_" + Hex(group_hash, 16);
      if (ImGui::SmallButton(watch_id.c_str())) {
        SetWatchedDrawGroup(row.key, group_hash, BestWatchFingerprint(live_group, fp));
      }
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(row.visible_count - row.popped_count));
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(row.visible_count));
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(row.popped_count));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(DrawBucketLabel(fp.bucket));
      ImGui::TableNextColumn();
      ImGui::Text("%u", live_group ? live_group->exact_count : 0);
      ImGui::TableNextColumn();
      const std::string object = DrawObjectLabel(fp);
      ImGui::TextUnformatted(object.c_str());
      ImGui::TableNextColumn();
      const auto& texture_fp =
          live_group && live_group->last_main_textured_seen_order != 0
              ? live_group->main_textured_representative
              : (live_group && live_group->last_textured_seen_order != 0
                     ? live_group->textured_representative
                     : fp);
      const std::string texture = DrawTextureLabel(texture_fp);
      ImGui::TextUnformatted(texture.c_str());
      ImGui::TableNextColumn();
      const std::string vs = Hex(fp.vertex_shader_hash, 16);
      ImGui::TextUnformatted(vs.c_str());
      ImGui::TableNextColumn();
      const std::string ps = Hex(fp.pixel_shader_hash, 16);
      ImGui::TextUnformatted(ps.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u", fp.primitive_type);
      ImGui::TableNextColumn();
      const std::string hash = Hex(group_hash, 16);
      ImGui::TextUnformatted(hash.c_str());
    }
    ImGui::EndTable();
  }

  if (ImGui::BeginTable("draw_groups", 16,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0.0f, draw_live_height))) {
    ImGui::TableSetupColumn("Draw", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Watch", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Shown", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Hidden", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Bucket", ImGuiTableColumnFlags_WidthFixed, 78.0f);
    ImGui::TableSetupColumn("Exact", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Verts", ImGuiTableColumnFlags_WidthFixed, 92.0f);
    ImGui::TableSetupColumn("Prims", ImGuiTableColumnFlags_WidthFixed, 92.0f);
    ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthFixed, 250.0f);
    ImGui::TableSetupColumn("VS", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableSetupColumn("PS", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableSetupColumn("Prim", ImGuiTableColumnFlags_WidthFixed, 54.0f);
    ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableHeadersRow();
    for (const auto& group : draw_groups) {
      const auto& fp = group.representative;
      const uint64_t group_hash = group.group_hash;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      bool draw_enabled = group.enabled_count > 0;
      std::string draw_id = "##draw_group_live_" + Hex(group_hash, 16);
      if (ImGui::Checkbox(draw_id.c_str(), &draw_enabled)) {
        for (uint64_t exact_hash : group.exact_hashes) {
          graphics::ultrawide_debug::SetDrawFingerprintEnabled(exact_hash, draw_enabled);
        }
      }
      ImGui::TableNextColumn();
      std::string watch_id = "W##draw_group_live_watch_" + Hex(group_hash, 16);
      if (ImGui::SmallButton(watch_id.c_str())) {
        SetWatchedDrawGroup(group.key, group_hash, BestWatchFingerprint(&group, fp));
      }
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(group.draw_count));
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(group.submitted_count));
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(group.skipped_count));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(DrawBucketLabel(fp.bucket));
      ImGui::TableNextColumn();
      ImGui::Text("%u", group.exact_count);
      ImGui::TableNextColumn();
      if (group.min_vertex_count == group.max_vertex_count) {
        ImGui::Text("%u", group.min_vertex_count);
      } else {
        ImGui::Text("%u-%u", group.min_vertex_count, group.max_vertex_count);
      }
      ImGui::TableNextColumn();
      if (group.min_primitive_count == group.max_primitive_count) {
        ImGui::Text("%u", group.min_primitive_count);
      } else {
        ImGui::Text("%u-%u", group.min_primitive_count, group.max_primitive_count);
      }
      ImGui::TableNextColumn();
      const std::string object = DrawObjectLabel(fp);
      ImGui::TextUnformatted(object.c_str());
      ImGui::TableNextColumn();
      const auto& texture_fp = group.last_main_textured_seen_order != 0
                                   ? group.main_textured_representative
                                   : (group.last_textured_seen_order != 0
                                          ? group.textured_representative
                                          : fp);
      const std::string texture = DrawTextureLabel(texture_fp);
      ImGui::TextUnformatted(texture.c_str());
      ImGui::TableNextColumn();
      const std::string vs = Hex(fp.vertex_shader_hash, 16);
      ImGui::TextUnformatted(vs.c_str());
      ImGui::TableNextColumn();
      const std::string ps = Hex(fp.pixel_shader_hash, 16);
      ImGui::TextUnformatted(ps.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u", fp.primitive_type);
      ImGui::TableNextColumn();
      ImGui::Text("%u/%u", group.enabled_count, group.exact_count);
      ImGui::TableNextColumn();
      const std::string hash = Hex(group_hash, 16);
      ImGui::TextUnformatted(hash.c_str());
    }
    ImGui::EndTable();
  }

  if (draw_section_expanded_ && draw_show_exact_fingerprints_ &&
      ImGui::BeginTable("draw_fingerprints", 18,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0.0f, 180.0f))) {
    ImGui::TableSetupColumn("Draw", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Shown", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Hidden", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Bucket", ImGuiTableColumnFlags_WidthFixed, 78.0f);
    ImGui::TableSetupColumn("Obj", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("VT", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Verts", ImGuiTableColumnFlags_WidthFixed, 62.0f);
    ImGui::TableSetupColumn("Prims", ImGuiTableColumnFlags_WidthFixed, 62.0f);
    ImGui::TableSetupColumn("VS", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableSetupColumn("PS", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableSetupColumn("Pkt", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("IB", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("IB Len", ImGuiTableColumnFlags_WidthFixed, 62.0f);
    ImGui::TableSetupColumn("VF0", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("VF1", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableHeadersRow();
    for (const auto& entry : draw_fingerprints) {
      const auto& fp = entry.fingerprint;
      const uint64_t fp_hash = entry.hash ? entry.hash : graphics::ultrawide_debug::HashDrawFingerprint(fp);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      bool draw_enabled = entry.enabled;
      std::string draw_id = "##draw_live_" + Hex(fp_hash, 16);
      if (ImGui::Checkbox(draw_id.c_str(), &draw_enabled)) {
        graphics::ultrawide_debug::SetDrawFingerprintEnabled(fp_hash, draw_enabled);
      }
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(entry.draw_count));
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(entry.submitted_count));
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(entry.skipped_count));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(DrawBucketLabel(fp.bucket));
      ImGui::TableNextColumn();
      const std::string owner = fp.guest_render_object ? Hex(fp.guest_render_object, 8) : "";
      ImGui::TextUnformatted(owner.c_str());
      ImGui::TableNextColumn();
      const std::string owner_vtable = fp.guest_render_vtable ? Hex(fp.guest_render_vtable, 8) : "";
      ImGui::TextUnformatted(owner_vtable.c_str());
      ImGui::TableNextColumn();
      const std::string owner_target = fp.guest_render_target ? Hex(fp.guest_render_target, 8) : "";
      ImGui::TextUnformatted(owner_target.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u", fp.vertex_count);
      ImGui::TableNextColumn();
      ImGui::Text("%u", fp.primitive_count);
      ImGui::TableNextColumn();
      const std::string vs = Hex(fp.vertex_shader_hash, 16);
      ImGui::TextUnformatted(vs.c_str());
      ImGui::TableNextColumn();
      const std::string ps = Hex(fp.pixel_shader_hash, 16);
      ImGui::TextUnformatted(ps.c_str());
      ImGui::TableNextColumn();
      const std::string pkt = fp.packet_ptr ? Hex(fp.packet_ptr, 8) : "";
      ImGui::TextUnformatted(pkt.c_str());
      ImGui::TableNextColumn();
      const std::string ib = fp.index_guest_base ? Hex(fp.index_guest_base, 8) : "";
      ImGui::TextUnformatted(ib.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u", fp.index_length);
      ImGui::TableNextColumn();
      const std::string vf0 = fp.vertex_fetch_address[0] ? Hex(fp.vertex_fetch_address[0], 8) : "";
      ImGui::TextUnformatted(vf0.c_str());
      ImGui::TableNextColumn();
      const std::string vf1 = fp.vertex_fetch_address[1] ? Hex(fp.vertex_fetch_address[1], 8) : "";
      ImGui::TextUnformatted(vf1.c_str());
      ImGui::TableNextColumn();
      const std::string hash = Hex(fp_hash, 16);
      ImGui::TextUnformatted(hash.c_str());
    }
    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Targets: %zu", targets.size());
  ImGui::SameLine();
  ImGui::Text("Screen read sites: %zu", screen_callbacks.size());
  ImGui::SameLine();
  ImGui::Text("Target aspect: %.4f",
              rex::cvar::Query<double>("skate3_ultrawide_target_aspect"));
  ImGui::SameLine();
  ImGui::Text("Export: %s", export_path_.string().c_str());

  if (ImGui::Button("Enable screen reads")) {
    graphics::ultrawide_debug::SetAllScreenCallbacksEnabled(true);
  }
  ImGui::SameLine();
  if (ImGui::Button("Disable screen reads")) {
    graphics::ultrawide_debug::SetAllScreenCallbacksEnabled(false);
  }

  if (ImGui::BeginTable("screen_callbacks", 8,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0.0f, 130.0f))) {
    ImGui::TableSetupColumn("Spoof", ImGuiTableColumnFlags_WidthFixed, 54.0f);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 86.0f);
    ImGui::TableSetupColumn("Site", ImGuiTableColumnFlags_WidthFixed, 94.0f);
    ImGui::TableSetupColumn("Native", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Returned", ImGuiTableColumnFlags_WidthFixed, 78.0f);
    ImGui::TableSetupColumn("Spoofed", ImGuiTableColumnFlags_WidthFixed, 62.0f);
    ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 82.0f);
    ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableHeadersRow();
    for (const auto& entry : screen_callbacks) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      bool enabled = entry.enabled;
      const std::string checkbox_id = "##screen_" + Hex(entry.hash, 16);
      if (ImGui::Checkbox(checkbox_id.c_str(), &enabled)) {
        graphics::ultrawide_debug::SetScreenCallbackEnabled(entry.hash, enabled);
      }
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(ScreenCallbackKindLabel(entry.kind));
      ImGui::TableNextColumn();
      const std::string lr = Hex(entry.caller_lr, 8);
      ImGui::TextUnformatted(lr.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u", entry.last_value);
      ImGui::TableNextColumn();
      ImGui::Text("%u", entry.last_returned_value);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(entry.last_spoofed ? "yes" : "no");
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(entry.call_count));
      ImGui::TableNextColumn();
      const std::string hash = Hex(entry.hash, 16);
      ImGui::TextUnformatted(hash.c_str());
    }
    ImGui::EndTable();
  }

  const ImGuiTableFlags table_flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("ultrawide_targets", 13, table_flags, ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("Apply", ImGuiTableColumnFlags_WidthFixed, 54.0f);
    ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Shadow", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 285.0f);
    ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    ImGui::TableSetupColumn("RT0");
    ImGui::TableSetupColumn("Depth");
    ImGui::TableSetupColumn("Surface");
    ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthFixed, 118.0f);
    ImGui::TableSetupColumn("DepthCtl");
    ImGui::TableSetupColumn("VTE");
    ImGui::TableSetupColumn("VS");
    ImGui::TableSetupColumn("Draws");
    ImGui::TableHeadersRow();

    for (const auto& target : targets) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      bool enabled = target.enabled;
      const std::string checkbox_id = "##apply_" + Hex(target.hash, 16);
      if (ImGui::Checkbox(checkbox_id.c_str(), &enabled)) {
        graphics::ultrawide_debug::SetTargetEnabled(target.hash, enabled);
      }

      ImGui::TableNextColumn();
      ImGui::TextUnformatted(target.default_enabled ? "yes" : "no");
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(target.shadow_candidate ? "yes" : "no");
      ImGui::TableNextColumn();
      const std::string group = TargetGroupLabel(target);
      ImGui::TextUnformatted(group.c_str());
      ImGui::TableNextColumn();
      const std::string hash = Hex(target.hash, 16);
      ImGui::TextUnformatted(hash.c_str());
      ImGui::TableNextColumn();
      const std::string rt0 = Hex(target.key.color_info[0], 8);
      ImGui::TextUnformatted(rt0.c_str());
      ImGui::TableNextColumn();
      const std::string depth = Hex(target.key.depth_info, 8);
      ImGui::TextUnformatted(depth.c_str());
      ImGui::TableNextColumn();
      const std::string surface = Hex(target.key.surface_info, 8);
      ImGui::TextUnformatted(surface.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%u,%u %ux%u", target.key.viewport_x, target.key.viewport_y,
                  target.key.viewport_width, target.key.viewport_height);
      ImGui::TableNextColumn();
      const std::string depth_control = Hex(target.key.depth_control, 8);
      ImGui::TextUnformatted(depth_control.c_str());
      ImGui::TableNextColumn();
      const std::string vte = Hex(target.key.pa_cl_vte_cntl, 8);
      ImGui::TextUnformatted(vte.c_str());
      ImGui::TableNextColumn();
      const std::string vs = Hex(target.key.vertex_shader_hash, 16);
      ImGui::TextUnformatted(vs.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%llu / %llu", static_cast<unsigned long long>(target.applied_draw_count),
                  static_cast<unsigned long long>(target.draw_count));
    }
    ImGui::EndTable();
  }

  ImGui::End();
  if (!open) {
    Hide();
  }
}

}  // namespace rex::ui
