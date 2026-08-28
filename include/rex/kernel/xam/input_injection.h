#pragma once

#include <cstddef>
#include <cstdint>

namespace rex::kernel::xam {

struct SyntheticInputStep {
  uint16_t buttons;
  uint32_t poll_count;
  uint8_t left_trigger = 0;
  uint8_t right_trigger = 0;
};

void QueueSyntheticInput(uint16_t buttons, uint32_t poll_count);
void QueueSyntheticInput(uint16_t buttons, uint8_t left_trigger, uint8_t right_trigger,
                         uint32_t poll_count);
void QueueSyntheticInputSequence(const SyntheticInputStep* steps, size_t step_count);
void SetSyntheticAutoTap(uint16_t buttons, bool enabled);
void ClearSyntheticInput();

// When suppressed, XamInputGetState returns a fully-neutral gamepad (no buttons,
// zero triggers/sticks) so the guest player is "frozen" -- used by online game
// modes (S.K.A.T.E.) to freeze waiting players. Synthetic injections above still
// apply (so a scripted reset can be injected even while suppressed).
void SetInputSuppressed(bool suppressed);
bool IsInputSuppressed();

// When enabled, XamInputGetState logs the FINAL gamepad state the guest reads
// (after freeze + synthetic injection), rate-limited, as [input-read] -- so the
// input path (real input arriving, freeze zeroing it, a reset injection) can be
// verified straight from the log without watching the skater.
void SetInputLogging(bool enabled);

// This player's board stance for the S.K.A.T.E. flick-it trick detector:
// 0 = goofy (default), 1 = regular. Regular mirrors the left/right flick
// direction (goofy kickflip = flick Up-Right; regular kickflip = Up-Left).
void SetTrickStance(int stance);

// Snapshot of the LAST completed right-stick flick gesture (for auto-stance
// correlation against the game's HUD trick name). `gesture_seq` monotonically
// increments each time a gesture completes -- callers poll it and, on a
// change, examine the raw flick sign vs the game's reported trick name to
// infer stance without asking the player. `pull_down` = the wind-up went
// down (regular pop) vs up (nollie). `flick_rx` is the peak sideways lean at
// flick time: > 0 = flicked right, < 0 = flicked left (stance-agnostic).
struct LastFlickSnapshot {
  uint64_t gesture_seq = 0;
  int32_t flick_rx = 0;
  int32_t flick_ry = 0;
  bool pull_down = false;
  bool have_flick = false;
};
LastFlickSnapshot GetLastFlick();

}  // namespace rex::kernel::xam
