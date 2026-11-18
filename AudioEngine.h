#pragma once
#include "AudioRenderer.h"
#include "RingBuffer.h"
#include "wasapi.h"
#include <atomic>
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

  std::atomic<bool> isRunning{false};
  IMMDevice *pCaptureDevice = nullptr;
  WAVEFORMATEX *captureFormat = nullptr;

  std::unique_ptr<AudioCapturer> capturer;
  std::vector<std::unique_ptr<OutputStream>> outputs;
  std::thread captureThread;

  std::mutex volumeMutex;
  std::unordered_map<std::wstring, float> deviceVolumes;

  IMMDevice *FindDeviceById(IMMDeviceEnumerator *pEnumerator,
                            const std::wstring &targetId);

public:
  AudioEngine() = default;
  ~AudioEngine();

  // Lists all active playback endpoints and flags the current Windows default
  std::vector<AudioDeviceInfo> GetOutputDevices();

  // Starts loopback capture and spawns dedicated render threads for each target
  // device ID
  std::vector<StreamStatus>
  Start(const std::vector<std::wstring> &targetDeviceIds);

  // Stops all capture/render threads and releases WASAPI resources cleanly
  void Stop();

  // Updates volume (0.0f to 1.5f) for a specific device before or during
  // streaming
  void SetDeviceVolume(const std::wstring &deviceId, float volume);

  bool IsRunning() const { return isRunning.load(); }
};