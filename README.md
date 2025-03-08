# multiaudio
Multithreaded Audio Processor in C++

## Clone Repository
```bash
git clone https://github.com/pedicino/multiaudio.git
cd multiaudio
```

## Prerequisites
### Compiler Requirements
- A modern C++ compiler supporting C++11 or later
  - Linux: GCC 7+ or Clang
- Required Libraries:
  - RtAudio
  - FFTW3
  - ALSA
  - JACK (optional)

## Dependency Installation
### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libfftw3-dev \
    librtaudio-dev \
    libasound2-dev \
    jack \
    libjack-dev
```

## Compilation
```bash
g++ -o noise_gate NoiseGate.cpp -lrtaudio -lfftw3 -lpthread -lasound -ljack
```

## Run
```bash
./noise_gate
```

### Configuration
You can modify noise gate parameters in the code directly:
- Adjust `thresh` in the `NoiseGate` constructor to control noise reduction sensitivity
- Modify `SAMPLE_RATE`, `FRAMES_PER_BUFFER`, and `NUM_BANDS` as needed
