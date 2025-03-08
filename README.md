# multiaudio
Multithreaded Audio Processor in C++

## Program Usage
For now, a rudimentary noise gate works.
Once the program is running, speak into your microphone to test it!

## Clone Repository
```bash
git clone https://github.com/pedicino/multiaudio.git
cd multiaudio
```

## Prerequisites
### Compiler Requirements
- A modern C++ compiler supporting C++11 or later
  - Linux: GCC 7+ or Clang
  - macOS: Clang (Xcode Command Line Tools)
  - Windows: MinGW-w64 or Visual Studio
- Required Libraries:
  - RtAudio
  - FFTW3
  - Platform-specific audio libraries

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

### macOS (using Homebrew)
```bash
# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install fftw rtaudio
```

### Windows (using MSYS2)
1. Install MSYS2 from https://www.msys2.org/
2. Open MSYS2 MinGW 64-bit terminal
```bash
# Update package database
pacman -Syu

# Install dependencies
pacman -S mingw-w64-x86_64-toolchain \
          mingw-w64-x86_64-fftw \
          mingw-w64-x86_64-rtaudio \
          mingw-w64-x86_64-cmake
```

## Compilation

### Linux
```bash
g++ -o noise_gate NoiseGate.cpp -lrtaudio -lfftw3 -lpthread -lasound -ljack
```

### macOS
```bash
g++ -o noise_gate NoiseGate.cpp -lrtaudio -lfftw3 -framework CoreAudio -framework CoreFoundation
```

### Windows (MSYS2 MinGW)
```bash
g++ -o noise_gate.exe NoiseGate.cpp -lrtaudio -lfftw3 -lole32 -lwinmm
```

## Run

### Linux and macOS
```bash
./noise_gate
```

### Windows
```bash
noise_gate.exe
```
(Press ENTER to terminate the program)

### Configuration
You can modify noise gate parameters in the code directly:
- Adjust `thresh` in the `NoiseGate` constructor to control noise reduction sensitivity
- Modify `SAMPLE_RATE`, `FRAMES_PER_BUFFER`, and `NUM_BANDS` as needed
