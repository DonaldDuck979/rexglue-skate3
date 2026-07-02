#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rex::graphics::ultrawide_debug {

struct TargetKey {
  std::array<uint32_t, 4> color_info{};
  uint32_t depth_info = 0;
  uint32_t surface_info = 0;
  uint32_t viewport_x = 0;
  uint32_t viewport_y = 0;
  uint32_t viewport_width = 0;
  uint32_t viewport_height = 0;
  uint32_t color_mask = 0;
  uint32_t depth_control = 0;
  uint32_t pa_cl_vte_cntl = 0;
  uint32_t primitive_type = 0;
  uint32_t host_vertex_shader_type = 0;
  uint64_t vertex_shader_hash = 0;
  uint64_t pixel_shader_hash = 0;

  bool operator==(const TargetKey& other) const;
};

struct TargetEntry {
  uint64_t hash = 0;
  TargetKey key;
  bool enabled = false;
  bool default_enabled = false;
  bool shadow_candidate = false;
  uint64_t draw_count = 0;
  uint64_t applied_draw_count = 0;
  uint64_t last_seen_order = 0;
};

enum class ScreenCallbackKind : uint32_t {
  kWidth = 0,
  kHeight = 1,
  kScreenFlag = 2,
  kWide = 3,
};

struct ScreenCallbackEntry {
  uint64_t hash = 0;
  ScreenCallbackKind kind = ScreenCallbackKind::kWidth;
  uint32_t caller_lr = 0;
  uint32_t last_value = 0;
  uint32_t last_returned_value = 0;
  bool last_spoofed = false;
  bool enabled = false;
  uint64_t call_count = 0;
  uint64_t last_seen_order = 0;
};

struct GuestStoreWatchEntry {
  uint64_t sequence = 0;
  std::string target;
  uint32_t address = 0;
  uint32_t value = 0;
  uint32_t caller_lr = 0;
  std::string function;
  std::string stack;
  uint64_t count = 1;
};

class GuestFunctionScope {
 public:
  explicit GuestFunctionScope(const char* function_name);
  ~GuestFunctionScope();

 private:
  bool active_ = false;
};

enum class DrawBucket : uint32_t {
  kMainColorDepth = 0,
  kDepthOnly = 1,
  kCopyResolve = 2,
  kMemexport = 3,
  kNoPixelShader = 4,
};

struct DrawFingerprint {
  DrawBucket bucket = DrawBucket::kMainColorDepth;
  uint32_t guest_render_object = 0;
  uint32_t guest_render_vtable = 0;
  uint32_t guest_render_target = 0;
  uint64_t vertex_shader_hash = 0;
  uint64_t pixel_shader_hash = 0;
  uint32_t primitive_type = 0;
  uint32_t vertex_count = 0;
  uint32_t primitive_count = 0;
  uint32_t packet_ptr = 0;
  uint32_t index_guest_base = 0;
  uint32_t index_length = 0;
  static constexpr size_t kVertexFetchCount = 4;
  uint32_t vertex_fetch_address[kVertexFetchCount] = {};
  uint32_t vertex_fetch_size[kVertexFetchCount] = {};
  static constexpr size_t kTextureFetchCount = 8;
  uint64_t texture_key_hash[kTextureFetchCount] = {};
  uint32_t texture_fetch_index[kTextureFetchCount] = {};
  uint32_t texture_base_address[kTextureFetchCount] = {};
  uint32_t texture_base_length[kTextureFetchCount] = {};
  uint32_t texture_width[kTextureFetchCount] = {};
  uint32_t texture_height[kTextureFetchCount] = {};
  uint32_t texture_format[kTextureFetchCount] = {};
};

struct DrawFingerprintEntry {
  DrawFingerprint fingerprint;
  uint64_t hash = 0;
  bool enabled = true;
  uint64_t draw_count = 0;
  uint64_t submitted_count = 0;
  uint64_t skipped_count = 0;
  uint64_t vertices = 0;
  uint64_t primitives = 0;
  uint64_t last_seen_order = 0;
  uint64_t last_seen_ms = 0;
};

enum class ApplyMode : uint32_t {
  kTargetList = 0,
  kForceAll = 1,
  kForceNone = 2,
  kSavedOnly = 3,
};

uint64_t HashKey(const TargetKey& key);
void NotifySkate3GameplayContext(uint32_t user_index, uint32_t context_id, uint32_t value);
bool IsSkate3GameplayUltrawideActive();
// Live value of the Skate 3 presence context 0x8001 (1 = in gameplay,
// 0 = frontend / pause menu / loading).
uint32_t Skate3GameplayContextValue();
bool ShouldApplyAndRecord(const TargetKey& key, bool default_enabled);
uint32_t ShadowMapCandidateLevel(const TargetKey& key);
bool IsShadowMapCandidate(const TargetKey& key, uint32_t mode = 1);
std::vector<TargetEntry> SnapshotTargets();
void SetTargetEnabled(uint64_t hash, bool enabled);
void SetAllTargetsEnabled(bool enabled);
void ResetTargetOverrides();
void SetTargetRecordingActive(bool active);
uint64_t HashScreenCallback(ScreenCallbackKind kind, uint32_t caller_lr);
bool RecordScreenCallback(ScreenCallbackKind kind, uint32_t caller_lr, uint32_t value);
void RecordScreenCallbackResult(ScreenCallbackKind kind, uint32_t caller_lr, uint32_t native_value,
                                uint32_t returned_value);
std::vector<ScreenCallbackEntry> SnapshotScreenCallbacks();
void SetScreenCallbackEnabled(uint64_t hash, bool enabled);
void SetAllScreenCallbacksEnabled(bool enabled);
void SetGuestStoreWatchTargets(uint32_t index_base, uint32_t vertex_fetch_address,
                               uint32_t texture_base_address, uint64_t group_hash = 0);
void ClearGuestStoreWatchTargets();
void ClearGuestStoreWatchEvents();
void SetGuestStoreWatchLogPath(const std::filesystem::path& path);
std::vector<GuestStoreWatchEntry> SnapshotGuestStoreWatchEvents();
void RecordGuestStoreU32(uint32_t address, uint32_t value, const char* function_name,
                         uint32_t caller_lr = 0);
void RecordDrawWatchTransition(bool present, uint64_t age, uint32_t packet_ptr,
                               uint32_t index_base, uint32_t vertex_fetch_address,
                               uint64_t group_hash);
bool IsGuestStoreWatchActive();
void SetCurrentDrawOwner(uint32_t object, uint32_t vtable, uint32_t target);
void ClearCurrentDrawOwner();
uint64_t HashDrawFingerprint(const DrawFingerprint& fingerprint);
bool RecordDrawFingerprint(const DrawFingerprint& fingerprint);
std::vector<DrawFingerprintEntry> SnapshotDrawFingerprints();
void SetDrawFingerprintEnabled(uint64_t hash, bool enabled);
void SetAllDrawFingerprintsEnabled(bool enabled);
void ResetDrawFingerprintOverrides();
void ClearDrawFingerprints();
ApplyMode GetApplyMode();
void SetApplyMode(ApplyMode mode);
void LoadBuiltInSkate3Classifier();
std::optional<std::filesystem::path> LoadTargets(const std::filesystem::path& path);
std::optional<std::filesystem::path> SaveTargets(const std::filesystem::path& path);

}  // namespace rex::graphics::ultrawide_debug
