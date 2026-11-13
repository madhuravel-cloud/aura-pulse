#pragma once
#include <atomic>
#include <vector>
#include <windows.h>


// Lock-free Single-Producer, Single-Consumer (SPSC) circular audio buffer
class RingBuffer {
private:
  std::vector<BYTE> buffer;
  size_t capacity;
  std::atomic<size_t> writePos{0}; // Running count of total bytes written
  std::atomic<size_t> readPos{0};  // Running count of total bytes read

public:
  // Allocates the fixed circular buffer upfront
  RingBuffer(size_t sizeInBytes);

  // Returns unread audio bytes waiting in the buffer
  size_t AvailableToRead() const;

  // Returns remaining free space in bytes before the buffer fills up
  size_t AvailableToWrite() const;

  // Pushes incoming audio bytes (or fills zeros if data is nullptr)
  size_t Write(const BYTE *data, size_t bytes);

  // Pulls waiting audio bytes out into the destination buffer
  size_t Read(BYTE *dest, size_t bytes);
};