#pragma once
#include <audioclient.h>
#include <functional>
#include <mmdeviceapi.h>
#include <windows.h>

class AudioCapturer {
private:
  IAudioClient *pAudioClient = nullptr;
  IAudioCaptureClient *pCaptureClient = nullptr;
  WAVEFORMATEX *pwfx = nullptr;
  UINT32 bufferFrameCount = 0;

public:
  ~AudioCapturer();

  const WAVEFORMATEX *GetFormat() const { return pwfx; }
  bool Initialize(IMMDevice *pTargetDevice);
  bool Start();
  void Stop();
  bool
  ReadPackets(const std::function<void(const BYTE *, size_t)> &onAudioData);
};