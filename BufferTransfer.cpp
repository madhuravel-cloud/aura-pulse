#include "BufferTransfer.h"
#include <cstring>

// Pulls 'bytesNeeded' out of ringBuffer into dest, filling the remainder with
// 0s on underrun
size_t TransferToDest(RingBuffer &ringBuffer, BYTE *dest, size_t bytesNeeded) {
  if (!dest || bytesNeeded == 0)
    return 0;

  // 1. Copy whatever audio bytes are currently ready from RingBuffer into dest
  size_t bytesRead = ringBuffer.Read(dest, bytesNeeded);

  // 2. If RingBuffer ran dry before filling dest, zero out the rest so there's
  // no static
  if (bytesRead < bytesNeeded) {
    size_t missingBytes = bytesNeeded - bytesRead;
    std::memset(dest + bytesRead, 0, missingBytes);
  }

  return bytesRead;
}