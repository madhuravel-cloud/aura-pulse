#pragma once
#include "RingBuffer.h"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>

class AudioRenderer {
private:
  IAudioClient *pAudioClient = nullptr;
  IAudioRenderClient *pRenderClient = nullptr;
  WAVEFORMATEX *pwfx = nullptr;
  UINT32 bufferFrameCount = 0;

public:
  ~AudioRenderer();

  bool Initialize(IMMDevice *pDevice, const WAVEFORMATEX *captureFormat);
  bool Start();
  void Stop();
  bool RenderFromBuffer(RingBuffer &ringBuffer);
};