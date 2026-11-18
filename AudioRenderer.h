#pragma once
#include "RingBuffer.h"
#include <atomic>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>

class AudioRenderer {
private:
  IAudioClient *pAudioClient = nullptr;
  IAudioRenderClient *pRenderClient = nullptr;
  WAVEFORMATEX *pwfx = nullptr;
  UINT32 bufferFrameCount = 0;
  std::atomic<float> volume_{1.0f};

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
};