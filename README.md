# multiaudio
Multithreaded Audio Processor in C++

## Program Usage
For now, a rudimentary noise gate and 3 band EQ works.
Once the program is running, speak into your microphone to test it!

Type a command and press Enter to apply it:

1. Toggle Noise Gate: "e"
2. Toggle 3 Band EQ: "g"
3. Increase gain for EQ bands: "1" (Low), "2" (Mid), and "3" (High)
4. Decrease gain for EQ bands: "z" (Low), "x" (Mid), and "c" (High)
5. Exit the program with "q"

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
g++ -o multiaudio.exe \
    main.cpp \
    audio/*.cpp \
    effects/*.cpp \
    -lrtaudio -lfftw3 -lpthread -lasound -ljack
```

### macOS
```bash
g++ -std=c++11 -o multiaudio \
    main.cpp \
    audio/*.cpp \
    effects/*.cpp \
    -fftw -rtaudio -lfftw3 -framework CoreAudio -framework CoreFoundation
```

### Windows (MSYS2 MinGW)

Note that in the MinGW terminal, to navigate to your project directory, ``` cd .. ``` twice to navigate to the root directory, then ``` cd c ``` to go to your C: folder.

```bash
g++ -I. -o multiaudio.exe \
    main.cpp \
    audio/*.cpp \
    effects/*.cpp \
    -lrtaudio -lfftw3 -lole32 -lwinmm
```

## Run

### Linux and macOS
```bash
./multiaudio
```

### Windows
```bash
./multiaudio.exe
```

### Configuration
You can modify noise gate parameters in the code directly:
- Adjust `thresh` in the `NoiseGate` constructor to control noise reduction sensitivity
- Modify `SAMPLE_RATE`, `FRAMES_PER_BUFFER`, and `NUM_BANDS` as needed
- Edit the `EQBAND_START` and `EQBAND_FACTOR` to modify the ranges for the low/mid/high bands on the EQ
