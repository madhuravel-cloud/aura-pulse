#include "RingBuffer.h"
#include <algorithm>
#include <cstring>


// Allocates the underlying byte vector once at startup
RingBuffer::RingBuffer(size_t sizeInBytes)
    : buffer(sizeInBytes), capacity(sizeInBytes) {}

// Total bytes written minus total bytes read = unread bytes sitting in buffer
size_t RingBuffer::AvailableToRead() const {
  return writePos.load() - readPos.load();
}

// Total capacity minus unread bytes = free space safe to overwrite
size_t RingBuffer::AvailableToWrite() const {
  return capacity - AvailableToRead();
}

// Writes audio (or silence) into the circular buffer, splitting across index 0
// if it wraps around
size_t RingBuffer::Write(const BYTE *data, size_t bytes) {
  size_t toWrite = (std::min)(bytes, AvailableToWrite());
  if (toWrite == 0)
    return 0;

  size_t index = writePos.load() % capacity;
  size_t firstChunk = (std::min)(toWrite, capacity - index);
  size_t secondChunk = toWrite - firstChunk;

  if (data != nullptr) {
    std::memcpy(&buffer[index], data, firstChunk);
    if (secondChunk > 0) {
      std::memcpy(&buffer[0], data + firstChunk, secondChunk);
    }
  } else {
    std::memset(&buffer[index], 0, firstChunk);
    if (secondChunk > 0) {
      std::memset(&buffer[0], 0, secondChunk);
    }
  }

  writePos.store(writePos.load() + toWrite);
  return toWrite;
}

// Reads waiting audio out into 'dest', handling wrap-around across index 0 if
// needed
size_t RingBuffer::Read(BYTE *dest, size_t bytes) {
  if (!dest)
    return 0;

  size_t toRead = (std::min)(bytes, AvailableToRead());
  if (toRead == 0)
    return 0;

  size_t index = readPos.load() % capacity;
  size_t firstChunk = (std::min)(toRead, capacity - index);
  size_t secondChunk = toRead - firstChunk;

  std::memcpy(dest, &buffer[index], firstChunk);
  if (secondChunk > 0) {
    std::memcpy(dest + firstChunk, &buffer[0], secondChunk);
  }

  readPos.store(readPos.load() + toRead);
  return toRead;
}