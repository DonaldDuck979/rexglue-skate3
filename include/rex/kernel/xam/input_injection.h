#pragma once

#include <cstddef>
#include <cstdint>

namespace rex::kernel::xam {

struct SyntheticInputStep {
  uint16_t buttons;
  uint32_t poll_count;
};

void QueueSyntheticInput(uint16_t buttons, uint32_t poll_count);
void QueueSyntheticInputSequence(const SyntheticInputStep* steps, size_t step_count);
void SetSyntheticAutoTap(uint16_t buttons, bool enabled);
void ClearSyntheticInput();

}  // namespace rex::kernel::xam
