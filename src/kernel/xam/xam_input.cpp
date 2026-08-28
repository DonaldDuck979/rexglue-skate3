/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <rex/input/input.h>
#include <rex/input/input_system.h>
#include <rex/kernel/xam/input_injection.h>
#include <rex/kernel/xam/private.h>
#include <rex/logging.h>
#include <rex/hook.h>
#include <rex/types.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xtypes.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <iterator>
#include <mutex>

#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace rex {
namespace kernel {
namespace xam {
using namespace rex::system;

using rex::input::X_INPUT_CAPABILITIES;
using rex::input::X_INPUT_KEYSTROKE;
using rex::input::X_INPUT_STATE;
using rex::input::X_INPUT_VIBRATION;

constexpr uint32_t XINPUT_FLAG_GAMEPAD = 0x01;
constexpr uint32_t XINPUT_FLAG_ANY_USER = 1 << 30;

namespace {

constexpr size_t kMaxSyntheticInputSteps = 16;

std::atomic<bool> g_synthetic_input_active{false};
std::mutex g_synthetic_input_mutex;
std::array<SyntheticInputStep, kMaxSyntheticInputSteps> g_synthetic_input_steps{};
size_t g_synthetic_input_step_count = 0;
size_t g_synthetic_input_step_index = 0;
uint32_t g_synthetic_input_step_remaining = 0;
uint16_t g_synthetic_auto_tap_buttons = 0;
bool g_synthetic_auto_tap_enabled = false;
std::atomic<bool> g_input_suppressed{false};  // freeze: neutral gamepad to guest.
std::atomic<bool> g_input_log{false};         // log the state the guest reads.

void QueueSyntheticInputSequenceLocked(const SyntheticInputStep* steps, size_t step_count) {
  g_synthetic_input_step_count = std::min(step_count, kMaxSyntheticInputSteps);
  g_synthetic_input_step_index = 0;
  for (size_t i = 0; i < g_synthetic_input_step_count; ++i) {
    g_synthetic_input_steps[i] = steps[i];
  }
  while (g_synthetic_input_step_count != 0 &&
         g_synthetic_input_steps[g_synthetic_input_step_count - 1].poll_count == 0) {
    --g_synthetic_input_step_count;
  }
  g_synthetic_input_step_remaining =
      g_synthetic_input_step_count ? g_synthetic_input_steps[0].poll_count : 0;
  g_synthetic_input_active.store(g_synthetic_input_step_count != 0 &&
                                     g_synthetic_input_step_remaining != 0,
                                 std::memory_order_relaxed);
}

void EnsureSyntheticAutoTapLocked() {
  if (!g_synthetic_auto_tap_enabled || g_synthetic_auto_tap_buttons == 0 ||
      g_synthetic_input_active.load(std::memory_order_relaxed)) {
    return;
  }

  const SyntheticInputStep auto_tap[] = {
      {g_synthetic_auto_tap_buttons, 5},
      {0, 15},
  };
  QueueSyntheticInputSequenceLocked(auto_tap, std::size(auto_tap));
}

void ApplySyntheticInput(X_INPUT_STATE* input_state) {
  if (!input_state) {
    return;
  }

  std::lock_guard lock(g_synthetic_input_mutex);
  EnsureSyntheticAutoTapLocked();
  if (!g_synthetic_input_active.load(std::memory_order_relaxed) ||
      g_synthetic_input_step_index >= g_synthetic_input_step_count) {
    g_synthetic_input_active.store(false, std::memory_order_relaxed);
    return;
  }

  const auto& step = g_synthetic_input_steps[g_synthetic_input_step_index];
  if (step.buttons != 0 || step.left_trigger != 0 || step.right_trigger != 0) {
    input_state->gamepad.buttons =
        static_cast<uint16_t>(static_cast<uint16_t>(input_state->gamepad.buttons) | step.buttons);
    input_state->gamepad.left_trigger =
        std::max(input_state->gamepad.left_trigger, step.left_trigger);
    input_state->gamepad.right_trigger =
        std::max(input_state->gamepad.right_trigger, step.right_trigger);
    input_state->packet_number =
        static_cast<uint32_t>(static_cast<uint32_t>(input_state->packet_number) + 1);
  }

  if (g_synthetic_input_step_remaining > 0) {
    --g_synthetic_input_step_remaining;
  }

  while (g_synthetic_input_step_remaining == 0) {
    ++g_synthetic_input_step_index;
    if (g_synthetic_input_step_index >= g_synthetic_input_step_count) {
      g_synthetic_input_active.store(false, std::memory_order_relaxed);
      return;
    }
    g_synthetic_input_step_remaining =
        g_synthetic_input_steps[g_synthetic_input_step_index].poll_count;
  }
}

bool PopSyntheticKeystroke(X_INPUT_KEYSTROKE* out_keystroke) {
  (void)out_keystroke;
  return false;
}

}  // namespace

void QueueSyntheticInput(uint16_t buttons, uint32_t poll_count) {
  SyntheticInputStep step{buttons, poll_count};
  QueueSyntheticInputSequence(&step, 1);
}

void QueueSyntheticInput(uint16_t buttons, uint8_t left_trigger, uint8_t right_trigger,
                         uint32_t poll_count) {
  SyntheticInputStep step{buttons, poll_count, left_trigger, right_trigger};
  QueueSyntheticInputSequence(&step, 1);
}

void QueueSyntheticInputSequence(const SyntheticInputStep* steps, size_t step_count) {
  if (!steps || step_count == 0) {
    return;
  }

  std::lock_guard lock(g_synthetic_input_mutex);
  QueueSyntheticInputSequenceLocked(steps, step_count);
}

void SetSyntheticAutoTap(uint16_t buttons, bool enabled) {
  std::lock_guard lock(g_synthetic_input_mutex);
  g_synthetic_auto_tap_buttons = buttons;
  g_synthetic_auto_tap_enabled = enabled && buttons != 0;
}

void ClearSyntheticInput() {
  std::lock_guard lock(g_synthetic_input_mutex);
  g_synthetic_input_step_count = 0;
  g_synthetic_input_step_index = 0;
  g_synthetic_input_step_remaining = 0;
  g_synthetic_auto_tap_buttons = 0;
  g_synthetic_auto_tap_enabled = false;
  g_synthetic_input_active.store(false, std::memory_order_relaxed);
}

void SetInputSuppressed(bool suppressed) {
  g_input_suppressed.store(suppressed, std::memory_order_relaxed);
}

bool IsInputSuppressed() {
  return g_input_suppressed.load(std::memory_order_relaxed);
}

void SetInputLogging(bool enabled) {
  g_input_log.store(enabled, std::memory_order_relaxed);
}

void SetTrickStance(int stance);  // defined below (uses g_trick_stance)

// [skate3-online] S.K.A.T.E. flick-it trick detector. Classifies a right-stick
// gesture into a trick by its PEAK deflection (the flick), which encodes both
// the family (flick UP = regular pop, flick DOWN = nollie) and the flip (peak
// to the RIGHT = kickflip, LEFT = heelflip, for GOOFY; regular stance mirrors
// L/R). Confirmed against James (competitive Skate 3): goofy kickflip = Down->
// Up-Right, heelflip = Up-Left, nollie kickflip = Up->Down-Right, etc. A flick
// is a right-stick excursion that crosses a HIGH magnitude then returns to
// neutral; we track the max-magnitude sample as the peak. Spins/shuvits are a
// later layer. g_trick_stance: 0=goofy, 1=regular.
std::atomic<int> g_trick_stance{0};

// Snapshot of the last completed right-stick flick, published to the game
// thread for auto-stance correlation against the game's HUD trick name.
// Atomics only (uint64_t seq + primitives) so the reader never sees a torn
// value; the writer only publishes AFTER the whole gesture has settled.
std::atomic<uint64_t> g_last_flick_seq{0};
std::atomic<int32_t> g_last_flick_rx{0};
std::atomic<int32_t> g_last_flick_ry{0};
std::atomic<uint8_t> g_last_flick_pull_down{0};
std::atomic<uint8_t> g_last_flick_have{0};

namespace {
// One flick-it gesture = a right-stick excursion that leaves neutral, winds up
// (the PULL: down for a pop trick, up for a nollie), whips to the opposite
// extreme (the FLICK), and returns to neutral. The PULL direction picks the
// family; the FLICK's horizontal lean picks the flip. Treating the whole thing
// as one gesture stops the wind-up from firing a phantom trick.
struct Gesture {
  bool active = false;
  bool reached_high = false;
  int start_ry = 0;          // pull-direction reference
  int frames = 0;
  int neutral = 0;           // consecutive near-center frames
  int32_t flick_mag2 = 0;    // strongest excursion OPPOSITE the pull
  int flick_rx = 0, flick_ry = 0;
  bool have_flick = false;
};
Gesture g_g;

const char* ClassifyTrick(bool pull_down, int flick_rx, int stance) {
  int prx = (stance == 1) ? -flick_rx : flick_rx;  // regular mirrors L/R
  const int kSide = 12000;  // horizontal lean to count as a flip vs straight
  if (pull_down) {          // pulled DOWN then flicked UP = regular-footed pop
    if (prx > kSide) return "Kickflip";
    if (prx < -kSide) return "Heelflip";
    return "Ollie";
  }
  if (prx > kSide) return "Nollie Kickflip";  // pulled UP then flicked DOWN
  if (prx < -kSide) return "Nollie Heelflip";
  return "Nollie";
}

// Fed the final right-stick value every input poll. Logs "[trick] <name>" once
// per completed gesture. Runs only while input logging is on (test/bring-up).
void ProcessTrickInput(int16_t rx, int16_t ry) {
  const int32_t mag2 = int32_t(rx) * rx + int32_t(ry) * ry;
  const int32_t kLow2 = 12000 * 12000;   // leave neutral => gesture starts
  const int32_t kHigh2 = 24000 * 24000;  // must be reached to be a real flick
  const int32_t kNeut2 = 7000 * 7000;    // near-center => gesture may end
  if (!g_g.active) {
    if (mag2 > kLow2) {
      g_g = Gesture{};
      g_g.active = true;
      g_g.start_ry = ry;
      g_g.reached_high = (mag2 > kHigh2);
    }
    return;
  }
  ++g_g.frames;
  if (mag2 > kHigh2) g_g.reached_high = true;
  // Lock the pull direction from the first strongly-vertical sample.
  if (g_g.frames < 8 && (ry > 15000 || ry < -15000) &&
      g_g.start_ry < 15000 && g_g.start_ry > -15000) {
    g_g.start_ry = ry;
  }
  const bool pull_down = g_g.start_ry < 0;
  const bool opp = pull_down ? (ry > 8000) : (ry < -8000);  // flick = opposite the pull
  if (opp && mag2 > g_g.flick_mag2) {
    g_g.flick_mag2 = mag2;
    g_g.flick_rx = rx;
    g_g.flick_ry = ry;
    g_g.have_flick = true;
  }
  g_g.neutral = (mag2 < kNeut2) ? g_g.neutral + 1 : 0;
  if (g_g.neutral >= 4 || g_g.frames > 150) {  // settled back to neutral (or timeout)
    if (g_g.reached_high && g_g.have_flick) {
      const char* name = ClassifyTrick(pull_down, g_g.flick_rx,
                                       g_trick_stance.load(std::memory_order_relaxed));
      REXKRNL_INFO("[trick] {} (pull={} flick rx={} ry={})", name,
                   pull_down ? "D" : "U", g_g.flick_rx, g_g.flick_ry);
      // Publish for the auto-stance correlator. Write payload first, then bump
      // the seq counter so a reader that sees the new seq gets consistent data.
      g_last_flick_rx.store(g_g.flick_rx, std::memory_order_relaxed);
      g_last_flick_ry.store(g_g.flick_ry, std::memory_order_relaxed);
      g_last_flick_pull_down.store(pull_down ? 1 : 0, std::memory_order_relaxed);
      g_last_flick_have.store(1, std::memory_order_relaxed);
      g_last_flick_seq.fetch_add(1, std::memory_order_release);
    }
    g_g.active = false;
  }
}
}  // namespace

void SetTrickStance(int stance) {
  g_trick_stance.store(stance, std::memory_order_relaxed);
}

LastFlickSnapshot GetLastFlick() {
  // Read the seq LAST with acquire ordering so the payload fields we read
  // first pair with a matching writer publish.
  LastFlickSnapshot s;
  // Snapshot payload, then confirm seq to detect a torn read.
  for (int attempt = 0; attempt < 3; ++attempt) {
    const uint64_t s0 = g_last_flick_seq.load(std::memory_order_acquire);
    s.flick_rx = g_last_flick_rx.load(std::memory_order_relaxed);
    s.flick_ry = g_last_flick_ry.load(std::memory_order_relaxed);
    s.pull_down = g_last_flick_pull_down.load(std::memory_order_relaxed) != 0;
    s.have_flick = g_last_flick_have.load(std::memory_order_relaxed) != 0;
    const uint64_t s1 = g_last_flick_seq.load(std::memory_order_acquire);
    if (s0 == s1) { s.gesture_seq = s1; return s; }
  }
  s.gesture_seq = g_last_flick_seq.load(std::memory_order_relaxed);
  return s;
}

rex::input::InputSystem* input_system() {
  return static_cast<rex::input::InputSystem*>(REX_KERNEL_STATE()->emulator()->input_system());
}

void XamResetInactivity_entry() {
  // Do we need to do anything?
}

u32 XamEnableInactivityProcessing_entry(u32 unk, u32 enable) {
  return X_ERROR_SUCCESS;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetcapabilities(v=vs.85).aspx
u32 XamInputGetCapabilities_entry(u32 user_index, u32 flags, ppc_ptr_t<X_INPUT_CAPABILITIES> caps) {
  REXKRNL_TRACE("[XAM] XamInputGetCapabilities called: user={}, flags=0x{:X}", (uint32_t)user_index,
                (uint32_t)flags);
  if (!caps) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  auto* is = input_system();
  return is->GetCapabilities(actual_user_index, flags, caps);
}

u32 XamInputGetCapabilitiesEx_entry(u32 unk, u32 user_index, u32 flags,
                                    ppc_ptr_t<X_INPUT_CAPABILITIES> caps) {
  if (!caps) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  (void)unk;  // Unused in this implementation
  auto* is = input_system();
  return is->GetCapabilities(actual_user_index, flags, caps);
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetstate(v=vs.85).aspx
u32 XamInputGetState_entry(u32 user_index, u32 flags, ppc_ptr_t<X_INPUT_STATE> input_state) {
  // Games call this with a NULL state ptr, probably as a query.
  static int call_count = 0;
  if (++call_count <= 5) {
    REXKRNL_TRACE("[XAM] XamInputGetState called: user={}, flags=0x{:X}", (uint32_t)user_index,
                  (uint32_t)flags);
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  auto* is = input_system();
  const u32 result = is->GetState(actual_user_index, input_state);
  if (result == X_ERROR_SUCCESS) {
    // Freeze: zero the real input so the guest player can't move/trick. Done
    // BEFORE synthetic injection so a scripted reset can still be injected while
    // frozen (online game modes: freeze waiting players / respawn on start).
    if (g_input_suppressed.load(std::memory_order_relaxed) && input_state) {
      input_state->gamepad.buttons = 0;
      input_state->gamepad.left_trigger = 0;
      input_state->gamepad.right_trigger = 0;
      input_state->gamepad.thumb_lx = 0;
      input_state->gamepad.thumb_ly = 0;
      input_state->gamepad.thumb_rx = 0;
      input_state->gamepad.thumb_ry = 0;
    }
    ApplySyntheticInput(input_state);
    // Diagnostic: log the FINAL state the guest reads (rate-limited) so the
    // input path can be verified from the log (real input, freeze, injection).
    if (g_input_log.load(std::memory_order_relaxed) && input_state) {
      static uint32_t s_n = 0;
      // Log every poll with buttons pressed (so a brief injected combo always
      // shows) plus a periodic sample of the neutral state. ALSO log every poll
      // the RIGHT stick is deflected ([skate3-online] flick-it trick capture):
      // Skate 3 tricks are right-stick gestures, and a flick lasts only ~100-
      // 200 ms, so the 30-poll sample is far too sparse -- dense-log the whole
      // flick path so the S.K.A.T.E. trick detector can be designed from it.
      const bool any_button = (uint16_t)input_state->gamepad.buttons != 0;
      const int16_t rx_now = (int16_t)input_state->gamepad.thumb_rx;
      const int16_t ry_now = (int16_t)input_state->gamepad.thumb_ry;
      const bool rstick_active =
          rx_now > 10000 || rx_now < -10000 || ry_now > 10000 || ry_now < -10000;
      ProcessTrickInput(rx_now, ry_now);  // S.K.A.T.E. trick detector
      if (any_button || rstick_active || (++s_n % 30) == 0) {
        REXKRNL_INFO(
            "[input-read] user={} buttons=0x{:04X} lt={} rt={} "
            "lx={} ly={} rx={} ry={} frozen={}",
            (uint32_t)actual_user_index, (uint16_t)input_state->gamepad.buttons,
            (uint32_t)input_state->gamepad.left_trigger,
            (uint32_t)input_state->gamepad.right_trigger,
            (int32_t)(int16_t)input_state->gamepad.thumb_lx,
            (int32_t)(int16_t)input_state->gamepad.thumb_ly,
            (int32_t)(int16_t)input_state->gamepad.thumb_rx,
            (int32_t)(int16_t)input_state->gamepad.thumb_ry,
            g_input_suppressed.load(std::memory_order_relaxed) ? 1 : 0);
      }
    }
  }
  return result;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputsetstate(v=vs.85).aspx
u32 XamInputSetState_entry(u32 user_index, u32 unk, ppc_ptr_t<X_INPUT_VIBRATION> vibration) {
  if (!vibration) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  uint32_t actual_user_index = user_index;
  if ((user_index & 0xFF) == 0xFF) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  (void)unk;  // Unused in this implementation
  auto* is = input_system();
  return is->SetState(actual_user_index, vibration);
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetkeystroke(v=vs.85).aspx
u32 XamInputGetKeystroke_entry(u32 user_index, u32 flags, ppc_ptr_t<X_INPUT_KEYSTROKE> keystroke) {
  // https://github.com/CodeAsm/ffplay360/blob/master/Common/AtgXime.cpp
  // user index = index or XUSER_INDEX_ANY
  // flags = XINPUT_FLAG_GAMEPAD (| _ANYUSER | _ANYDEVICE)

  if (!keystroke) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  if (actual_user_index == 0 && PopSyntheticKeystroke(keystroke)) {
    return X_ERROR_SUCCESS;
  }

  auto* is = input_system();
  return is->GetKeystroke(actual_user_index, flags, keystroke);
}

// Same as non-ex, just takes a pointer to user index.
u32 XamInputGetKeystrokeEx_entry(mapped_u32 user_index_ptr, u32 flags,
                                 ppc_ptr_t<X_INPUT_KEYSTROKE> keystroke) {
  if (!keystroke) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t user_index = *user_index_ptr;
  if ((user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    user_index = 0;
  }

  auto* is = input_system();
  auto result = is->GetKeystroke(user_index, flags, keystroke);
  if (XSUCCEEDED(result)) {
    *user_index_ptr = keystroke->user_index;
  }
  return result;
}

i32 XamUserGetDeviceContext_entry(u32 user_index, u32 unk, mapped_u32 out_ptr) {
  // Games check the result - usually with some masking.
  // If this function fails they assume zero, so let's fail AND
  // set zero just to be safe.
  *out_ptr = 0;
  if (!user_index || (user_index & 0xFF) == 0xFF) {
    return X_E_SUCCESS;
  } else {
    return X_E_DEVICE_NOT_CONNECTED;
  }
}

}  // namespace xam
}  // namespace kernel
}  // namespace rex

REX_EXPORT(__imp__XamResetInactivity, rex::kernel::xam::XamResetInactivity_entry)
REX_EXPORT(__imp__XamEnableInactivityProcessing,
           rex::kernel::xam::XamEnableInactivityProcessing_entry)
REX_EXPORT(__imp__XamInputGetCapabilities, rex::kernel::xam::XamInputGetCapabilities_entry)
REX_EXPORT(__imp__XamInputGetCapabilitiesEx, rex::kernel::xam::XamInputGetCapabilitiesEx_entry)
REX_EXPORT(__imp__XamInputGetState, rex::kernel::xam::XamInputGetState_entry)
REX_EXPORT(__imp__XamInputSetState, rex::kernel::xam::XamInputSetState_entry)
REX_EXPORT(__imp__XamInputGetKeystroke, rex::kernel::xam::XamInputGetKeystroke_entry)
REX_EXPORT(__imp__XamInputGetKeystrokeEx, rex::kernel::xam::XamInputGetKeystrokeEx_entry)
REX_EXPORT(__imp__XamUserGetDeviceContext, rex::kernel::xam::XamUserGetDeviceContext_entry)

REX_EXPORT_STUB(__imp__XamInputControl);
REX_EXPORT_STUB(__imp__XamInputEnableAutobind);
REX_EXPORT_STUB(__imp__XamInputGetDeviceStats);
REX_EXPORT_STUB(__imp__XamInputGetFailedConnectionOrBind);
REX_EXPORT_STUB(__imp__XamInputGetKeyLocks);
REX_EXPORT_STUB(__imp__XamInputGetKeystrokeHud);
REX_EXPORT_STUB(__imp__XamInputGetKeystrokeHudEx);
REX_EXPORT_STUB(__imp__XamInputGetUserVibrationLevel);
REX_EXPORT_STUB(__imp__XamInputNonControllerGetRaw);
REX_EXPORT_STUB(__imp__XamInputNonControllerGetRawEx);
REX_EXPORT_STUB(__imp__XamInputNonControllerSetRaw);
REX_EXPORT_STUB(__imp__XamInputNonControllerSetRawEx);
REX_EXPORT_STUB(__imp__XamInputRawState);
REX_EXPORT_STUB(__imp__XamInputResetLayoutKeyboard);
REX_EXPORT_STUB(__imp__XamInputSendStayAliveRequest);
REX_EXPORT_STUB(__imp__XamInputSendXenonButtonPress);
REX_EXPORT_STUB(__imp__XamInputSetKeyLocks);
REX_EXPORT_STUB(__imp__XamInputSetKeyboardTranslationHud);
REX_EXPORT_STUB(__imp__XamInputSetLayoutKeyboard);
REX_EXPORT_STUB(__imp__XamInputSetMinMaxAuthDelay);
REX_EXPORT_STUB(__imp__XamInputSetTextMessengerIndicator);
REX_EXPORT_STUB(__imp__XamInputToggleKeyLocks);
