#include "AudioRenderer.h"
#include "BufferTransfer.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mmreg.h>

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

  // Apply per-device volume gain
  const float vol = volume_.load(std::memory_order_relaxed);
  if (vol != 1.0f) {
    if (vol == 0.0f) {
      std::memset(dest, 0, bytesNeeded);
    } else {
      bool isFloat = (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
      if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
          pwfx->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        auto *wfex = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(pwfx);
        isFloat = (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
      }

      if (isFloat && pwfx->wBitsPerSample == 32) {
        size_t sampleCount = bytesNeeded / sizeof(float);
        float *samples = reinterpret_cast<float *>(dest);
        for (size_t i = 0; i < sampleCount; ++i) {
          samples[i] = std::clamp(samples[i] * vol, -1.0f, 1.0f);
        }
      } else if (pwfx->wBitsPerSample == 16) {
        size_t sampleCount = bytesNeeded / sizeof(int16_t);
        int16_t *samples = reinterpret_cast<int16_t *>(dest);
        for (size_t i = 0; i < sampleCount; ++i) {
          float scaled = static_cast<float>(samples[i]) * vol;
          samples[i] =
              static_cast<int16_t>(std::clamp(scaled, -32768.0f, 32767.0f));
        }
      }
    }
  }

  hr = pRenderClient->ReleaseBuffer(availableFrames, 0);
  return SUCCEEDED(hr);
}