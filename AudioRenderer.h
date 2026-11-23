#pragma once
#include "RingBuffer.h"
#include <atomic>
#include <audioclient.h>
#include <cstdint>
#include <mmdeviceapi.h>
#include <vector>
#include <windows.h>

class AudioRenderer {
private:
  IAudioClient *pAudioClient = nullptr;
  IAudioRenderClient *pRenderClient = nullptr;
  WAVEFORMATEX *pwfx = nullptr;
  UINT32 bufferFrameCount = 0;

  std::atomic<float> volume_{1.0f};
  std::atomic<uint32_t> delayMs_{0};

  std::vector<BYTE> delayBuffer_;
  size_t delayWritePos_ = 0;

public:
  AudioRenderer() = default;
  ~AudioRenderer();

  bool Initialize(IMMDevice *pDevice, const WAVEFORMATEX *captureFormat);
  bool Start();
  void Stop();
  bool RenderFromBuffer(RingBuffer &ringBuffer);

  void SetVolume(float volume) {
    if (volume < 0.0f)
      volume = 0.0f;
    if (volume > 1.5f)
      volume = 1.5f;
    volume_.store(volume, std::memory_order_relaxed);
  }

  float GetVolume() const { return volume_.load(std::memory_order_relaxed); }

  void SetDelayMs(uint32_t ms) {
    if (ms > 300)
      ms = 300;
    delayMs_.store(ms, std::memory_order_relaxed);
  }

  uint32_t GetDelayMs() const {
    return delayMs_.load(std::memory_order_relaxed);
  }
};