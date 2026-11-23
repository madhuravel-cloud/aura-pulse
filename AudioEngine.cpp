#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "AudioEngine.h"
#include <functiondiscoverykeys_devpkey.h>

// COM listener for real-time Windows audio endpoint plug/unplug/default events
class AudioEngine::DeviceNotificationClient : public IMMNotificationClient {
private:
  LONG refCount_{1};
  AudioEngine *engine_;

public:
  explicit DeviceNotificationClient(AudioEngine *engine) : engine_(engine) {}

  ULONG STDMETHODCALLTYPE AddRef() override {
    return InterlockedIncrement(&refCount_);
  }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = InterlockedDecrement(&refCount_);
    if (ref == 0) {
      delete this;
    }
    return ref;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           VOID **ppvInterface) override {
    if (!ppvInterface)
      return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
      *ppvInterface = static_cast<IMMNotificationClient *>(this);
      AddRef();
      return S_OK;
    }
    *ppvInterface = nullptr;
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
    if (engine_)
      engine_->NotifyDevicesChanged();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
    if (engine_)
      engine_->NotifyDevicesChanged();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
    if (engine_)
      engine_->NotifyDevicesChanged();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role,
                                                   LPCWSTR) override {
    if (engine_ && flow == eRender && role == eConsole) {
      engine_->NotifyDevicesChanged();
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,
                                                   const PROPERTYKEY) override {
    return S_OK;
  }
};

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

AudioEngine::AudioEngine() {
  HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                (void **)&pNotifyEnumerator);
  if (SUCCEEDED(hr) && pNotifyEnumerator) {
    pNotifyClient = new DeviceNotificationClient(this);
    pNotifyEnumerator->RegisterEndpointNotificationCallback(pNotifyClient);
  }
  if (SUCCEEDED(hrCo)) {
    CoUninitialize();
  }
}

AudioEngine::~AudioEngine() {
  Stop();
  SetDevicesChangedCallback(nullptr);

  if (pNotifyEnumerator && pNotifyClient) {
    pNotifyEnumerator->UnregisterEndpointNotificationCallback(pNotifyClient);
  }
  if (pNotifyClient) {
    pNotifyClient->Release();
    pNotifyClient = nullptr;
  }
  if (pNotifyEnumerator) {
    pNotifyEnumerator->Release();
    pNotifyEnumerator = nullptr;
  }
}

void AudioEngine::SetDevicesChangedCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(callbackMutex);
  onDevicesChangedCallback = std::move(callback);
}

void AudioEngine::NotifyDevicesChanged() {
  std::function<void()> cb;
  {
    std::lock_guard<std::mutex> lock(callbackMutex);
    cb = onDevicesChangedCallback;
  }
  if (cb) {
    cb();
  }
}

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

void AudioEngine::SetDeviceVolume(const std::wstring &deviceId, float volume) {
  std::lock_guard<std::mutex> lock(settingsMutex);
  deviceVolumes[deviceId] = volume;

  for (auto &out : outputs) {
    if (out && out->id == deviceId && out->renderer) {
      out->renderer->SetVolume(volume);
    }
  }
}

void AudioEngine::SetDeviceDelay(const std::wstring &deviceId,
                                 uint32_t delayMs) {
  std::lock_guard<std::mutex> lock(settingsMutex);
  deviceDelaysMs[deviceId] = delayMs;

  for (auto &out : outputs) {
    if (out && out->id == deviceId && out->renderer) {
      out->renderer->SetDelayMs(delayMs);
    }
  }
}

std::vector<StreamStatus>
AudioEngine::Start(const std::vector<std::wstring> &targetDeviceIds) {
  Stop();
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

    {
      std::lock_guard<std::mutex> lock(settingsMutex);
      auto volIt = deviceVolumes.find(targetId);
      if (volIt != deviceVolumes.end()) {
        stream->renderer->SetVolume(volIt->second);
      }
      auto delayIt = deviceDelaysMs.find(targetId);
      if (delayIt != deviceDelaysMs.end()) {
        stream->renderer->SetDelayMs(delayIt->second);
      }
    }

    if (stream->renderer->Initialize(stream->pDevice, captureFormat)) {
      stream->renderer->Start();
      stream->active = true;
      results.push_back({targetId, devName, true, "Streaming active"});
      outputs.push_back(std::move(stream));
    } else {
      results.push_back({targetId, devName, false,
                         "Endpoint rejected stream (Windows single-A2DP limit "
                         "or device busy)"});
    }
  }

  pEnumerator->Release();

  if (outputs.empty()) {
    Stop();
    return results;
  }

  isRunning = true;

  for (auto &out : outputs) {
    OutputStream *rawOut = out.get();
    rawOut->renderThread = std::thread([this, rawOut]() {
      CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      while (isRunning.load() && rawOut->active.load()) {
        if (!rawOut->renderer->RenderFromBuffer(*rawOut->ringBuffer)) {
          rawOut->active = false;
          NotifyDevicesChanged();
          break;
        }
        Sleep(5);
      }
      CoUninitialize();
    });
  }

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