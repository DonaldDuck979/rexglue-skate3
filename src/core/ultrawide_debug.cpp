#include <rex/graphics/ultrawide_debug.h>

#include <rex/cvar.h>
#include <rex/logging.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <toml++/toml.hpp>

REXCVAR_DEFINE_BOOL(skate3_ultrawide_screen_callback_tracking, false, "Skate 3/Ultrawide",
                    "Track guest screen callback callers in the F7 ultrawide overlay")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace rex::graphics::ultrawide_debug {

namespace {

void HashAdd(uint64_t& hash, uint64_t value);

struct TargetKeyHash {
  size_t operator()(const TargetKey& key) const {
    return size_t(HashKey(key));
  }
};

enum class SemanticViewportClass : uint32_t {
  kUnknown = 0,
  kGameAspect = 1,
  kDoubleWideGameAspect = 2,
  kSquare = 3,
};

struct NormalizedViewport {
  SemanticViewportClass viewport_class = SemanticViewportClass::kUnknown;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

struct SemanticTargetKey {
  SemanticViewportClass viewport_class = SemanticViewportClass::kUnknown;
  uint32_t viewport_x = 0;
  uint32_t viewport_y = 0;
  uint32_t viewport_width = 0;
  uint32_t viewport_height = 0;
  uint32_t depth_info = 0;
  uint32_t surface_info = 0;
  uint32_t color_mask = 0;
  uint32_t depth_control = 0;
  uint32_t pa_cl_vte_cntl = 0;
  uint32_t primitive_type = 0;
  uint32_t host_vertex_shader_type = 0;
  uint64_t vertex_shader_hash = 0;
  uint64_t pixel_shader_hash = 0;

  bool operator==(const SemanticTargetKey& other) const {
    return viewport_class == other.viewport_class && color_mask == other.color_mask &&
           viewport_x == other.viewport_x && viewport_y == other.viewport_y &&
           viewport_width == other.viewport_width && viewport_height == other.viewport_height &&
           depth_info == other.depth_info && surface_info == other.surface_info &&
           depth_control == other.depth_control && pa_cl_vte_cntl == other.pa_cl_vte_cntl &&
           primitive_type == other.primitive_type &&
           host_vertex_shader_type == other.host_vertex_shader_type &&
           vertex_shader_hash == other.vertex_shader_hash &&
           pixel_shader_hash == other.pixel_shader_hash;
  }
};

struct SemanticTargetKeyHash {
  size_t operator()(const SemanticTargetKey& key) const {
    uint64_t hash = 1469598103934665603ull;
    HashAdd(hash, uint32_t(key.viewport_class));
    HashAdd(hash, key.viewport_x);
    HashAdd(hash, key.viewport_y);
    HashAdd(hash, key.viewport_width);
    HashAdd(hash, key.viewport_height);
    HashAdd(hash, key.depth_info);
    HashAdd(hash, key.surface_info);
    HashAdd(hash, key.color_mask);
    HashAdd(hash, key.depth_control);
    HashAdd(hash, key.pa_cl_vte_cntl);
    HashAdd(hash, key.primitive_type);
    HashAdd(hash, key.host_vertex_shader_type);
    HashAdd(hash, key.vertex_shader_hash);
    HashAdd(hash, key.pixel_shader_hash);
    return size_t(hash);
  }
};

struct BuiltInTargetOverride {
  uint64_t hash = 0;
  SemanticTargetKey key;
  bool enabled = false;
};

#include "skate3_ultrawide_classifier.inc"

struct DrawFingerprintKey {
  DrawFingerprint fingerprint;

  bool operator==(const DrawFingerprintKey& other) const {
    const DrawFingerprint& a = fingerprint;
    const DrawFingerprint& b = other.fingerprint;
    return a.bucket == b.bucket && a.guest_render_object == b.guest_render_object &&
           a.guest_render_vtable == b.guest_render_vtable &&
           a.guest_render_target == b.guest_render_target &&
           a.vertex_shader_hash == b.vertex_shader_hash &&
           a.pixel_shader_hash == b.pixel_shader_hash && a.primitive_type == b.primitive_type &&
           a.vertex_count == b.vertex_count && a.primitive_count == b.primitive_count &&
           a.index_guest_base == b.index_guest_base && a.index_length == b.index_length &&
           std::equal(std::begin(a.vertex_fetch_address), std::end(a.vertex_fetch_address),
                      std::begin(b.vertex_fetch_address)) &&
           std::equal(std::begin(a.vertex_fetch_size), std::end(a.vertex_fetch_size),
                      std::begin(b.vertex_fetch_size)) &&
           std::equal(std::begin(a.texture_key_hash), std::end(a.texture_key_hash),
                      std::begin(b.texture_key_hash)) &&
           std::equal(std::begin(a.texture_fetch_index), std::end(a.texture_fetch_index),
                      std::begin(b.texture_fetch_index));
  }
};

struct DrawFingerprintKeyHash {
  size_t operator()(const DrawFingerprintKey& key) const {
    return size_t(HashDrawFingerprint(key.fingerprint));
  }
};

std::mutex g_mutex;
std::unordered_map<TargetKey, TargetEntry, TargetKeyHash> g_targets;
std::unordered_map<uint64_t, bool> g_hash_overrides;
// std::atomic<std::shared_ptr<T>> is not available on every target toolchain
// (libc++ gates it behind a newer macOS deployment target than we target), so
// publish copy-on-write snapshots through a small mutex-guarded holder. The
// expensive map rebuild happens before store(), outside this lock, so readers
// never block on a writer's rebuild -- only the pointer swap/copy is guarded.
template <typename T>
class SnapshotPtr {
 public:
  SnapshotPtr() = default;
  SnapshotPtr(std::shared_ptr<T> initial) : ptr_(std::move(initial)) {}
  std::shared_ptr<T> load(std::memory_order = std::memory_order_seq_cst) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ptr_;
  }
  void store(std::shared_ptr<T> next, std::memory_order = std::memory_order_seq_cst) {
    std::lock_guard<std::mutex> lock(mutex_);
    ptr_ = std::move(next);
  }

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<T> ptr_;
};

SnapshotPtr<const std::unordered_map<uint64_t, bool>> g_hash_overrides_snapshot =
    std::make_shared<const std::unordered_map<uint64_t, bool>>();
std::unordered_map<SemanticTargetKey, bool, SemanticTargetKeyHash> g_semantic_overrides;
SnapshotPtr<const std::unordered_map<SemanticTargetKey, bool, SemanticTargetKeyHash>>
    g_semantic_overrides_snapshot =
        std::make_shared<const std::unordered_map<SemanticTargetKey, bool,
                                                 SemanticTargetKeyHash>>();
std::unordered_map<uint64_t, ScreenCallbackEntry> g_screen_callbacks;
std::unordered_map<uint64_t, bool> g_screen_callback_overrides;
std::deque<GuestStoreWatchEntry> g_guest_store_watch_events;
std::unordered_map<DrawFingerprintKey, DrawFingerprintEntry, DrawFingerprintKeyHash>
    g_draw_fingerprints;
std::unordered_map<uint64_t, bool> g_draw_fingerprint_overrides;
uint64_t g_seen_order = 0;
uint64_t g_guest_store_watch_sequence = 0;
uint64_t g_guest_store_watch_group_hash = 0;
std::filesystem::path g_guest_store_watch_log_path;
std::ofstream g_guest_store_watch_log;
std::atomic<ApplyMode> g_apply_mode = ApplyMode::kTargetList;
std::atomic<bool> g_target_overrides_loaded = false;
std::atomic<bool> g_builtin_target_overrides_active = false;
std::atomic<bool> g_target_recording_active = false;
std::atomic<bool> g_skate3_gameplay_ultrawide_latched = false;
std::atomic<uint32_t> g_target_decision_cache_epoch = 1;
std::atomic<bool> g_guest_store_watch_enabled = false;
std::atomic<uint32_t> g_guest_store_watch_index_base = 0;
std::atomic<uint32_t> g_guest_store_watch_vertex_fetch_address = 0;
std::atomic<uint32_t> g_guest_store_watch_texture_base_address = 0;
constexpr size_t kMaxGuestStoreWatchEvents = 256;
constexpr size_t kMaxDrawFingerprints = 4096;
constexpr size_t kMaxGuestFunctionStackDepth = 96;
thread_local const char* g_guest_function_stack[kMaxGuestFunctionStackDepth] = {};
thread_local uint32_t g_guest_function_stack_depth = 0;
thread_local uint32_t g_current_draw_owner_object = 0;
thread_local uint32_t g_current_draw_owner_vtable = 0;
thread_local uint32_t g_current_draw_owner_target = 0;

void InvalidateTargetDecisionCache() {
  g_target_decision_cache_epoch.fetch_add(1, std::memory_order_acq_rel);
}

void PublishHashOverridesSnapshotLocked() {
  g_hash_overrides_snapshot.store(
      std::make_shared<const std::unordered_map<uint64_t, bool>>(g_hash_overrides),
      std::memory_order_release);
  InvalidateTargetDecisionCache();
}

void PublishSemanticOverridesSnapshotLocked() {
  g_semantic_overrides_snapshot.store(
      std::make_shared<const std::unordered_map<SemanticTargetKey, bool, SemanticTargetKeyHash>>(
          g_semantic_overrides),
      std::memory_order_release);
  InvalidateTargetDecisionCache();
}

void InstallBuiltInSkate3ClassifierLocked() {
  std::unordered_map<uint64_t, bool> loaded_overrides;
  std::unordered_map<SemanticTargetKey, bool, SemanticTargetKeyHash> loaded_semantic_overrides;
  loaded_overrides.reserve(
      sizeof(kBuiltInSkate3TargetOverrides) / sizeof(kBuiltInSkate3TargetOverrides[0]));
  loaded_semantic_overrides.reserve(
      sizeof(kBuiltInSkate3TargetOverrides) / sizeof(kBuiltInSkate3TargetOverrides[0]));
  for (const BuiltInTargetOverride& target : kBuiltInSkate3TargetOverrides) {
    loaded_overrides[target.hash] = target.enabled;
    loaded_semantic_overrides[target.key] = target.enabled;
  }

  g_hash_overrides = std::move(loaded_overrides);
  PublishHashOverridesSnapshotLocked();
  g_semantic_overrides = std::move(loaded_semantic_overrides);
  PublishSemanticOverridesSnapshotLocked();
  g_apply_mode.store(ApplyMode::kTargetList, std::memory_order_relaxed);
  g_builtin_target_overrides_active.store(true, std::memory_order_release);
  g_target_overrides_loaded.store(true, std::memory_order_release);
}

void EnsureBuiltInSkate3ClassifierLoaded() {
  if (g_target_overrides_loaded.load(std::memory_order_acquire)) {
    return;
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_target_overrides_loaded.load(std::memory_order_relaxed)) {
    return;
  }
  InstallBuiltInSkate3ClassifierLocked();
}

void HashAdd(uint64_t& hash, uint64_t value) {
  for (size_t i = 0; i < sizeof(value); ++i) {
    hash ^= uint8_t(value >> (i * 8));
    hash *= 1099511628211ull;
  }
}

void WriteHex(std::ostream& stream, uint64_t value, uint32_t width) {
  stream << "0x" << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << value
         << std::dec << std::setfill(' ');
}

std::string_view ScreenCallbackKindName(ScreenCallbackKind kind) {
  switch (kind) {
    case ScreenCallbackKind::kWidth:
      return "width";
    case ScreenCallbackKind::kHeight:
      return "height";
    case ScreenCallbackKind::kScreenFlag:
      return "screen_flag";
    case ScreenCallbackKind::kWide:
      return "wide";
  }
  return "unknown";
}

std::optional<ScreenCallbackKind> ParseScreenCallbackKind(std::string_view name) {
  if (name == "width") {
    return ScreenCallbackKind::kWidth;
  }
  if (name == "height") {
    return ScreenCallbackKind::kHeight;
  }
  if (name == "screen_flag") {
    return ScreenCallbackKind::kScreenFlag;
  }
  if (name == "wide") {
    return ScreenCallbackKind::kWide;
  }
  return std::nullopt;
}

std::optional<uint64_t> ParseHexString(std::string_view text) {
  if (text.starts_with("0x") || text.starts_with("0X")) {
    text.remove_prefix(2);
  }
  if (text.empty()) {
    return std::nullopt;
  }

  uint64_t value = 0;
  for (char c : text) {
    uint32_t digit = 0;
    if (c >= '0' && c <= '9') {
      digit = uint32_t(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      digit = uint32_t(c - 'a') + 10;
    } else if (c >= 'A' && c <= 'F') {
      digit = uint32_t(c - 'A') + 10;
    } else {
      return std::nullopt;
    }
    value = (value << 4) | digit;
  }
  return value;
}

std::optional<uint32_t> ParseU32TomlValue(toml::node_view<toml::node> value) {
  if (auto integer = value.value<int64_t>()) {
    if (*integer >= 0 && *integer <= UINT32_MAX) {
      return uint32_t(*integer);
    }
    return std::nullopt;
  }
  if (auto text = value.value<std::string>()) {
    const auto parsed = ParseHexString(*text);
    if (parsed && *parsed <= UINT32_MAX) {
      return uint32_t(*parsed);
    }
  }
  return std::nullopt;
}

std::optional<uint64_t> ParseU64TomlValue(toml::node_view<toml::node> value) {
  if (auto integer = value.value<int64_t>()) {
    if (*integer >= 0) {
      return uint64_t(*integer);
    }
    return std::nullopt;
  }
  if (auto text = value.value<std::string>()) {
    return ParseHexString(*text);
  }
  return std::nullopt;
}

SemanticViewportClass ClassifyViewport(uint32_t width, uint32_t height) {
  if (!width || !height) {
    return SemanticViewportClass::kUnknown;
  }

  const double aspect = double(width) / double(height);
  if (std::abs(aspect - (16.0 / 9.0)) <= 0.08) {
    return SemanticViewportClass::kGameAspect;
  }
  if (std::abs(aspect - (32.0 / 9.0)) <= 0.16) {
    return SemanticViewportClass::kDoubleWideGameAspect;
  }
  if (std::abs(aspect - 1.0) <= 0.03) {
    return SemanticViewportClass::kSquare;
  }
  return SemanticViewportClass::kUnknown;
}

uint32_t DivRound(uint64_t value, uint64_t divisor) {
  if (!divisor) {
    return value > UINT32_MAX ? UINT32_MAX : uint32_t(value);
  }
  const uint64_t result = (value + divisor / 2) / divisor;
  return result > UINT32_MAX ? UINT32_MAX : uint32_t(result);
}

std::optional<NormalizedViewport> NormalizeKnownViewport(uint32_t x, uint32_t y, uint32_t width,
                                                         uint32_t height) {
  struct KnownViewportSize {
    uint32_t width;
    uint32_t height;
  };
  struct KnownViewportScale {
    uint32_t numerator;
    uint32_t denominator;
  };
  constexpr KnownViewportSize kKnownBaseSizes[] = {
      {2304, 1280}, {2304, 640}, {2560, 1440}, {64, 32},    {144, 80},
      {576, 320},   {1024, 1024}, {16384, 16384},
  };
  constexpr KnownViewportScale kKnownScales[] = {
      {1, 2}, {1, 1}, {3, 2}, {2, 1}, {3, 1}, {4, 1},
  };

  for (const KnownViewportSize& base : kKnownBaseSizes) {
    if (!base.width || !base.height) {
      continue;
    }
    for (const KnownViewportScale& scale : kKnownScales) {
      if (uint64_t(base.width) * scale.numerator != uint64_t(width) * scale.denominator ||
          uint64_t(base.height) * scale.numerator != uint64_t(height) * scale.denominator) {
        continue;
      }

      NormalizedViewport viewport;
      viewport.viewport_class = ClassifyViewport(base.width, base.height);
      viewport.x = DivRound(uint64_t(x) * scale.denominator, scale.numerator);
      viewport.y = DivRound(uint64_t(y) * scale.denominator, scale.numerator);
      viewport.width = base.width;
      viewport.height = base.height;
      return viewport;
    }
  }
  return std::nullopt;
}

NormalizedViewport NormalizeViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
  NormalizedViewport viewport;
  viewport.viewport_class = ClassifyViewport(width, height);
  viewport.x = x;
  viewport.y = y;
  viewport.width = width;
  viewport.height = height;

  if (const auto normalized_viewport = NormalizeKnownViewport(x, y, width, height)) {
    return *normalized_viewport;
  }

  return viewport;
}

SemanticTargetKey MakeSemanticTargetKey(const TargetKey& key) {
  SemanticTargetKey semantic_key;
  const NormalizedViewport viewport =
      NormalizeViewport(key.viewport_x, key.viewport_y, key.viewport_width, key.viewport_height);
  semantic_key.viewport_class = viewport.viewport_class;
  semantic_key.viewport_x = viewport.x;
  semantic_key.viewport_y = viewport.y;
  semantic_key.viewport_width = viewport.width;
  semantic_key.viewport_height = viewport.height;
  semantic_key.depth_info = key.depth_info;
  semantic_key.surface_info = key.surface_info;
  semantic_key.color_mask = key.color_mask;
  semantic_key.depth_control = key.depth_control;
  semantic_key.pa_cl_vte_cntl = key.pa_cl_vte_cntl;
  semantic_key.primitive_type = key.primitive_type;
  semantic_key.host_vertex_shader_type = key.host_vertex_shader_type;
  semantic_key.vertex_shader_hash = key.vertex_shader_hash;
  semantic_key.pixel_shader_hash = key.pixel_shader_hash;
  return semantic_key;
}

std::optional<SemanticTargetKey> ParseSemanticTargetKey(toml::table& target) {
  auto* viewport = target["viewport"].as_array();
  if (!viewport || viewport->size() < 4) {
    return std::nullopt;
  }

  const auto viewport_x = viewport->get(0) ? viewport->get(0)->value<int64_t>() : std::nullopt;
  const auto viewport_y = viewport->get(1) ? viewport->get(1)->value<int64_t>() : std::nullopt;
  const auto viewport_width = viewport->get(2) ? viewport->get(2)->value<int64_t>() : std::nullopt;
  const auto viewport_height =
      viewport->get(3) ? viewport->get(3)->value<int64_t>() : std::nullopt;
  const auto depth_info = ParseU32TomlValue(target["depth_info"]);
  const auto surface_info = ParseU32TomlValue(target["surface_info"]);
  const auto color_mask = ParseU32TomlValue(target["color_mask"]);
  const auto depth_control = ParseU32TomlValue(target["depth_control"]);
  const auto pa_cl_vte_cntl = ParseU32TomlValue(target["pa_cl_vte_cntl"]);
  const auto primitive_type = ParseU32TomlValue(target["primitive_type"]);
  const auto host_vertex_shader_type = ParseU32TomlValue(target["host_vertex_shader_type"]);
  const auto vertex_shader_hash = ParseU64TomlValue(target["vertex_shader_hash"]);
  const auto pixel_shader_hash = ParseU64TomlValue(target["pixel_shader_hash"]);
  if (!viewport_x || !viewport_y || !viewport_width || !viewport_height || *viewport_x < 0 ||
      *viewport_y < 0 || *viewport_width < 0 || *viewport_height < 0 ||
      *viewport_x > UINT32_MAX || *viewport_y > UINT32_MAX || *viewport_width > UINT32_MAX ||
      *viewport_height > UINT32_MAX || !depth_info || !surface_info || !color_mask ||
      !depth_control || !pa_cl_vte_cntl || !primitive_type || !host_vertex_shader_type ||
      !vertex_shader_hash || !pixel_shader_hash) {
    return std::nullopt;
  }

  SemanticTargetKey semantic_key;
  const NormalizedViewport normalized_viewport =
      NormalizeViewport(uint32_t(*viewport_x), uint32_t(*viewport_y), uint32_t(*viewport_width),
                        uint32_t(*viewport_height));
  semantic_key.viewport_class = normalized_viewport.viewport_class;
  semantic_key.viewport_x = normalized_viewport.x;
  semantic_key.viewport_y = normalized_viewport.y;
  semantic_key.viewport_width = normalized_viewport.width;
  semantic_key.viewport_height = normalized_viewport.height;
  semantic_key.depth_info = *depth_info;
  semantic_key.surface_info = *surface_info;
  semantic_key.color_mask = *color_mask;
  semantic_key.depth_control = *depth_control;
  semantic_key.pa_cl_vte_cntl = *pa_cl_vte_cntl;
  semantic_key.primitive_type = *primitive_type;
  semantic_key.host_vertex_shader_type = *host_vertex_shader_type;
  semantic_key.vertex_shader_hash = *vertex_shader_hash;
  semantic_key.pixel_shader_hash = *pixel_shader_hash;
  return semantic_key;
}

std::optional<bool> FindSemanticOverride(const TargetKey& key) {
  const auto overrides = g_semantic_overrides_snapshot.load(std::memory_order_acquire);
  if (!overrides) {
    return std::nullopt;
  }
  const auto semantic_key = MakeSemanticTargetKey(key);
  const auto it = overrides->find(semantic_key);
  if (it == overrides->end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<bool> FindBuiltInFallbackOverride(const TargetKey& key) {
  if (!g_builtin_target_overrides_active.load(std::memory_order_acquire)) {
    return std::nullopt;
  }

  const NormalizedViewport viewport =
      NormalizeViewport(key.viewport_x, key.viewport_y, key.viewport_width, key.viewport_height);
  if (viewport.x == 0 && viewport.y == 0 && viewport.width == 2304 && viewport.height == 640) {
    return true;
  }
  return std::nullopt;
}

uint64_t NowMilliseconds() {
  return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count());
}

void OpenGuestStoreWatchLogLocked(bool truncate) {
  if (g_guest_store_watch_log_path.empty()) {
    return;
  }
  if (g_guest_store_watch_log.is_open()) {
    return;
  }
  std::error_code error;
  std::filesystem::create_directories(g_guest_store_watch_log_path.parent_path(), error);
  g_guest_store_watch_log.open(
      g_guest_store_watch_log_path,
      std::ios::out | (truncate ? std::ios::trunc : std::ios::app));
  if (g_guest_store_watch_log) {
    g_guest_store_watch_log
        << "ms,event,seq,group,target,state,age,pkt,ib,vf0,tex,addr,value,caller_lr,function,stack,count\n";
  }
}

void WriteGuestStoreWatchTargetsLocked(const char* event_name) {
  OpenGuestStoreWatchLogLocked(false);
  if (!g_guest_store_watch_log) {
    return;
  }
  const uint32_t index_base =
      g_guest_store_watch_index_base.load(std::memory_order_relaxed);
  const uint32_t vertex_fetch_address =
      g_guest_store_watch_vertex_fetch_address.load(std::memory_order_relaxed);
  const uint32_t texture_base_address =
      g_guest_store_watch_texture_base_address.load(std::memory_order_relaxed);
  g_guest_store_watch_log << NowMilliseconds() << "," << event_name << ",0,";
  WriteHex(g_guest_store_watch_log, g_guest_store_watch_group_hash, 16);
  g_guest_store_watch_log << ",,,0,,";
  WriteHex(g_guest_store_watch_log, index_base, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, vertex_fetch_address, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, texture_base_address, 8);
  g_guest_store_watch_log << ",,,,,0\n";
  g_guest_store_watch_log.flush();
}

void WriteGuestStoreWatchStoreLocked(const GuestStoreWatchEntry& entry) {
  OpenGuestStoreWatchLogLocked(false);
  if (!g_guest_store_watch_log) {
    return;
  }
  const uint32_t index_base =
      g_guest_store_watch_index_base.load(std::memory_order_relaxed);
  const uint32_t vertex_fetch_address =
      g_guest_store_watch_vertex_fetch_address.load(std::memory_order_relaxed);
  const uint32_t texture_base_address =
      g_guest_store_watch_texture_base_address.load(std::memory_order_relaxed);
  g_guest_store_watch_log << NowMilliseconds() << ",store," << entry.sequence << ",";
  WriteHex(g_guest_store_watch_log, g_guest_store_watch_group_hash, 16);
  g_guest_store_watch_log << "," << entry.target << ",,,0,";
  WriteHex(g_guest_store_watch_log, index_base, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, vertex_fetch_address, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, texture_base_address, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, entry.address, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, entry.value, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, entry.caller_lr, 8);
  g_guest_store_watch_log << "," << entry.function << "," << entry.stack << "," << entry.count
                          << "\n";
  g_guest_store_watch_log.flush();
}

const char* DrawBucketName(DrawBucket bucket) {
  switch (bucket) {
    case DrawBucket::kMainColorDepth:
      return "Main";
    case DrawBucket::kDepthOnly:
      return "Depth";
    case DrawBucket::kCopyResolve:
      return "Copy";
    case DrawBucket::kMemexport:
      return "Memexport";
    case DrawBucket::kNoPixelShader:
      return "NoPS";
  }
  return "Unknown";
}

bool DrawHasWatchedTexture(const DrawFingerprint& fingerprint, uint32_t texture_base_address) {
  if (texture_base_address == 0) {
    return false;
  }
  for (uint32_t address : fingerprint.texture_base_address) {
    if (address == texture_base_address) {
      return true;
    }
  }
  return false;
}

bool DrawHasWatchedVertexFetch(const DrawFingerprint& fingerprint,
                               uint32_t vertex_fetch_address) {
  if (vertex_fetch_address == 0) {
    return true;
  }
  for (uint32_t address : fingerprint.vertex_fetch_address) {
    if (address == vertex_fetch_address) {
      return true;
    }
  }
  return false;
}

bool DrawMatchesGuestStoreWatchTargets(const DrawFingerprint& fingerprint) {
  if (!g_guest_store_watch_enabled.load(std::memory_order_relaxed)) {
    return false;
  }
  const uint32_t index_base =
      g_guest_store_watch_index_base.load(std::memory_order_relaxed);
  const uint32_t vertex_fetch_address =
      g_guest_store_watch_vertex_fetch_address.load(std::memory_order_relaxed);
  if (index_base != 0 && fingerprint.index_guest_base != index_base) {
    return false;
  }
  return DrawHasWatchedVertexFetch(fingerprint, vertex_fetch_address);
}

void WriteGuestStoreWatchDrawLocked(const DrawFingerprint& fingerprint, bool submitted,
                                    uint64_t exact_hash) {
  if (!DrawMatchesGuestStoreWatchTargets(fingerprint)) {
    return;
  }
  OpenGuestStoreWatchLogLocked(false);
  if (!g_guest_store_watch_log) {
    return;
  }
  const uint32_t texture_base_address =
      g_guest_store_watch_texture_base_address.load(std::memory_order_relaxed);
  const bool has_watched_texture = DrawHasWatchedTexture(fingerprint, texture_base_address);
  const bool main_textured =
      has_watched_texture && fingerprint.bucket == DrawBucket::kMainColorDepth;
  g_guest_store_watch_log << NowMilliseconds() << ",draw_seen,0,";
  WriteHex(g_guest_store_watch_log, g_guest_store_watch_group_hash, 16);
  g_guest_store_watch_log << "," << DrawBucketName(fingerprint.bucket) << ","
                          << (main_textured ? "main_tex" : (has_watched_texture ? "tex" : "no_tex"))
                          << ",0,";
  WriteHex(g_guest_store_watch_log, fingerprint.packet_ptr, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, fingerprint.index_guest_base, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, fingerprint.vertex_fetch_address[0], 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, texture_base_address, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, uint32_t(fingerprint.vertex_shader_hash), 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, uint32_t(fingerprint.pixel_shader_hash), 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, uint32_t(fingerprint.primitive_type), 8);
  g_guest_store_watch_log << ",vs=";
  WriteHex(g_guest_store_watch_log, fingerprint.vertex_shader_hash, 16);
  g_guest_store_watch_log << " ps=";
  WriteHex(g_guest_store_watch_log, fingerprint.pixel_shader_hash, 16);
  g_guest_store_watch_log << " exact=";
  WriteHex(g_guest_store_watch_log, exact_hash, 16);
  g_guest_store_watch_log << " submitted=" << (submitted ? "1" : "0");
  g_guest_store_watch_log << ",v=" << fingerprint.vertex_count << " p="
                          << fingerprint.primitive_count << ",1\n";
  g_guest_store_watch_log.flush();
}

std::string CurrentGuestFunctionStack() {
  std::string stack;
  const uint32_t depth = std::min<uint32_t>(g_guest_function_stack_depth, 12);
  for (uint32_t i = 0; i < depth; ++i) {
    const uint32_t index = g_guest_function_stack_depth - depth + i;
    const char* function_name = g_guest_function_stack[index];
    if (!function_name) {
      continue;
    }
    if (!stack.empty()) {
      stack += ">";
    }
    stack += function_name;
  }
  return stack;
}

}  // namespace

// Live value of the Skate 3 presence context (0x8001): 1 = in gameplay,
// 0 = frontend / pause menu / loading. Consumers (the native scene renderer)
// yield to the emulated output while the game is in menus.
static std::atomic<uint32_t> g_skate3_gameplay_context_value{0};

void NotifySkate3GameplayContext(uint32_t user_index, uint32_t context_id, uint32_t value) {
  constexpr uint32_t kSkate3GameplayContext = 0x8001;
  constexpr uint32_t kSkate3GameplayValue = 1;
  if (user_index != 0 || context_id != kSkate3GameplayContext) {
    return;
  }
  g_skate3_gameplay_context_value.store(value, std::memory_order_relaxed);
  if (value != kSkate3GameplayValue) {
    return;
  }

  bool expected = false;
  if (g_skate3_gameplay_ultrawide_latched.compare_exchange_strong(expected, true)) {
    REXLOG_INFO("Skate 3 ultrawide: gameplay context reached; enabling ultrawide correction");
  }
}

bool IsSkate3GameplayUltrawideActive() {
  return g_skate3_gameplay_ultrawide_latched.load(std::memory_order_relaxed);
}

uint32_t Skate3GameplayContextValue() {
  return g_skate3_gameplay_context_value.load(std::memory_order_relaxed);
}

GuestFunctionScope::GuestFunctionScope(const char* function_name) {
  if (!IsGuestStoreWatchActive()) {
    return;
  }
  if (g_guest_function_stack_depth >= kMaxGuestFunctionStackDepth) {
    return;
  }
  g_guest_function_stack[g_guest_function_stack_depth++] = function_name;
  active_ = true;
}

GuestFunctionScope::~GuestFunctionScope() {
  if (active_ && g_guest_function_stack_depth != 0) {
    --g_guest_function_stack_depth;
  }
}

bool TargetKey::operator==(const TargetKey& other) const {
  return color_info == other.color_info && depth_info == other.depth_info &&
         surface_info == other.surface_info && viewport_x == other.viewport_x &&
         viewport_y == other.viewport_y && viewport_width == other.viewport_width &&
         viewport_height == other.viewport_height && color_mask == other.color_mask &&
         depth_control == other.depth_control && pa_cl_vte_cntl == other.pa_cl_vte_cntl &&
         primitive_type == other.primitive_type &&
         host_vertex_shader_type == other.host_vertex_shader_type &&
         vertex_shader_hash == other.vertex_shader_hash &&
         pixel_shader_hash == other.pixel_shader_hash;
}

uint32_t ShadowMapCandidateLevel(const TargetKey& key) {
  const bool depth_only = key.color_mask == 0 && key.pixel_shader_hash == 0;
  const bool square = key.viewport_width == key.viewport_height && key.viewport_width != 0;
  const bool atlas_aligned = square && (key.viewport_x == 0 || key.viewport_x % key.viewport_width == 0);

  // Skate 3 uses square 1024x1024 shadow atlas slices at X offsets that are
  // multiples of the slice size. These may have either depth-only or color
  // resolve/filtering passes, so don't require color_mask or pixel shader tests.
  if (atlas_aligned && key.viewport_width >= 512 && key.viewport_width <= 4096) {
    return 1;
  }

  // Clip-disabled offscreen passes often come through as very large square
  // viewports. Keep this separate because it's broader than the atlas rule.
  if (depth_only && square && key.viewport_width > 4096) {
    return 2;
  }

  // Broad diagnostic: any depth-only pass. This may include depth pre-passes,
  // so it should be used only if the narrower modes still leave shadow offsets.
  if (depth_only) {
    return 3;
  }

  return 0;
}

bool IsShadowMapCandidate(const TargetKey& key, uint32_t mode) {
  const uint32_t level = ShadowMapCandidateLevel(key);
  return level != 0 && level <= mode;
}

uint64_t HashKey(const TargetKey& key) {
  uint64_t hash = 1469598103934665603ull;
  for (uint32_t color_info : key.color_info) {
    HashAdd(hash, color_info);
  }
  HashAdd(hash, key.depth_info);
  HashAdd(hash, key.surface_info);
  HashAdd(hash, key.viewport_x);
  HashAdd(hash, key.viewport_y);
  HashAdd(hash, key.viewport_width);
  HashAdd(hash, key.viewport_height);
  HashAdd(hash, key.color_mask);
  HashAdd(hash, key.depth_control);
  HashAdd(hash, key.pa_cl_vte_cntl);
  HashAdd(hash, key.primitive_type);
  HashAdd(hash, key.host_vertex_shader_type);
  HashAdd(hash, key.vertex_shader_hash);
  HashAdd(hash, key.pixel_shader_hash);
  return hash;
}

bool ShouldApplyFast(const TargetKey& key, bool default_enabled, ApplyMode apply_mode) {
  if (apply_mode == ApplyMode::kForceAll) {
    return true;
  }
  if (apply_mode == ApplyMode::kForceNone) {
    return false;
  }

  struct TargetDecisionCacheEntry {
    TargetKey key;
    uint64_t hash = 0;
    uint32_t epoch = 0;
    ApplyMode apply_mode = ApplyMode::kTargetList;
    bool default_enabled = false;
    bool result = false;
    bool valid = false;
  };

  constexpr size_t kTargetDecisionCacheSize = 8192;
  thread_local std::array<TargetDecisionCacheEntry, kTargetDecisionCacheSize>
      target_decision_cache;

  const uint64_t key_hash = HashKey(key);
  const uint32_t epoch = g_target_decision_cache_epoch.load(std::memory_order_acquire);
  TargetDecisionCacheEntry& cached =
      target_decision_cache[size_t(key_hash) & (kTargetDecisionCacheSize - 1)];
  if (cached.valid && cached.epoch == epoch && cached.hash == key_hash &&
      cached.apply_mode == apply_mode && cached.default_enabled == default_enabled &&
      cached.key == key) {
    return cached.result;
  }

  bool result = default_enabled;
  const auto overrides = g_hash_overrides_snapshot.load(std::memory_order_acquire);
  if (overrides) {
    const auto it = overrides->find(key_hash);
    if (it != overrides->end()) {
      result = it->second;
      cached = TargetDecisionCacheEntry{key, key_hash, epoch, apply_mode, default_enabled, result,
                                        true};
      return result;
    }
  }
  if (const auto semantic_override = FindSemanticOverride(key)) {
    result = *semantic_override;
  } else if (const auto fallback_override = FindBuiltInFallbackOverride(key)) {
    result = *fallback_override;
  }
  cached =
      TargetDecisionCacheEntry{key, key_hash, epoch, apply_mode, default_enabled, result, true};
  return result;
}

bool ShouldApplyAndRecord(const TargetKey& key, bool default_enabled) {
  EnsureBuiltInSkate3ClassifierLoaded();
  const ApplyMode apply_mode = g_apply_mode.load(std::memory_order_relaxed);
  if (apply_mode == ApplyMode::kForceNone) {
    return false;
  }
  if (apply_mode == ApplyMode::kSavedOnly ||
      !g_target_recording_active.load(std::memory_order_relaxed)) {
    return ShouldApplyFast(key, default_enabled, apply_mode);
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  auto [it, inserted] = g_targets.try_emplace(key);
  TargetEntry& entry = it->second;
  if (inserted) {
    entry.hash = HashKey(key);
    entry.key = key;
    if (auto override_it = g_hash_overrides.find(entry.hash);
        override_it != g_hash_overrides.end()) {
      entry.enabled = override_it->second;
    } else if (const auto semantic_override = FindSemanticOverride(key)) {
      entry.enabled = *semantic_override;
    } else if (const auto fallback_override = FindBuiltInFallbackOverride(key)) {
      entry.enabled = *fallback_override;
    } else {
      entry.enabled = default_enabled;
    }
    entry.default_enabled = default_enabled;
    entry.shadow_candidate = ShadowMapCandidateLevel(key) != 0;
  } else {
    entry.default_enabled = default_enabled;
    entry.shadow_candidate = ShadowMapCandidateLevel(key) != 0;
  }
  ++entry.draw_count;
  entry.last_seen_order = ++g_seen_order;
  const bool should_apply =
      apply_mode == ApplyMode::kForceAll ||
      (apply_mode == ApplyMode::kTargetList && entry.enabled);
  if (should_apply) {
    ++entry.applied_draw_count;
  }
  return should_apply;
}

std::vector<TargetEntry> SnapshotTargets() {
  std::lock_guard<std::mutex> lock(g_mutex);
  std::vector<TargetEntry> targets;
  targets.reserve(g_targets.size());
  for (const auto& [key, entry] : g_targets) {
    targets.push_back(entry);
  }
  return targets;
}

void SetTargetEnabled(uint64_t hash, bool enabled) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_hash_overrides[hash] = enabled;
  PublishHashOverridesSnapshotLocked();
  for (auto& [key, entry] : g_targets) {
    if (entry.hash == hash) {
      entry.enabled = enabled;
    }
  }
}

void SetAllTargetsEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(g_mutex);
  for (auto& [key, entry] : g_targets) {
    entry.enabled = enabled;
    g_hash_overrides[entry.hash] = enabled;
  }
  PublishHashOverridesSnapshotLocked();
}

void ResetTargetOverrides() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_hash_overrides.clear();
  PublishHashOverridesSnapshotLocked();
  for (auto& [key, entry] : g_targets) {
    entry.enabled = entry.default_enabled;
  }
}

void SetTargetRecordingActive(bool active) {
  g_target_recording_active.store(active, std::memory_order_release);
}

uint64_t HashScreenCallback(ScreenCallbackKind kind, uint32_t caller_lr) {
  uint64_t hash = 1469598103934665603ull;
  HashAdd(hash, uint32_t(kind));
  HashAdd(hash, caller_lr);
  return hash;
}

bool RecordScreenCallback(ScreenCallbackKind kind, uint32_t caller_lr, uint32_t value) {
  if (!REXCVAR_GET(skate3_ultrawide_screen_callback_tracking)) {
    return false;
  }

  const uint64_t hash = HashScreenCallback(kind, caller_lr);
  std::lock_guard<std::mutex> lock(g_mutex);
  auto [it, inserted] = g_screen_callbacks.try_emplace(hash);
  ScreenCallbackEntry& entry = it->second;
  if (inserted) {
    entry.hash = hash;
    entry.kind = kind;
    entry.caller_lr = caller_lr;
    if (auto override_it = g_screen_callback_overrides.find(hash);
        override_it != g_screen_callback_overrides.end()) {
      entry.enabled = override_it->second;
    }
  }
  entry.last_value = value;
  entry.last_returned_value = value;
  entry.last_spoofed = false;
  ++entry.call_count;
  entry.last_seen_order = ++g_seen_order;
  return entry.enabled;
}

void RecordScreenCallbackResult(ScreenCallbackKind kind, uint32_t caller_lr,
                                uint32_t native_value, uint32_t returned_value) {
  if (!REXCVAR_GET(skate3_ultrawide_screen_callback_tracking)) {
    return;
  }

  const uint64_t hash = HashScreenCallback(kind, caller_lr);
  std::lock_guard<std::mutex> lock(g_mutex);
  auto [it, inserted] = g_screen_callbacks.try_emplace(hash);
  ScreenCallbackEntry& entry = it->second;
  if (inserted) {
    entry.hash = hash;
    entry.kind = kind;
    entry.caller_lr = caller_lr;
    if (auto override_it = g_screen_callback_overrides.find(hash);
        override_it != g_screen_callback_overrides.end()) {
      entry.enabled = override_it->second;
    }
  }
  entry.last_value = native_value;
  entry.last_returned_value = returned_value;
  entry.last_spoofed = native_value != returned_value;
}

std::vector<ScreenCallbackEntry> SnapshotScreenCallbacks() {
  if (!REXCVAR_GET(skate3_ultrawide_screen_callback_tracking)) {
    return {};
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  std::vector<ScreenCallbackEntry> entries;
  entries.reserve(g_screen_callbacks.size());
  for (const auto& [hash, entry] : g_screen_callbacks) {
    entries.push_back(entry);
  }
  return entries;
}

void SetScreenCallbackEnabled(uint64_t hash, bool enabled) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_screen_callback_overrides[hash] = enabled;
  if (auto it = g_screen_callbacks.find(hash); it != g_screen_callbacks.end()) {
    it->second.enabled = enabled;
  }
}

void SetAllScreenCallbacksEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(g_mutex);
  for (auto& [hash, entry] : g_screen_callbacks) {
    entry.enabled = enabled;
    g_screen_callback_overrides[hash] = enabled;
  }
}

void SetGuestStoreWatchTargets(uint32_t index_base, uint32_t vertex_fetch_address,
                               uint32_t texture_base_address, uint64_t group_hash) {
  g_guest_store_watch_index_base.store(index_base, std::memory_order_relaxed);
  g_guest_store_watch_vertex_fetch_address.store(vertex_fetch_address, std::memory_order_relaxed);
  g_guest_store_watch_texture_base_address.store(texture_base_address, std::memory_order_relaxed);
  g_guest_store_watch_enabled.store(index_base != 0 || vertex_fetch_address != 0 ||
                                        texture_base_address != 0,
                                    std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(g_mutex);
  g_guest_store_watch_group_hash = group_hash;
  g_guest_store_watch_events.clear();
  if (g_guest_store_watch_log.is_open()) {
    g_guest_store_watch_log.close();
  }
  OpenGuestStoreWatchLogLocked(true);
  WriteGuestStoreWatchTargetsLocked("watch_begin");
}

void ClearGuestStoreWatchTargets() {
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    WriteGuestStoreWatchTargetsLocked("watch_clear");
    g_guest_store_watch_events.clear();
    if (g_guest_store_watch_log.is_open()) {
      g_guest_store_watch_log.flush();
    }
  }
  g_guest_store_watch_enabled.store(false, std::memory_order_relaxed);
  g_guest_store_watch_index_base.store(0, std::memory_order_relaxed);
  g_guest_store_watch_vertex_fetch_address.store(0, std::memory_order_relaxed);
  g_guest_store_watch_texture_base_address.store(0, std::memory_order_relaxed);
}

void ClearGuestStoreWatchEvents() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_guest_store_watch_events.clear();
}

std::vector<GuestStoreWatchEntry> SnapshotGuestStoreWatchEvents() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return {g_guest_store_watch_events.begin(), g_guest_store_watch_events.end()};
}

void SetGuestStoreWatchLogPath(const std::filesystem::path& path) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_guest_store_watch_log_path = path;
  if (g_guest_store_watch_log.is_open()) {
    g_guest_store_watch_log.close();
  }
}

void RecordGuestStoreU32(uint32_t address, uint32_t value, const char* function_name,
                         uint32_t caller_lr) {
  if (!g_guest_store_watch_enabled.load(std::memory_order_relaxed)) {
    return;
  }

  const uint32_t index_base =
      g_guest_store_watch_index_base.load(std::memory_order_relaxed);
  const uint32_t vertex_fetch_address =
      g_guest_store_watch_vertex_fetch_address.load(std::memory_order_relaxed);
  const uint32_t texture_base_address =
      g_guest_store_watch_texture_base_address.load(std::memory_order_relaxed);

  const char* target = nullptr;
  if (index_base != 0 && value == index_base) {
    target = "IB";
  } else if (vertex_fetch_address != 0 && value == vertex_fetch_address) {
    target = "VF0";
  } else if (vertex_fetch_address != 0 && value == (vertex_fetch_address >> 2)) {
    target = "VF0>>2";
  } else if (texture_base_address != 0 && value == texture_base_address) {
    target = "Tex";
  } else if (texture_base_address != 0 && value == (texture_base_address >> 12)) {
    target = "Tex>>12";
  } else {
    return;
  }

  const std::string current_stack = CurrentGuestFunctionStack();
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_guest_store_watch_events.empty()) {
    GuestStoreWatchEntry& previous = g_guest_store_watch_events.back();
    if (previous.target == target && previous.address == address && previous.value == value &&
        previous.caller_lr == caller_lr &&
        previous.function == (function_name ? function_name : "") &&
        previous.stack == current_stack) {
      previous.sequence = ++g_guest_store_watch_sequence;
      ++previous.count;
      WriteGuestStoreWatchStoreLocked(previous);
      return;
    }
  }

  GuestStoreWatchEntry entry;
  entry.sequence = ++g_guest_store_watch_sequence;
  entry.target = target;
  entry.address = address;
  entry.value = value;
  entry.caller_lr = caller_lr;
  entry.function = function_name ? function_name : "";
  entry.stack = current_stack;
  g_guest_store_watch_events.push_back(std::move(entry));
  WriteGuestStoreWatchStoreLocked(g_guest_store_watch_events.back());
  while (g_guest_store_watch_events.size() > kMaxGuestStoreWatchEvents) {
    g_guest_store_watch_events.pop_front();
  }
}

void RecordDrawWatchTransition(bool present, uint64_t age, uint32_t packet_ptr,
                               uint32_t index_base, uint32_t vertex_fetch_address,
                               uint64_t group_hash) {
  std::lock_guard<std::mutex> lock(g_mutex);
  OpenGuestStoreWatchLogLocked(false);
  if (!g_guest_store_watch_log) {
    return;
  }
  const uint32_t texture_base_address =
      g_guest_store_watch_texture_base_address.load(std::memory_order_relaxed);
  g_guest_store_watch_log << NowMilliseconds() << ",draw_transition,0,";
  WriteHex(g_guest_store_watch_log, group_hash, 16);
  g_guest_store_watch_log << ",," << (present ? "present" : "missing") << ",";
  if (age == UINT64_MAX) {
    g_guest_store_watch_log << "n/a";
  } else {
    g_guest_store_watch_log << age;
  }
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, packet_ptr, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, index_base, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, vertex_fetch_address, 8);
  g_guest_store_watch_log << ",";
  WriteHex(g_guest_store_watch_log, texture_base_address, 8);
  g_guest_store_watch_log << ",,,,,0\n";
  g_guest_store_watch_log.flush();
}

bool IsGuestStoreWatchActive() {
  return g_guest_store_watch_enabled.load(std::memory_order_relaxed);
}

void SetCurrentDrawOwner(uint32_t object, uint32_t vtable, uint32_t target) {
  g_current_draw_owner_object = object;
  g_current_draw_owner_vtable = vtable;
  g_current_draw_owner_target = target;
}

void ClearCurrentDrawOwner() {
  g_current_draw_owner_object = 0;
  g_current_draw_owner_vtable = 0;
  g_current_draw_owner_target = 0;
}

uint64_t HashDrawFingerprint(const DrawFingerprint& fingerprint) {
  uint64_t hash = 1469598103934665603ull;
  HashAdd(hash, uint32_t(fingerprint.bucket));
  HashAdd(hash, fingerprint.guest_render_object);
  HashAdd(hash, fingerprint.guest_render_vtable);
  HashAdd(hash, fingerprint.guest_render_target);
  HashAdd(hash, fingerprint.vertex_shader_hash);
  HashAdd(hash, fingerprint.pixel_shader_hash);
  HashAdd(hash, fingerprint.primitive_type);
  HashAdd(hash, fingerprint.vertex_count);
  HashAdd(hash, fingerprint.primitive_count);
  HashAdd(hash, fingerprint.index_guest_base);
  HashAdd(hash, fingerprint.index_length);
  for (size_t i = 0; i < DrawFingerprint::kVertexFetchCount; ++i) {
    HashAdd(hash, fingerprint.vertex_fetch_address[i]);
    HashAdd(hash, fingerprint.vertex_fetch_size[i]);
  }
  for (size_t i = 0; i < DrawFingerprint::kTextureFetchCount; ++i) {
    HashAdd(hash, fingerprint.texture_key_hash[i]);
    HashAdd(hash, fingerprint.texture_fetch_index[i]);
  }
  return hash;
}

bool RecordDrawFingerprint(const DrawFingerprint& fingerprint) {
  DrawFingerprint fingerprint_with_owner = fingerprint;
  fingerprint_with_owner.guest_render_object = g_current_draw_owner_object;
  fingerprint_with_owner.guest_render_vtable = g_current_draw_owner_vtable;
  fingerprint_with_owner.guest_render_target = g_current_draw_owner_target;
  const uint64_t hash = HashDrawFingerprint(fingerprint_with_owner);
  std::lock_guard<std::mutex> lock(g_mutex);
  DrawFingerprintKey key{fingerprint_with_owner};
  auto it = g_draw_fingerprints.find(key);
  if (it == g_draw_fingerprints.end()) {
    if (g_draw_fingerprints.size() >= kMaxDrawFingerprints) {
      return true;
    }
    it = g_draw_fingerprints.try_emplace(key).first;
  }
  DrawFingerprintEntry& entry = it->second;
  if (!entry.draw_count) {
    entry.fingerprint = fingerprint_with_owner;
    entry.hash = hash;
  } else {
    entry.fingerprint.guest_render_object = fingerprint_with_owner.guest_render_object;
    entry.fingerprint.guest_render_vtable = fingerprint_with_owner.guest_render_vtable;
    entry.fingerprint.guest_render_target = fingerprint_with_owner.guest_render_target;
    entry.fingerprint.packet_ptr = fingerprint_with_owner.packet_ptr;
    entry.fingerprint.index_guest_base = fingerprint_with_owner.index_guest_base;
    entry.fingerprint.index_length = fingerprint_with_owner.index_length;
    std::copy(std::begin(fingerprint_with_owner.vertex_fetch_address),
              std::end(fingerprint_with_owner.vertex_fetch_address),
              std::begin(entry.fingerprint.vertex_fetch_address));
    std::copy(std::begin(fingerprint_with_owner.texture_key_hash),
              std::end(fingerprint_with_owner.texture_key_hash),
              std::begin(entry.fingerprint.texture_key_hash));
    std::copy(std::begin(fingerprint_with_owner.texture_fetch_index),
              std::end(fingerprint_with_owner.texture_fetch_index),
              std::begin(entry.fingerprint.texture_fetch_index));
    std::copy(std::begin(fingerprint_with_owner.texture_base_address),
              std::end(fingerprint_with_owner.texture_base_address),
              std::begin(entry.fingerprint.texture_base_address));
    std::copy(std::begin(fingerprint_with_owner.texture_base_length),
              std::end(fingerprint_with_owner.texture_base_length),
              std::begin(entry.fingerprint.texture_base_length));
    std::copy(std::begin(fingerprint_with_owner.texture_width),
              std::end(fingerprint_with_owner.texture_width),
              std::begin(entry.fingerprint.texture_width));
    std::copy(std::begin(fingerprint_with_owner.texture_height),
              std::end(fingerprint_with_owner.texture_height),
              std::begin(entry.fingerprint.texture_height));
    std::copy(std::begin(fingerprint_with_owner.texture_format),
              std::end(fingerprint_with_owner.texture_format),
              std::begin(entry.fingerprint.texture_format));
  }
  if (auto override_it = g_draw_fingerprint_overrides.find(hash);
      override_it != g_draw_fingerprint_overrides.end()) {
    entry.enabled = override_it->second;
  }
  ++entry.draw_count;
  if (entry.enabled) {
    ++entry.submitted_count;
    entry.vertices += fingerprint_with_owner.vertex_count;
    entry.primitives += fingerprint_with_owner.primitive_count;
  } else {
    ++entry.skipped_count;
  }
  entry.last_seen_order = ++g_seen_order;
  entry.last_seen_ms = NowMilliseconds();
  WriteGuestStoreWatchDrawLocked(fingerprint_with_owner, entry.enabled, hash);
  return entry.enabled;
}

std::vector<DrawFingerprintEntry> SnapshotDrawFingerprints() {
  std::lock_guard<std::mutex> lock(g_mutex);
  std::vector<DrawFingerprintEntry> entries;
  entries.reserve(g_draw_fingerprints.size());
  for (const auto& [_, entry] : g_draw_fingerprints) {
    entries.push_back(entry);
  }
  return entries;
}

void ClearDrawFingerprints() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_draw_fingerprints.clear();
}

void SetDrawFingerprintEnabled(uint64_t hash, bool enabled) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_draw_fingerprint_overrides[hash] = enabled;
  for (auto& [key, entry] : g_draw_fingerprints) {
    if (entry.hash == hash) {
      entry.enabled = enabled;
    }
  }
}

void SetAllDrawFingerprintsEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(g_mutex);
  for (auto& [key, entry] : g_draw_fingerprints) {
    entry.enabled = enabled;
    g_draw_fingerprint_overrides[entry.hash] = enabled;
  }
}

void ResetDrawFingerprintOverrides() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_draw_fingerprint_overrides.clear();
  for (auto& [key, entry] : g_draw_fingerprints) {
    entry.enabled = true;
  }
}

ApplyMode GetApplyMode() {
  return g_apply_mode.load(std::memory_order_relaxed);
}

void SetApplyMode(ApplyMode mode) {
  const ApplyMode previous = g_apply_mode.exchange(mode, std::memory_order_acq_rel);
  if (previous != mode) {
    InvalidateTargetDecisionCache();
  }
}

void LoadBuiltInSkate3Classifier() {
  std::lock_guard<std::mutex> lock(g_mutex);
  InstallBuiltInSkate3ClassifierLocked();
  for (auto& [key, entry] : g_targets) {
    if (const auto semantic_override = FindSemanticOverride(key)) {
      entry.enabled = *semantic_override;
    }
  }
}

std::optional<std::filesystem::path> LoadTargets(const std::filesystem::path& path) {
  if (path.empty() || !std::filesystem::exists(path)) {
    return std::nullopt;
  }

  toml::table config;
  try {
    config = toml::parse_file(path.string());
  } catch (const toml::parse_error&) {
    return std::nullopt;
  }

  std::unordered_map<uint64_t, bool> loaded_overrides;
  std::unordered_map<SemanticTargetKey, bool, SemanticTargetKeyHash> loaded_semantic_overrides;
  std::unordered_map<uint64_t, bool> loaded_screen_callback_overrides;
  if (auto* targets = config["target"].as_array()) {
    targets->for_each([&](toml::table& target) {
      const auto hash_string = target["hash"].value<std::string>();
      const auto enabled = target["enabled"].value<bool>();
      if (!hash_string || !enabled) {
        return;
      }
      const auto hash = ParseHexString(*hash_string);
      if (hash) {
        loaded_overrides[*hash] = *enabled;
      }
      if (auto semantic_key = ParseSemanticTargetKey(target)) {
        loaded_semantic_overrides[*semantic_key] = *enabled;
      }
    });
  }
  if (auto* callbacks = config["screen_callback"].as_array()) {
    callbacks->for_each([&](toml::table& callback) {
      const auto kind_string = callback["kind"].value<std::string>();
      const auto caller_lr_string = callback["caller_lr"].value<std::string>();
      const auto enabled = callback["enabled"].value<bool>();
      if (!kind_string || !caller_lr_string || !enabled) {
        return;
      }
      const auto kind = ParseScreenCallbackKind(*kind_string);
      const auto caller_lr = ParseHexString(*caller_lr_string);
      if (kind && caller_lr) {
        loaded_screen_callback_overrides[HashScreenCallback(*kind, uint32_t(*caller_lr))] =
            *enabled;
      }
    });
  }

  ApplyMode loaded_mode = ApplyMode::kTargetList;
  if (auto mode = config["apply_mode"].value<std::string>()) {
    if (*mode == "force_all") {
      loaded_mode = ApplyMode::kForceAll;
    } else if (*mode == "force_none") {
      loaded_mode = ApplyMode::kForceNone;
    } else if (*mode == "saved_only" || *mode == "hard_config") {
      loaded_mode = ApplyMode::kSavedOnly;
    }
  }

  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_hash_overrides = std::move(loaded_overrides);
    PublishHashOverridesSnapshotLocked();
    g_semantic_overrides = std::move(loaded_semantic_overrides);
    PublishSemanticOverridesSnapshotLocked();
    g_builtin_target_overrides_active.store(false, std::memory_order_release);
    g_screen_callback_overrides = std::move(loaded_screen_callback_overrides);
    g_target_overrides_loaded.store(true, std::memory_order_release);
    for (auto& [key, entry] : g_targets) {
      if (auto override_it = g_hash_overrides.find(entry.hash);
          override_it != g_hash_overrides.end()) {
        entry.enabled = override_it->second;
      } else if (const auto semantic_override = FindSemanticOverride(key)) {
        entry.enabled = *semantic_override;
      }
    }
    for (auto& [hash, entry] : g_screen_callbacks) {
      if (auto override_it = g_screen_callback_overrides.find(hash);
          override_it != g_screen_callback_overrides.end()) {
        entry.enabled = override_it->second;
      }
    }
  }
  SetApplyMode(loaded_mode);
  return path;
}

std::optional<std::filesystem::path> SaveTargets(const std::filesystem::path& path) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return std::nullopt;
  }

  auto targets = SnapshotTargets();
  std::sort(targets.begin(), targets.end(), [](const TargetEntry& a, const TargetEntry& b) {
    if (a.enabled != b.enabled) {
      return a.enabled > b.enabled;
    }
    if (a.last_seen_order != b.last_seen_order) {
      return a.last_seen_order > b.last_seen_order;
    }
    return a.hash < b.hash;
  });

  stream << "# Skate 3 ultrawide NDC target selection export\n";
  stream << "# enabled targets are the draw groups currently receiving Hor+ NDC correction\n\n";
  const ApplyMode apply_mode = GetApplyMode();
  stream << "apply_mode = \""
         << (apply_mode == ApplyMode::kForceAll
                 ? "force_all"
                 : (apply_mode == ApplyMode::kForceNone
                        ? "force_none"
                        : (apply_mode == ApplyMode::kSavedOnly ? "saved_only" : "target_list")))
         << "\"\n\n";
  for (const TargetEntry& entry : targets) {
    stream << "[[target]]\n";
    stream << "enabled = " << (entry.enabled ? "true" : "false") << "\n";
    stream << "default_enabled = " << (entry.default_enabled ? "true" : "false") << "\n";
    stream << "shadow_candidate = " << (entry.shadow_candidate ? "true" : "false") << "\n";
    stream << "draw_count = " << entry.draw_count << "\n";
    stream << "applied_draw_count = " << entry.applied_draw_count << "\n";
    stream << "hash = \"";
    WriteHex(stream, entry.hash, 16);
    stream << "\"\n";
    stream << "color_info = [";
    for (size_t i = 0; i < entry.key.color_info.size(); ++i) {
      if (i) {
        stream << ", ";
      }
      stream << "\"";
      WriteHex(stream, entry.key.color_info[i], 8);
      stream << "\"";
    }
    stream << "]\n";
    stream << "depth_info = \"";
    WriteHex(stream, entry.key.depth_info, 8);
    stream << "\"\n";
    stream << "surface_info = \"";
    WriteHex(stream, entry.key.surface_info, 8);
    stream << "\"\n";
    stream << "viewport = [" << entry.key.viewport_x << ", " << entry.key.viewport_y << ", "
           << entry.key.viewport_width << ", " << entry.key.viewport_height << "]\n";
    stream << "color_mask = \"";
    WriteHex(stream, entry.key.color_mask, 8);
    stream << "\"\n";
    stream << "depth_control = \"";
    WriteHex(stream, entry.key.depth_control, 8);
    stream << "\"\n";
    stream << "pa_cl_vte_cntl = \"";
    WriteHex(stream, entry.key.pa_cl_vte_cntl, 8);
    stream << "\"\n";
    stream << "primitive_type = " << entry.key.primitive_type << "\n";
    stream << "host_vertex_shader_type = " << entry.key.host_vertex_shader_type << "\n";
    stream << "vertex_shader_hash = \"";
    WriteHex(stream, entry.key.vertex_shader_hash, 16);
    stream << "\"\n";
    stream << "pixel_shader_hash = \"";
    WriteHex(stream, entry.key.pixel_shader_hash, 16);
    stream << "\"\n\n";
  }

  auto callbacks = SnapshotScreenCallbacks();
  std::sort(callbacks.begin(), callbacks.end(),
            [](const ScreenCallbackEntry& a, const ScreenCallbackEntry& b) {
              if (a.kind != b.kind) {
                return uint32_t(a.kind) < uint32_t(b.kind);
              }
              return a.caller_lr < b.caller_lr;
            });

  for (const ScreenCallbackEntry& entry : callbacks) {
    stream << "[[screen_callback]]\n";
    stream << "enabled = " << (entry.enabled ? "true" : "false") << "\n";
    stream << "kind = \"" << ScreenCallbackKindName(entry.kind) << "\"\n";
    stream << "caller_lr = \"";
    WriteHex(stream, entry.caller_lr, 8);
    stream << "\"\n";
    stream << "last_value = " << entry.last_value << "\n";
    stream << "call_count = " << entry.call_count << "\n";
    stream << "hash = \"";
    WriteHex(stream, entry.hash, 16);
    stream << "\"\n\n";
  }

  return path;
}

}  // namespace rex::graphics::ultrawide_debug
