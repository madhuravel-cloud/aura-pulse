#pragma once
#include "AudioRenderer.h"
#include "RingBuffer.h"
#include "wasapi.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mmdeviceapi.h>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <windows.h>

struct AudioDeviceInfo {
  std::wstring id;
  std::wstring name;
  bool isDefault = false;
};

struct StreamStatus {
  std::wstring id;
  std::wstring name;
  bool active = false;
  std::string statusMessage;
};

class AudioEngine {
private:
  struct OutputStream {
    std::wstring id;
    std::wstring name;
    IMMDevice *pDevice = nullptr;
    std::unique_ptr<RingBuffer> ringBuffer;
    std::unique_ptr<AudioRenderer> renderer;
    std::thread renderThread;
    std::atomic<bool> active{false};

    ~OutputStream();
  };

  class DeviceNotificationClient;

  std::atomic<bool> isRunning{false};
  IMMDevice *pCaptureDevice = nullptr;
  WAVEFORMATEX *captureFormat = nullptr;

  std::unique_ptr<AudioCapturer> capturer;
  std::vector<std::unique_ptr<OutputStream>> outputs;
  std::thread captureThread;

  std::mutex settingsMutex;
  std::unordered_map<std::wstring, float> deviceVolumes;
  std::unordered_map<std::wstring, uint32_t> deviceDelaysMs;

  std::mutex callbackMutex;
  std::function<void()> onDevicesChangedCallback;
  IMMDeviceEnumerator *pNotifyEnumerator = nullptr;
  DeviceNotificationClient *pNotifyClient = nullptr;

  IMMDevice *FindDeviceById(IMMDeviceEnumerator *pEnumerator,
                            const std::wstring &targetId);
  void NotifyDevicesChanged();

public:
  AudioEngine();
  ~AudioEngine();

  std::vector<AudioDeviceInfo> GetOutputDevices();
  std::vector<StreamStatus>
  Start(const std::vector<std::wstring> &targetDeviceIds);
  void Stop();

  void SetDeviceVolume(const std::wstring &deviceId, float volume);
  void SetDeviceDelay(const std::wstring &deviceId, uint32_t delayMs);
  void SetDevicesChangedCallback(std::function<void()> callback);

  bool IsRunning() const { return isRunning.load(); }
};