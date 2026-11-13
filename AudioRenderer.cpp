#include "AudioRenderer.h"
#include "BufferTransfer.h"

// Cleans up COM interfaces and format memory when the renderer is destroyed
AudioRenderer::~AudioRenderer() {
  Stop();
  if (pRenderClient) {
    pRenderClient->Release();
    pRenderClient = nullptr;
  }
  if (pAudioClient) {
    pAudioClient->Release();
    pAudioClient = nullptr;
  }
  if (pwfx) {
    CoTaskMemFree(pwfx);
    pwfx = nullptr;
  }
}

// Initializes WASAPI in shared playback mode on the chosen headphone device
bool AudioRenderer::Initialize(IMMDevice *pDevice) {
  if (!pDevice)
    return false;

  HRESULT hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 (void **)&pAudioClient);
  if (FAILED(hr))
    return false;

  hr = pAudioClient->GetMixFormat(&pwfx);
  if (FAILED(hr))
    return false;

  // 100ms buffer duration in 100-nanosecond units (10,000,000 = 1 second)
  REFERENCE_TIME hnsRequestedDuration = 1000000;

  // Note: Flags = 0 (standard playback mode, unlike Loopback mode in
  // AudioCapturer)
  hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                hnsRequestedDuration, 0, pwfx, nullptr);
  if (FAILED(hr))
    return false;

  hr = pAudioClient->GetBufferSize(&bufferFrameCount);
  if (FAILED(hr))
    return false;

  hr = pAudioClient->GetService(__uuidof(IAudioRenderClient),
                                (void **)&pRenderClient);
  return SUCCEEDED(hr);
}

// Tells Windows to start pulling audio from the playback buffer to the
// headphones
bool AudioRenderer::Start() {
  if (!pAudioClient)
    return false;
  return SUCCEEDED(pAudioClient->Start());
}

// Pauses/stops the playback stream
void AudioRenderer::Stop() {
  if (pAudioClient) {
    pAudioClient->Stop();
  }
}

// Checks free space in the headphone's buffer, gets 'dest', fills it via
// TransferToDest, and releases it
bool AudioRenderer::RenderFromBuffer(RingBuffer &ringBuffer) {
  if (!pAudioClient || !pRenderClient || !pwfx)
    return false;

  // 1. Check how many frames are still sitting unplayed in the headphone's
  // buffer
  UINT32 paddingFrames = 0;
  HRESULT hr = pAudioClient->GetCurrentPadding(&paddingFrames);
  if (FAILED(hr))
    return false;

  // 2. Subtract unplayed frames from total buffer size to find empty frames
  // ready to fill
  UINT32 availableFrames = bufferFrameCount - paddingFrames;
  if (availableFrames == 0)
    return true; // Buffer is currently full, nothing to write right now

  // 3. Ask Windows for the raw memory pointer ('dest') to those empty frames
  BYTE *pData = nullptr;
  hr = pRenderClient->GetBuffer(availableFrames, &pData);
  if (FAILED(hr))
    return false;

  // 4. Convert frames to bytes and run our BufferTransfer function into pData
  // ('dest')
  size_t bytesNeeded = availableFrames * pwfx->nBlockAlign;
  TransferToDest(ringBuffer, pData, bytesNeeded);

  // 5. Hand the filled buffer back to Windows so it transmits it over Bluetooth
  hr = pRenderClient->ReleaseBuffer(availableFrames, 0);
  return SUCCEEDED(hr);
}