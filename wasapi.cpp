#include "wasapi.h"

AudioCapturer::~AudioCapturer() {
  Stop();
  if (pCaptureClient) {
    pCaptureClient->Release();
    pCaptureClient = nullptr;
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

bool AudioCapturer::Initialize(IMMDevice *pDevice) {
  if (!pDevice)
    return false;

  HRESULT hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 (void **)&pAudioClient);
  if (FAILED(hr))
    return false;

  hr = pAudioClient->GetMixFormat(&pwfx);
  if (FAILED(hr))
    return false;

  REFERENCE_TIME hnsRequestedDuration = 1000000; // 100ms buffer
  hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_LOOPBACK,
                                hnsRequestedDuration, 0, pwfx, nullptr);
  if (FAILED(hr))
    return false;

  hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient),
                                (void **)&pCaptureClient);
  return SUCCEEDED(hr);
}

bool AudioCapturer::Start() {
  if (!pAudioClient)
    return false;
  return SUCCEEDED(pAudioClient->Start());
}

void AudioCapturer::Stop() {
  if (pAudioClient) {
    pAudioClient->Stop();
  }
}

bool AudioCapturer::ReadPackets(
    const std::function<void(const BYTE *, size_t)> &onPacket) {
  if (!pCaptureClient || !pwfx)
    return false;

  UINT32 packetLength = 0;
  HRESULT hr = pCaptureClient->GetNextPacketSize(&packetLength);
  if (FAILED(hr))
    return false;

  while (packetLength != 0) {
    BYTE *pData = nullptr;
    UINT32 numFramesAvailable = 0;
    DWORD flags = 0;

    hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr,
                                   nullptr);
    if (FAILED(hr))
      return false;

    size_t bytesToRead = numFramesAvailable * pwfx->nBlockAlign;

    if (onPacket) {
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        onPacket(nullptr,
                 bytesToRead); // RingBuffer::Write(nullptr, ...) fills zeros
      } else {
        onPacket(pData, bytesToRead);
      }
    }

    hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
    if (FAILED(hr))
      return false;

    hr = pCaptureClient->GetNextPacketSize(&packetLength);
    if (FAILED(hr))
      return false;
  }

  return true;
}