#include "BufferTransfer.h"
#include "RingBuffer.h"
#include <iostream>
#include <vector>

int main() {
  // 1. Create a tiny 10-byte RingBuffer so we force wrap-around math quickly
  RingBuffer rb(10);

  // Push 8 bytes: [1, 2, 3, 4, 5, 6, 7, 8]
  BYTE firstBatch[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  rb.Write(firstBatch, 8);
  std::cout << "After writing 8 bytes -> AvailableToRead: "
            << rb.AvailableToRead() << ", Free: " << rb.AvailableToWrite()
            << "\n";

  // Read 6 bytes out -> readPos moves to index 6 (leaving [7, 8] inside)
  BYTE out1[6] = {0};
  rb.Read(out1, 6);
  std::cout << "Read 6 bytes -> Remaining in buffer: " << rb.AvailableToRead()
            << "\n";

  // 2. Now write 5 bytes: [9, 10, 11, 12, 13]
  // Slots 8 and 9 take [9, 10], then it wraps around to slots 0, 1, 2 for [11,
  // 12, 13]!
  BYTE secondBatch[5] = {9, 10, 11, 12, 13};
  size_t written = rb.Write(secondBatch, 5);
  std::cout << "Wrapped write of 5 bytes -> Actually written: " << written
            << ", AvailableToRead: " << rb.AvailableToRead() << "\n";

  // 3. Test BufferTransfer with underrun padding:
  // Buffer has 7 bytes ([7, 8, 9, 10, 11, 12, 13]), but we ask for 10 bytes.
  // The last 3 bytes in 'dest' should automatically become 0 (silence).
  BYTE dest[10] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99};
  size_t realAudioBytes = TransferToDest(rb, dest, 10);

  std::cout << "\nTransferToDest pulled " << realAudioBytes
            << " real bytes. Contents of dest:\n";
  for (int i = 0; i < 10; ++i) {
    std::cout << (int)dest[i] << " ";
  }
  std::cout << "\n";

  return 0;
}