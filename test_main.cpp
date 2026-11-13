#include "wasapi.h"
#include <chrono>
#include <iostream>
#include <thread>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

int main() {
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    std::cout << "COM init failed\n";
    return 1;
  }

  IMMDeviceEnumerator *pEnumerator = nullptr;
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator);

  IMMDevice *pDefaultDevice = nullptr;
  if (SUCCEEDED(hr) && pEnumerator) {
    pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefaultDevice);
  }

  AudioCapturer capturer;
  if (!capturer.Initialize(pDefaultDevice)) {
    std::cout << "AudioCapturer Initialize failed.\n";
    return 1;
  }

  if (!capturer.Start()) {
    std::cout << "AudioCapturer Start failed.\n";
    return 1;
  }

  std::cout << "Capturing loopback audio for 2 seconds... (Play a video/song "
               "to see active bytes!)\n\n";

  size_t totalBytesCaptured = 0;
  size_t packetCount = 0;

  // Poll every 10ms for 200 iterations (~2 seconds)
  for (int i = 0; i < 200; ++i) {
    bool ok = capturer.ReadPackets([&](const BYTE *pData, size_t byteSize) {
      packetCount++;
      totalBytesCaptured += byteSize;

      // Print every 25th packet so we don't flood the console
      if (packetCount % 25 == 0) {
        if (pData == nullptr) {
          std::cout << "[Packet " << packetCount << "] " << byteSize
                    << " bytes (SILENT)\n";
        } else {
          // Cast to float* to peek at the very first audio sample
          const float *samples = reinterpret_cast<const float *>(pData);
          std::cout << "[Packet " << packetCount << "] " << byteSize
                    << " bytes | First sample: " << samples[0] << "\n";
        }
      }
    });

    if (!ok) {
      std::cout << "ReadPackets encountered an error!\n";
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  capturer.Stop();
  std::cout << "\nDone! Captured " << packetCount << " packets ("
            << totalBytesCaptured << " total bytes).\n";

  if (pDefaultDevice)
    pDefaultDevice->Release();
  if (pEnumerator)
    pEnumerator->Release();
  CoUninitialize();
  return 0;
}