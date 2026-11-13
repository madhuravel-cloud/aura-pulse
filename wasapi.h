#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

class AudioCapturer {
private:
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    WAVEFORMATEX* pwfx = nullptr;
    UINT32 bufferFrameCount = 0;

public:
    ~AudioCapturer();

    const WAVEFORMATEX* GetFormat() const { return pwfx; }
    bool Initialize(IMMDevice* pTargetDevice);
    bool Start();
    void Stop();
};