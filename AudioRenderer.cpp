#include "AudioRenderer.h"
#include "BufferTransfer.h"
#include <cstring>
#include <iostream>

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

bool AudioRenderer::Initialize(IMMDevice *pDevice,
                               const WAVEFORMATEX *captureFormat) {
  if (!pDevice || !captureFormat)
    return false;

  HRESULT hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 (void **)&pAudioClient);
  if (FAILED(hr)) {
    std::cout << "Activate failed: 0x" << std::hex << hr << std::dec << "\n";
    return false;
  }

  size_t formatSize = sizeof(WAVEFORMATEX) + captureFormat->cbSize;
  pwfx = (WAVEFORMATEX *)CoTaskMemAlloc(formatSize);
  if (!pwfx)
    return false;
  std::memcpy(pwfx, captureFormat, formatSize);

  REFERENCE_TIME hnsRequestedDuration = 1000000; // 100ms buffer
  DWORD flags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

  hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags,
                                hnsRequestedDuration, 0, pwfx, nullptr);
  if (FAILED(hr)) {
    std::cout << "Initialize failed: 0x" << std::hex << hr << std::dec << "\n";
    return false;
  }

  hr = pAudioClient->GetBufferSize(&bufferFrameCount);
  if (FAILED(hr))
    return false;

  hr = pAudioClient->GetService(__uuidof(IAudioRenderClient),
                                (void **)&pRenderClient);
  if (FAILED(hr)) {
    std::cout << "GetService failed: 0x" << std::hex << hr << std::dec << "\n";
    return false;
  }

  return true;
}

bool AudioRenderer::Start() {
  if (!pAudioClient)
    return false;
  return SUCCEEDED(pAudioClient->Start());
}

void AudioRenderer::Stop() {
  if (pAudioClient) {
    pAudioClient->Stop();
  }
}

bool AudioRenderer::RenderFromBuffer(RingBuffer &ringBuffer) {
  if (!pAudioClient || !pRenderClient || !pwfx)
    return false;

  UINT32 paddingFrames = 0;
  HRESULT hr = pAudioClient->GetCurrentPadding(&paddingFrames);
  if (FAILED(hr))
    return false;

  UINT32 availableFrames = bufferFrameCount - paddingFrames;
  if (availableFrames == 0)
    return true;

  BYTE *dest = nullptr;
  hr = pRenderClient->GetBuffer(availableFrames, &dest);
  if (FAILED(hr))
    return false;

  size_t bytesNeeded = availableFrames * pwfx->nBlockAlign;
  TransferToDest(ringBuffer, dest, bytesNeeded);

  hr = pRenderClient->ReleaseBuffer(availableFrames, 0);
  return SUCCEEDED(hr);
}