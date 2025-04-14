@echo off
REM ========================================================
REM Build script for Multiaudio Project
REM ========================================================
echo Starting build...

REM Compiler and Flags
C:\msys64\mingw64\bin\g++.exe ^
-std=c++17 ^
-DWIN32_LEAN_AND_MEAN ^
-DNOMINMAX ^
-Wall -Wextra -O2 ^
-I. ^
-Ilib ^
-Ilib/imgui ^
-Ilib/imgui/backends ^
-IC:\msys64\mingw64\include ^
-o multiaudio.exe ^
main.cpp ^
audio/BufferQueue.cpp ^
effects/DeEsser.cpp ^
effects/Limiter.cpp ^
effects/NoiseGate.cpp ^
effects/ThreeBandEQ.cpp ^
gui/GUIManager.cpp ^
lib/imgui/imgui.cpp ^
lib/imgui/imgui_draw.cpp ^
lib/imgui/imgui_widgets.cpp ^
lib/imgui/imgui_tables.cpp ^
lib/imgui/backends/imgui_impl_glfw.cpp ^
lib/imgui/backends/imgui_impl_opengl3.cpp ^
-mconsole ^
-LC:\msys64\mingw64\lib ^
-Wl,-rpath,'$ORIGIN/lib' ^
-lglfw3 -lopengl32 -lgdi32 -lrtaudio -lfftw3 -lwinmm -lole32 -pthread

REM Check for errors
if %errorlevel% neq 0 (
    echo Build failed! Errorlevel: %errorlevel%
    pause
    exit /b %errorlevel%
)

echo Build successful: multiaudio.exe created.
REM pause
