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

public:
  ~AudioCapturer();

  bool Initialize(IMMDevice *pDevice);
  bool Start();
  void Stop();

  // Reads all available loopback packets and passes each chunk to your callback
  bool ReadPackets(const std::function<void(const BYTE *, size_t)> &onPacket);
};