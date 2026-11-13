#pragma once
#include "RingBuffer.h"
#include <windows.h>


// Transfers a fixed chunk of audio from RingBuffer into the destination
// playback buffer (dest). Pads any missing bytes with silence (0) if the
// RingBuffer runs dry.
size_t TransferToDest(RingBuffer &ringBuffer, BYTE *dest, size_t bytesNeeded);