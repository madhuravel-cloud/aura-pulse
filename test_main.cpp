#include "wasapi.h"
#include <iostream>


// Automatically tells Visual Studio to link the Windows COM libraries
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
  if (capturer.Initialize(pDefaultDevice)) {
    const WAVEFORMATEX *fmt = capturer.GetFormat();
    std::cout << "Loopback Initialized!\n";
    std::cout << "Sample Rate: " << fmt->nSamplesPerSec << " Hz\n";
    std::cout << "Channels:    " << fmt->nChannels << "\n";
    std::cout << "Bit Depth:   " << fmt->wBitsPerSample << " bits\n";
    std::cout << "Block Align: " << fmt->nBlockAlign << " bytes\n";
  } else {
    std::cout << "AudioCapturer Initialize failed.\n";
  }

  if (pDefaultDevice)
    pDefaultDevice->Release();
  if (pEnumerator)
    pEnumerator->Release();
  CoUninitialize();
  return 0;
}