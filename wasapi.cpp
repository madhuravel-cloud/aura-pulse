#include "wasapi.h"

bool AudioCapturer::Initialize(IMMDevice *pTargetDevice) {
  if (!pTargetDevice)
    return false;

  // 1. Activate the IAudioClient interface on the target playback device
  HRESULT hr = pTargetDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
                                       (void **)&pAudioClient);
  if (FAILED(hr) || !pAudioClient) {
    return false;
  }

  // 2. Query the exact audio format the Windows mixer is running at
  hr = pAudioClient->GetMixFormat(&pwfx);
  if (FAILED(hr) || !pwfx) {
    pAudioClient->Release();
    pAudioClient = nullptr;
    return false;
  }

  // 3. Initialize the audio client in Shared Loopback mode
  REFERENCE_TIME hnsRequestedDuration = 10000000;
  hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_LOOPBACK,
                                hnsRequestedDuration, 0, pwfx, NULL);
  if (FAILED(hr)) {
    CoTaskMemFree(pwfx);
    pwfx = nullptr;
    pAudioClient->Release();
    pAudioClient = nullptr;
    return false;
  }

  // 4. Retrieve the actual allocated buffer size in frames
  hr = pAudioClient->GetBufferSize(&bufferFrameCount);
  if (FAILED(hr)) {
    CoTaskMemFree(pwfx);
    pwfx = nullptr;
    pAudioClient->Release();
    pAudioClient = nullptr;
    return false;
  }

  // 5. Grab the IAudioCaptureClient service to read incoming loopback packets
  hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient),
                                (void **)&pCaptureClient);
  if (FAILED(hr) || !pCaptureClient) {
    CoTaskMemFree(pwfx);
    pwfx = nullptr;
    pAudioClient->Release();
    pAudioClient = nullptr;
    return false;
  }

  return true;
}