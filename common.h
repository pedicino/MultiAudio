#ifndef COMMON_H
#define COMMON_H

// Include standard libraries needed by multiple components
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <algorithm>
#include <fftw3.h>
#include <rtaudio/RtAudio.h>

using namespace std;

// Global constants
const unsigned int SAMPLE_RATE = 48000;
const unsigned int FRAMES_PER_BUFFER = 1024;
const unsigned int FFT_SIZE = 1024;
const unsigned int NUM_CHANNELS = 2;
const unsigned int NUM_BANDS = 4;
const unsigned int NUM_EQ_BANDS = 3;

#endif // COMMON_H