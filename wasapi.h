#pragma once
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>


class AudioCapturer {
private:
  IAudioClient *pAudioClient = nullptr;
  IAudioCaptureClient *pCaptureClient = nullptr;
  WAVEFORMATEX *pwfx = nullptr;
  UINT32 bufferFrameCount = 0;

public:
  const WAVEFORMATEX *GetFormat() const { return pwfx; }
  bool Initialize(IMMDevice *pTargetDevice);
};