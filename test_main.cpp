#include "AudioRenderer.h"
#include "RingBuffer.h"
#include "wasapi.h"
#include <atomic>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <mmdeviceapi.h>
#include <thread>
#include <windows.h>

#pragma comment(lib, "ole32.lib")

int main() {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  IMMDeviceEnumerator *pEnumerator = nullptr;
  CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                   __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator);

  IMMDeviceCollection *pCollection = nullptr;
  pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);

  UINT count = 0;
  pCollection->GetCount(&count);

  std::cout << "Available Output Devices:\n";
  for (UINT i = 0; i < count; ++i) {
    IMMDevice *pDev = nullptr;
    pCollection->Item(i, &pDev);

    IPropertyStore *pProps = nullptr;
    pDev->OpenPropertyStore(STGM_READ, &pProps);

    PROPVARIANT varName;
    PropVariantInit(&varName);
    pProps->GetValue(PKEY_Device_FriendlyName, &varName);

    std::wcout << L"  [" << i << L"] " << varName.pwszVal << L"\n";

    PropVariantClear(&varName);
    pProps->Release();
    pDev->Release();
  }

  UINT choice1 = 0, choice2 = 0;
  std::cout << "\nEnter FIRST headphone index: ";
  std::cin >> choice1;
  std::cout << "Enter SECOND headphone index: ";
  std::cin >> choice2;

  if (choice1 >= count || choice2 >= count || choice1 == choice2) {
    std::cout << "Pick two different valid device numbers.\n";
    return 1;
  }

  // Make sure Windows default output is set to Speakers (Senary Audio) before
  // running!
  IMMDevice *pCaptureDevice = nullptr;
  pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pCaptureDevice);

  IMMDevice *pRenderDev1 = nullptr;
  IMMDevice *pRenderDev2 = nullptr;
  pCollection->Item(choice1, &pRenderDev1);
  pCollection->Item(choice2, &pRenderDev2);

  // Grab capture device mix format so both renderers auto-convert to match it
  IAudioClient *pTempClient = nullptr;
  WAVEFORMATEX *captureFormat = nullptr;
  pCaptureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           (void **)&pTempClient);
  pTempClient->GetMixFormat(&captureFormat);
  pTempClient->Release();

  AudioCapturer capturer;
  if (!capturer.Initialize(pCaptureDevice)) {
    std::cout << "Failed to initialize AudioCapturer.\n";
    return 1;
  }

  // Dedicated 1-second RingBuffer and AudioRenderer per headphone
  RingBuffer rb1(captureFormat->nAvgBytesPerSec);
  RingBuffer rb2(captureFormat->nAvgBytesPerSec);

  AudioRenderer renderer1;
  if (!renderer1.Initialize(pRenderDev1, captureFormat)) {
    std::cout << "Failed to initialize Headphone 1.\n";
    return 1;
  }

  AudioRenderer renderer2;
  if (!renderer2.Initialize(pRenderDev2, captureFormat)) {
    std::cout << "Failed to initialize Headphone 2.\n";
    return 1;
  }

  capturer.Start();
  renderer1.Start();
  renderer2.Start();

  std::atomic<bool> isRunning{true};

  // Thread 1: Capture Thread -> duplicates packets into BOTH RingBuffers
  std::thread captureThread([&]() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    while (isRunning) {
      capturer.ReadPackets([&](const BYTE *data, size_t bytes) {
        rb1.Write(data, bytes);
        rb2.Write(data, bytes);
      });
      Sleep(5);
    }
    CoUninitialize();
  });

  // Thread 2: Playback Thread for Headphone 1
  std::thread renderThread1([&]() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    while (isRunning) {
      renderer1.RenderFromBuffer(rb1);
      Sleep(5);
    }
    CoUninitialize();
  });

  // Thread 3: Playback Thread for Headphone 2
  std::thread renderThread2([&]() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    while (isRunning) {
      renderer2.RenderFromBuffer(rb2);
      Sleep(5);
    }
    CoUninitialize();
  });

  std::cout << "\nStreaming to BOTH headphones for 15 seconds! Play a song or "
               "video...\n";
  Sleep(15000);

  isRunning = false;
  captureThread.join();
  renderThread1.join();
  renderThread2.join();

  capturer.Stop();
  renderer1.Stop();
  renderer2.Stop();

  CoTaskMemFree(captureFormat);
  pRenderDev1->Release();
  pRenderDev2->Release();
  pCaptureDevice->Release();
  pCollection->Release();
  pEnumerator->Release();
  CoUninitialize();

  std::cout << "Done!\n";
  return 0;
}