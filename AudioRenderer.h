#pragma once
#include "RingBuffer.h"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>


// Streams audio from a RingBuffer out to a specific Windows playback device
// (e.g., Bluetooth headphones)
class AudioRenderer {
private:
  IAudioClient *pAudioClient =
      nullptr; // Master stream controller for the output device
  IAudioRenderClient *pRenderClient =
      nullptr; // Interface that gives us the 'dest' hardware buffer pointer
  WAVEFORMATEX *pwfx =
      nullptr; // Device audio format (sample rate, channels, bytes per frame)
  UINT32 bufferFrameCount =
      0; // Total capacity of Windows' playback buffer in frames

public:
  ~AudioRenderer();

  // Connects to the target playback device and sets up its hardware audio
  // buffer
  bool Initialize(IMMDevice *pDevice);

  // Starts the playback stream
  bool Start();

  // Stops the playback stream
  void Stop();

  // Grabs empty space in Windows' playback buffer ('dest') and fills it using
  // TransferToDest()
  bool RenderFromBuffer(RingBuffer &ringBuffer);
};