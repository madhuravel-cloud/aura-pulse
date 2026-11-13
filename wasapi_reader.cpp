#include "wasapi.h"

bool AudioCapturer::ReadPackets(
    const std::function<void(const BYTE *, size_t)> &onAudioData) {
  if (!pCaptureClient || !pwfx)
    return false;

  UINT32 packetLength = 0;
  HRESULT hr = pCaptureClient->GetNextPacketSize(&packetLength);
  if (FAILED(hr))
    return false;

  while (packetLength > 0) {
    BYTE *pData = nullptr;
    UINT32 numFramesAvailable = 0;
    DWORD flags = 0;

    // 1. Grab the temporary pointer to Windows' internal buffer
    hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL,
                                   NULL);
    if (FAILED(hr))
      return false;

    if (numFramesAvailable > 0) {
      size_t byteSize =
          static_cast<size_t>(numFramesAvailable) * pwfx->nBlockAlign;

      // 2. Check if Windows marked this packet as pure silence
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        onAudioData(nullptr, byteSize);
      } else {
        onAudioData(pData, byteSize);
      }
    }

    // 3. Hand the slot back to Windows immediately before pData becomes invalid
    hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
    if (FAILED(hr))
      return false;

    // 4. Check if another packet queued up while we were copying
    hr = pCaptureClient->GetNextPacketSize(&packetLength);
    if (FAILED(hr))
      return false;
  }

  return true;
}