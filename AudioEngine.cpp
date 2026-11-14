#include "AudioEngine.h"
#include <functiondiscoverykeys_devpkey.h>

AudioEngine::OutputStream::~OutputStream() {
  active = false;
  if (renderThread.joinable()) {
    renderThread.join();
  }
  if (renderer) {
    renderer->Stop();
  }
  if (pDevice) {
    pDevice->Release();
    pDevice = nullptr;
  }
}

AudioEngine::~AudioEngine() { Stop(); }

std::vector<AudioDeviceInfo> AudioEngine::GetOutputDevices() {
  std::vector<AudioDeviceInfo> list;
  HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  IMMDeviceEnumerator *pEnumerator = nullptr;
  HRESULT hr =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                       __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator);
  if (FAILED(hr) || !pEnumerator) {
    if (SUCCEEDED(hrCo))
      CoUninitialize();
    return list;
  }

  // Grab current default playback ID so the UI/caller can exclude it
  std::wstring defaultId;
  IMMDevice *pDefault = nullptr;
  if (SUCCEEDED(
          pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefault)) &&
      pDefault) {
    LPWSTR rawId = nullptr;
    if (SUCCEEDED(pDefault->GetId(&rawId)) && rawId) {
      defaultId = rawId;
      CoTaskMemFree(rawId);
    }
    pDefault->Release();
  }

  IMMDeviceCollection *pCollection = nullptr;
  if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                                &pCollection)) &&
      pCollection) {
    UINT count = 0;
    pCollection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
      IMMDevice *pDev = nullptr;
      if (FAILED(pCollection->Item(i, &pDev)) || !pDev)
        continue;

      LPWSTR rawId = nullptr;
      pDev->GetId(&rawId);
      std::wstring devId = rawId ? rawId : L"";
      if (rawId)
        CoTaskMemFree(rawId);

      std::wstring friendlyName = L"Unknown Audio Device";
      IPropertyStore *pProps = nullptr;
      if (SUCCEEDED(pDev->OpenPropertyStore(STGM_READ, &pProps)) && pProps) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) &&
            varName.pwszVal) {
          friendlyName = varName.pwszVal;
        }
        PropVariantClear(&varName);
        pProps->Release();
      }

      list.push_back({devId, friendlyName, devId == defaultId});
      pDev->Release();
    }
    pCollection->Release();
  }

  pEnumerator->Release();
  if (SUCCEEDED(hrCo))
    CoUninitialize();
  return list;
}

IMMDevice *AudioEngine::FindDeviceById(IMMDeviceEnumerator *pEnumerator,
                                       const std::wstring &targetId) {
  IMMDevice *pDevice = nullptr;
  if (SUCCEEDED(pEnumerator->GetDevice(targetId.c_str(), &pDevice))) {
    return pDevice;
  }
  return nullptr;
}

std::vector<StreamStatus>
AudioEngine::Start(const std::vector<std::wstring> &targetDeviceIds) {
  Stop(); // Ensure clean slate before starting
  std::vector<StreamStatus> results;

  if (targetDeviceIds.empty())
    return results;

  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  IMMDeviceEnumerator *pEnumerator = nullptr;
  HRESULT hr =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                       __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator);
  if (FAILED(hr) || !pEnumerator)
    return results;

  // 1. Setup loopback capture on current Windows default output
  hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pCaptureDevice);
  if (FAILED(hr) || !pCaptureDevice) {
    pEnumerator->Release();
    return results;
  }

  LPWSTR rawDefId = nullptr;
  pCaptureDevice->GetId(&rawDefId);
  std::wstring defaultId = rawDefId ? rawDefId : L"";
  if (rawDefId)
    CoTaskMemFree(rawDefId);

  IAudioClient *pTempClient = nullptr;
  if (SUCCEEDED(pCaptureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                         nullptr, (void **)&pTempClient))) {
    pTempClient->GetMixFormat(&captureFormat);
    pTempClient->Release();
  }

  if (!captureFormat) {
    Stop();
    pEnumerator->Release();
    return results;
  }

  capturer = std::make_unique<AudioCapturer>();
  if (!capturer->Initialize(pCaptureDevice)) {
    Stop();
    pEnumerator->Release();
    return results;
  }

  // 2. Initialize independent RingBuffer + AudioRenderer per requested endpoint
  for (const auto &targetId : targetDeviceIds) {
    if (targetId == defaultId) {
      results.push_back(
          {targetId, L"", false,
           "Skipped: cannot route default capture device to itself"});
      continue;
    }

    IMMDevice *pDev = FindDeviceById(pEnumerator, targetId);
    if (!pDev) {
      results.push_back(
          {targetId, L"", false, "Device not found or disconnected"});
      continue;
    }

    std::wstring devName = L"Audio Endpoint";
    IPropertyStore *pProps = nullptr;
    if (SUCCEEDED(pDev->OpenPropertyStore(STGM_READ, &pProps)) && pProps) {
      PROPVARIANT varName;
      PropVariantInit(&varName);
      if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) &&
          varName.pwszVal) {
        devName = varName.pwszVal;
      }
      PropVariantClear(&varName);
      pProps->Release();
    }

    auto stream = std::make_unique<OutputStream>();
    stream->id = targetId;
    stream->name = devName;
    stream->pDevice = pDev;
    stream->ringBuffer =
        std::make_unique<RingBuffer>(captureFormat->nAvgBytesPerSec);
    stream->renderer = std::make_unique<AudioRenderer>();

    if (stream->renderer->Initialize(stream->pDevice, captureFormat)) {
      stream->renderer->Start();
      stream->active = true;
      results.push_back({targetId, devName, true, "Streaming active"});
      outputs.push_back(std::move(stream));
    } else {
      results.push_back({targetId, devName, false,
                         "Endpoint rejected stream (Windows single-A2DP limit "
                         "or device busy)"});
      // stream destructor automatically releases pDevice
    }
  }

  pEnumerator->Release();

  if (outputs.empty()) {
    Stop();
    return results;
  }

  // 3. Spin up consumer threads (1 per active output)
  isRunning = true;

  for (auto &out : outputs) {
    OutputStream *rawOut = out.get();
    rawOut->renderThread = std::thread([this, rawOut]() {
      CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      while (isRunning.load() && rawOut->active.load()) {
        if (!rawOut->renderer->RenderFromBuffer(*rawOut->ringBuffer)) {
          rawOut->active = false;
          break;
        }
        Sleep(5);
      }
      CoUninitialize();
    });
  }

  // 4. Spin up producer capture thread fanning out packets to all active
  // outputs
  capturer->Start();
  captureThread = std::thread([this]() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    while (isRunning.load()) {
      capturer->ReadPackets([this](const BYTE *data, size_t bytes) {
        for (auto &out : outputs) {
          if (out->active.load()) {
            out->ringBuffer->Write(data, bytes);
          }
        }
      });
      Sleep(5);
    }
    CoUninitialize();
  });

  return results;
}

void AudioEngine::Stop() {
  isRunning = false;

  if (captureThread.joinable()) {
    captureThread.join();
  }

  if (capturer) {
    capturer->Stop();
    capturer.reset();
  }

  // Clearing outputs triggers ~OutputStream(), joining each renderThread and
  // stopping each renderer
  outputs.clear();

  if (captureFormat) {
    CoTaskMemFree(captureFormat);
    captureFormat = nullptr;
  }

  if (pCaptureDevice) {
    pCaptureDevice->Release();
    pCaptureDevice = nullptr;
  }
}