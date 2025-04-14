#include "common.h"
#include "audio/BufferQueue.h"
#include "effects/NoiseGate.h"
#include "effects/ThreeBandEQ.h"
#include "effects/Limiter.h"
#include "effects/DeEsser.h"
#include "gui/GUIManager.h"
#include <iostream> // Needed for std::cout, std::cerr
#include <rtaudio/RtAudio.h> // Keep this explicit include
#include <vector>   // Ensure vector is included (likely via common.h)
#include <algorithm> // For std::copy, std::fill_n, std::transform
#include <cmath>     // For std::isnan, std::isinf (optional checks)
#include <limits>    // For numeric_limits (optional checks)

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

// Safe buffer resize function with padding to prevent memory issues
void safeResize(vector<float> &buffer, size_t newSize)
{
    if (newSize > 1024 * 1024 * 16) { // Example limit: 16M floats
         std::cerr << "Warning: Attempting large resize: " << newSize << std::endl;
    }
    try {
        buffer.resize(newSize + 32); // Add padding for safety
    } catch (const std::bad_alloc& e) {
        std::cerr << "ERROR: Failed to resize buffer to " << newSize + 32 << " floats. " << e.what() << std::endl;
        // Handle allocation failure, maybe throw or exit
    }
}

// --- Global Variables ---
audio::BufferQueue inputBuffer;
audio::BufferQueue outputBuffer;
audio::NoiseGate noiseGate;
audio::ThreeBandEQ eq;
audio::Limiter limiter;
atomic<bool> running(true);
struct DeEsserSettings {
    bool enabled = false; double reductionDB = 6.0; int startFreq = 4000; int endFreq = 10000;
} deesserConfig;
// --- End Global Variables ---

int audioCallback(void *outputBufferCallback, void *inputBufferCallback, unsigned int nFrames,
                  double streamTime, RtAudioStreamStatus status, void *userData)
{
    static vector<float> fixedInBuffer(FRAMES_PER_BUFFER * NUM_CHANNELS + 64); // Uses NUM_CHANNELS from common.h

    float *input = static_cast<float *>(inputBufferCallback);
    float *output = static_cast<float *>(outputBufferCallback);
    size_t samplesAvailable = nFrames * NUM_CHANNELS; // Total samples for all channels

    if (status) { std::cerr << "Warning: Audio stream status: " << status << std::endl; }
    if (samplesAvailable > fixedInBuffer.size()) {
        std::cerr << "ERROR: nFrames (" << nFrames << ") * NUM_CHANNELS (" << NUM_CHANNELS
                  << ") exceeds fixedInBuffer size in audioCallback!" << std::endl;
        std::fill_n(output, samplesAvailable, 0.0f); return 1;
    }

    // Copy data from RtAudio input buffer
    std::copy(input, input + samplesAvailable, fixedInBuffer.begin());
    // Push exact amount to processing thread
    vector<float> currentInput(fixedInBuffer.begin(), fixedInBuffer.begin() + samplesAvailable);
    ::inputBuffer.push(currentInput);

    // Attempt to get processed data
    vector<float> currentOutput; // Let pop resize this
    bool pop_success = ::outputBuffer.pop(currentOutput); // <<<--- Check success

    if (pop_success) {
        // --- Debug Print (Success Case) ---
        // Uncomment the next line to verify pops are succeeding (can be verbose)
        // std::cout << "DEBUG: audioCallback pop SUCCESS (size: " << currentOutput.size() << ")" << std::endl;

        if (currentOutput.size() == samplesAvailable) {
            std::copy(currentOutput.begin(), currentOutput.end(), output);
        } else {
            // Size mismatch is an error condition
            std::cerr << "ERROR: Popped output buffer size mismatch in audioCallback! Expected "
                      << samplesAvailable << ", got " << currentOutput.size() << ". Outputting silence." << std::endl;
            std::fill_n(output, samplesAvailable, 0.0f);
        }
    } else {
        // --- Debug Print (Failure Case) ---
        // Pop failed - this means the processing thread isn't providing data in time (underrun)
        // This is expected during shutdown but indicates a problem if it happens constantly while running.
         std::cout << "DEBUG: audioCallback pop FAILED (Output Underrun)" << std::endl; // <<<--- Print if pop fails

        if (running.load()) { /* Log persistent underruns if needed */ }
        std::fill_n(output, samplesAvailable, 0.0f); // Output silence
    }
    return 0;
}

void processingThread()
{
    std::cout << "[Processing Thread] Started." << std::endl;
#ifdef _WIN32
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
         std::cerr << "[Processing Thread] Warning: Failed to set thread priority. Error code: " << GetLastError() << std::endl;
    } else { std::cout << "[Processing Thread] Priority set to TIME_CRITICAL." << std::endl; }
#elif defined(__APPLE__) || defined(__linux__)
    struct sched_param param; param.sched_priority = sched_get_priority_max(SCHED_RR);
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param) != 0) {
        std::cerr << "[Processing Thread] Warning: Failed to set real-time thread priority (requires permissions?)." << std::endl;
    } else { std::cout << "[Processing Thread] Priority set to SCHED_RR max." << std::endl; }
#endif

    // Adjust buffer sizes based on actual NUM_CHANNELS
    const size_t MAX_EXPECTED_FRAMES = FRAMES_PER_BUFFER * 2; // Max frames expected
    const size_t PADDED_BUFFER_FRAMES = MAX_EXPECTED_FRAMES + 64; // Frame padding

    vector<float> inputData; // Pop resizes this
    vector<float> monoChannel(PADDED_BUFFER_FRAMES); // Buffer for mono processing
    vector<float> gateOutput(PADDED_BUFFER_FRAMES);
    vector<float> eqOutput(PADDED_BUFFER_FRAMES);
    vector<float> deessedData(PADDED_BUFFER_FRAMES);
    vector<float> limiterOutput(PADDED_BUFFER_FRAMES); // Final mono processed stage
    vector<float> outputData; // Final stereo (or multi-channel) output
    vector<double> tempDeEsser(PADDED_BUFFER_FRAMES);

    std::cout << "[Processing Thread] Entering main loop." << std::endl;
    while (running.load()) {
        if (!inputBuffer.pop(inputData)) {
            if (running.load()) { std::cerr << "[Processing Thread] Warning: inputBuffer.pop failed while running." << std::endl; }
            else { std::cout << "[Processing Thread] Input buffer done, exiting loop." << std::endl; }
            break;
        }
        size_t samplesReceived = inputData.size();
        if (samplesReceived == 0) { std::cerr << "[Processing Thread] Warning: Received empty input buffer." << std::endl; continue; }
        if (samplesReceived % NUM_CHANNELS != 0) {
             std::cerr << "[Processing Thread] ERROR: Received buffer size (" << samplesReceived << ") not divisible by NUM_CHANNELS (" << NUM_CHANNELS << ")!" << std::endl;
             continue;
        }
        size_t numFrames = samplesReceived / NUM_CHANNELS; // Number of frames per channel

        // Ensure intermediate buffers are large enough for MONO processing
        if (monoChannel.size() < numFrames) monoChannel.resize(numFrames);
        if (gateOutput.size() < numFrames) gateOutput.resize(numFrames);
        if (eqOutput.size() < numFrames) eqOutput.resize(numFrames);
        if (deessedData.size() < numFrames) deessedData.resize(numFrames);
        if (limiterOutput.size() < numFrames) limiterOutput.resize(numFrames);
        if (tempDeEsser.size() < numFrames) tempDeEsser.resize(numFrames);

        // Extract first channel for mono processing
        // Assumes interleaved inputData: [L1, R1, L2, R2, ...]
        for (size_t i = 0; i < numFrames; ++i) {
             monoChannel[i] = inputData[i * NUM_CHANNELS]; // Take first channel sample
        }

        // --- Effects Chain (on mono data) ---
        noiseGate.process(monoChannel.data(), gateOutput.data(), numFrames);
        eq.process(gateOutput.data(), eqOutput.data(), numFrames);

        const float* deesserInputPtr = eqOutput.data();
        if (deesserConfig.enabled) {
            std::copy(eqOutput.begin(), eqOutput.begin() + numFrames, tempDeEsser.begin());
            audio::applyDeEsser(tempDeEsser, SAMPLE_RATE, deesserConfig.startFreq, deesserConfig.endFreq, deesserConfig.reductionDB);
            std::transform(tempDeEsser.begin(), tempDeEsser.begin() + numFrames, deessedData.begin(), [](double x){ return static_cast<float>(x); });
            deesserInputPtr = deessedData.data();
        }
        limiter.process(deesserInputPtr, limiterOutput.data(), numFrames); // limiterOutput is mono

        // --- Prepare Output Buffer ---
        size_t outputSamples = numFrames * NUM_CHANNELS; // Total samples for output
        outputData.resize(outputSamples);

        // Duplicate processed mono data to all output channels (dual mono if NUM_CHANNELS=2)
        for (size_t i = 0; i < numFrames; ++i) {
            for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
                 outputData[i * NUM_CHANNELS + ch] = limiterOutput[i];
            }
        }

        // --- Check output data before pushing --- <<<--- ADDED CHECK
        float minVal = 0.0f, maxVal = 0.0f;
        bool allZero = true;
        bool hasNanInf = false;
        if (!outputData.empty()) {
            minVal = outputData[0];
            maxVal = outputData[0];
            for(float val : outputData) {
                if (std::isnan(val) || std::isinf(val)) {
                    hasNanInf = true;
                    allZero = false; // Treat NaN/Inf as non-zero for this check
                    break; // Stop checking once NaN/Inf is found
                }
                if (val != 0.0f) allZero = false;
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }
        }
        // // Print status of the buffer being pushed
        // std::cout << "[Processing Thread] Pre-push check: size=" << outputData.size()
        //           << ", allZero=" << (allZero ? "true" : "false")
        //           << ", hasNaN/Inf=" << (hasNanInf ? "true" : "false")
        //           << ", min=" << minVal << ", max=" << maxVal << std::endl;
        // // --- End check ---

        // Push the final data to the output queue
        outputBuffer.push(outputData);
    }
    std::cout << "[Processing Thread] Exited main loop." << std::endl;
}

int main()
{
    std::cout << "DEBUG: main() started." << std::endl;
    try {
        std::cout << "DEBUG: Creating RtAudio object..." << std::endl;
#ifdef _WIN32
        RtAudio audio(RtAudio::Api::WINDOWS_WASAPI);
#else
        RtAudio audio;
#endif
        std::cout << "DEBUG: RtAudio object created." << std::endl;

        std::cout << "DEBUG: Checking audio device count..." << std::endl;
        if (audio.getDeviceCount() < 1) { cerr << "ERROR: No audio devices detected" << endl; return 1; }
        std::cout << "DEBUG: Audio device count checked (" << audio.getDeviceCount() << ")." << std::endl;

        RtAudio::StreamParameters inputParams;
        inputParams.deviceId = audio.getDefaultInputDevice(); inputParams.nChannels = NUM_CHANNELS; inputParams.firstChannel = 0;
        std::cout << "DEBUG: Input parameters set (Device: " << inputParams.deviceId << ", Channels: " << inputParams.nChannels << ")." << std::endl;

        RtAudio::StreamParameters outputParams;
        outputParams.deviceId = audio.getDefaultOutputDevice(); outputParams.nChannels = NUM_CHANNELS; outputParams.firstChannel = 0;
        std::cout << "DEBUG: Output parameters set (Device: " << outputParams.deviceId << ", Channels: " << outputParams.nChannels << ")." << std::endl;

        unsigned int bufferFrames = FRAMES_PER_BUFFER;
        std::cout << "DEBUG: Buffer frames variable set to " << bufferFrames << "." << std::endl;

        std::cout << "DEBUG: Opening audio stream..." << std::endl;
        RtAudioErrorType openResult = audio.openStream(
            &outputParams, &inputParams, RTAUDIO_FLOAT32, SAMPLE_RATE, &bufferFrames, &audioCallback, nullptr);
        if (openResult != RTAUDIO_NO_ERROR) {
             std::cerr << "ERROR: Failed to open RtAudio stream: " << audio.getErrorText() << std::endl;
             return 1;
        }
        std::cout << "DEBUG: Audio stream opened (bufferFrames possibly adjusted to: " << bufferFrames << ")." << std::endl;

        std::cout << "DEBUG: Starting processing thread..." << std::endl;
        thread procThread(::processingThread);
        std::cout << "DEBUG: Processing thread object created." << std::endl;

        std::cout << "DEBUG: Starting audio stream..." << std::endl;
        RtAudioErrorType startResult = audio.startStream();
         if (startResult != RTAUDIO_NO_ERROR) {
             std::cerr << "ERROR: Failed to start RtAudio stream: " << audio.getErrorText() << std::endl;
             running.store(false); if (audio.isStreamOpen()) audio.closeStream();
             inputBuffer.setDone(); outputBuffer.setDone(); if (procThread.joinable()) procThread.join();
             return 1;
         }
        std::cout << "DEBUG: Audio stream started." << std::endl;

        std::cout << "DEBUG: Initializing GUIManager..." << std::endl;
        gui::GUIManager guiManager(noiseGate, eq, limiter, deesserConfig.enabled, deesserConfig.reductionDB, deesserConfig.startFreq, deesserConfig.endFreq);
        std::cout << "DEBUG: GUIManager object created." << std::endl;

        std::cout << "DEBUG: Calling guiManager.initialize()..." << std::endl;
        if (!guiManager.initialize()) {
            cerr << "ERROR: Failed to initialize GUI" << endl;
            running.store(false); if (audio.isStreamOpen()) { if (audio.isStreamRunning()) audio.stopStream(); audio.closeStream(); }
            inputBuffer.setDone(); outputBuffer.setDone(); if (procThread.joinable()) procThread.join();
            return 1;
        }
        std::cout << "DEBUG: guiManager.initialize() successful." << std::endl;

        std::cout << "DEBUG: Entering main GUI loop..." << std::endl;
        while (running.load() && guiManager.isRunning()) {
            guiManager.update();
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::cout << "DEBUG: Exited main GUI loop." << std::endl;

        std::cout << "DEBUG: Initiating shutdown..." << std::endl;
        running.store(false);

        std::cout << "DEBUG: Stopping/closing audio stream..." << std::endl;
        if (audio.isStreamOpen()) {
            if (audio.isStreamRunning()) { audio.stopStream(); std::cout << "DEBUG: Audio stream stopped." << std::endl; }
            audio.closeStream(); std::cout << "DEBUG: Audio stream closed." << std::endl;
        } else { std::cout << "DEBUG: Audio stream was not open." << std::endl; }

        std::cout << "DEBUG: Signaling buffer queues done..." << std::endl;
        inputBuffer.setDone(); outputBuffer.setDone();

        std::cout << "DEBUG: Joining processing thread..." << std::endl;
        if (procThread.joinable()) { procThread.join(); std::cout << "DEBUG: Processing thread joined." << std::endl;
        } else { std::cout << "DEBUG: Processing thread was not joinable." << std::endl; }

        std::cout << "DEBUG: GUI cleanup (implicit via destructor)..." << std::endl;
        std::cout << "DEBUG: Shutdown sequence complete." << std::endl;

    }
    catch (const std::exception& e) {
         std::cerr << "ERROR: Standard exception caught in main: " << e.what() << std::endl;
         return 1;
    }
    catch (...) {
        cerr << "ERROR: Unknown exception occurred in main" << endl;
        return 1;
    }

    std::cout << "DEBUG: main() finished successfully." << std::endl;
    return 0;
}