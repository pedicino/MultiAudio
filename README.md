# multiaudio
Multithreaded Audio Processor in C++

# Multi-Effect Audio Processor

## Clone Repository
```bash
git clone https://github.com/pedicino/multiaudio.git
cd multiaudio
```

## Prerequisites
### Compiler Requirements
- A modern C++ compiler supporting C++11 or later
  - Windows: Visual Studio 2017 or later, or MinGW-w64
  - macOS: Xcode Command Line Tools or Clang
  - Linux: GCC 7+ or Clang

## Dependency Installation
### Windows
#### Using vcpkg
1. Install Chocolatey (Package Manager)
   ```powershell
   Set-ExecutionPolicy Bypass -Scope Process -Force
   [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
   iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
   ```
2. Install Git and CMake
   ```powershell
   choco install git cmake
   ```
3. Install vcpkg
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
4. Install dependencies
   ```powershell
   .\vcpkg install fftw3 rtaudio --triplet x64-windows
   .\vcpkg integrate install
   ```

### macOS
#### Homebrew
1. Install Homebrew
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```
2. Install dependencies
   ```bash
   brew install cmake fftw rtaudio
   ```

### Linux (Ubuntu/Debian)
#### Using Package Manager
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libfftw3-dev \
    librtaudio-dev \
    libasound2-dev \
    jack \
    libjack-dev
```

## Build
### Windows
```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### macOS and Linux
```bash
mkdir build
cd build
cmake ..
make
```

## Run
To run the executable,
```bash
./NoiseGate
```

### Configuration
You can modify noise gate parameters in the code directly:
- Adjust `thresh` in the `NoiseGate` constructor to control noise reduction sensitivity
- Modify `SAMPLE_RATE`, `FRAMES_PER_BUFFER`, and `NUM_BANDS` as needed
