/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <queue>
#include <stack>

#include <rex/audio/audio_driver.h>
#include <rex/thread.h>

#include <SDL3/SDL.h>

namespace rex::audio::sdl {

class SDLAudioDriver : public AudioDriver {
 public:
  SDLAudioDriver(memory::Memory* memory, rex::thread::Semaphore* semaphore);
  ~SDLAudioDriver() override;

  bool Initialize();
  void SubmitFrame(uint32_t frame_ptr) override;
  void Shutdown();

 protected:
  static void SDLCallback(void* userdata, SDL_AudioStream* stream, int additional_amount,
                          int total_amount);

  // Releases guest frame credits, paced to real time when
  // audio_realtime_credit_pacing is enabled: a misbehaving audio device that
  // drains the stream faster than real time (e.g. a Bluetooth device in a
  // broken state) would otherwise release credits at an unbounded rate,
  // speeding up the guest's whole audio clock and anything paced by it (such
  // as video playback). Called only from the SDL audio thread.
  void ReleasePacedCredits(uint32_t new_credits);

  rex::thread::Semaphore* semaphore_ = nullptr;

  SDL_AudioStream* sdl_stream_ = nullptr;
  bool sdl_initialized_ = false;
  uint8_t sdl_device_channels_ = 0;

  static const uint32_t frame_frequency_ = 48000;
  static const uint32_t frame_channels_ = 6;
  static const uint32_t channel_samples_ = 256;
  static const uint32_t frame_samples_ = frame_channels_ * channel_samples_;
  static const uint32_t frame_size_ = sizeof(float) * frame_samples_;
  std::queue<float*> frames_queued_ = {};
  std::stack<float*> frames_unused_ = {};
  std::mutex frames_mutex_ = {};

  // Credit pacing state - only touched on the SDL audio thread. The allowance
  // accumulates with wall time (slightly above real time to tolerate device
  // clock drift) and is capped so pauses don't bank an unbounded burst.
  double pace_allowance_frames_ = 4.0;
  uint64_t pace_last_ns_ = 0;
  uint64_t pace_deferred_credits_ = 0;
};

}  // namespace rex::audio::sdl
