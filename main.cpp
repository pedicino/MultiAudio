#include "common.h"
#include "audio/BufferQueue.h"
#include "effects/NoiseGate.h"
#include "effects/ThreeBandEQ.h"
#include "effects/Limiter.h" // <-- 1. Add include for Limiter
#include "effects/DeEsser.h" 

// Global variables used by multiple threads
BufferQueue inputBuffer;      // Queue for raw microphone input data
BufferQueue outputBuffer;     // Queue for processed output data
NoiseGate noiseGate;          // The noise gate effect instance
ThreeBandEQ eq;               // The 3-band EQ effect instance
Limiter limiter;              // <-- 2. Add Limiter instance
atomic<bool> running(true);   // Signal to control thread execution

// Holds configuration for the de-esser effect
struct DeEsserSettings {
    bool enabled = true;
    double reductionDB = 6.0;
    int startFreq = 4000;
    int endFreq = 10000;
} deesserConfig;

/*
RtAudio Callback function
    - Called by audio API for each audio buffer
    - Receive the raw audio from the microphone, pass it to thread, send processed audio back
*/
int audioCallback(
    void *outputBufferCallback, // Renamed to avoid conflict with global 'outputBuffer'
    void *inputBufferCallback,  // Renamed to avoid conflict with global 'inputBuffer'
    unsigned int numFrames,    // Number of frames in the current buffer
    double streamTime,         // Current stream time in seconds
    RtAudioStreamStatus status,// Status flags from RtAudio
    void *userData             // User-provided data pointer
)
{
    // Cast void pointers to actual audio sample type
    float *input = (float*)inputBufferCallback;
    float *output = (float*)outputBufferCallback;

    // Handle stream overflow
    if (status)
    {
        if (running.load()) {
             cerr << "ERROR (non-fatal): Stream overflow :(" << endl;
        }
    }

    // Create a vector from the input samples, add them to the processing queue
    vector<float> inBuffer(input, input + numFrames);
    ::inputBuffer.push(inBuffer); // Use :: scope for global

    // Try to get processed output data
    vector<float> outBuffer;
    bool success = ::outputBuffer.pop(outBuffer); // Use :: scope for global

    // If we managed to get the data and its the right size
    if (success && outBuffer.size() == numFrames)
    {
        // Copy the processed data to the output buffer (audible)
        // Use std::copy for potential efficiency
        std::copy(outBuffer.begin(), outBuffer.end(), output);
    }
    else
    {
        // If we couldn't get data or wrong size, fill with silence
        // Check if we are shutting down
         if (!success && !running.load()) {
             // Normal shutdown, fill with silence
              std::fill_n(output, numFrames, 0.0f);
         } else if (success && outBuffer.size() != numFrames) {
              // Size mismatch - unexpected, log and silence
              if (running.load()){ // Avoid logging during shutdown race condition
                  cerr << "ERROR: Popped buffer size mismatch! Expected " << numFrames << ", got " << outBuffer.size() << endl;
              }
              std::fill_n(output, numFrames, 0.0f);
         } else {
            // Pop failed while running - potential underrun, fill silence
             std::fill_n(output, numFrames, 0.0f);
             if (running.load()) {
                // maybe log underrun if it happens frequently?
             }
         }
    }

    return 0; // Success :)
}


/*
 Audio processing thread function
    - Runs in a separate thread from the audio I/O
    - Takes audio data from the input queue
    - Processes it through the effects chain
    - Places results in output queue
*/
void processingThread()
{
    // Buffers needed for the processing chain
    // Chain: Input -> NoiseGate -> EQ -> (optional) DeEsser -> Limiter -> Output
    vector<float> inputData;                          // Holds incoming audio from queue
    vector<float> noiseGateOutput(FRAMES_PER_BUFFER);   // Holds audio after NoiseGate
    vector<float> eqOutput(FRAMES_PER_BUFFER);          // Holds audio after EQ
    vector<float> deessedData(FRAMES_PER_BUFFER);       // Holds de-essed audio (if enabled)
    vector<float> limiterOutput(FRAMES_PER_BUFFER);     // Holds final processed audio (Limiter output)

    // Process audio until told to stop
    while (running.load())
    {
        // Try to get input data from the queue
        if (!inputBuffer.pop(inputData))
        {
            if (running.load()) {
                std::cerr << "Warning: inputBuffer.pop failed while running." << std::endl;
            }
            break;  // Exit thread loop on pop failure (likely shutdown)
        }

        // Ensure intermediate buffers match the size of the current input block
        size_t currentBufferSize = inputData.size();
        if (noiseGateOutput.size() != currentBufferSize) noiseGateOutput.resize(currentBufferSize);
        if (eqOutput.size() != currentBufferSize) eqOutput.resize(currentBufferSize);
        if (deessedData.size() != currentBufferSize) deessedData.resize(currentBufferSize);
        if (limiterOutput.size() != currentBufferSize) limiterOutput.resize(currentBufferSize);

        // --- Processing Chain ---
        // 1. Noise Gate
        noiseGate.process(inputData.data(), noiseGateOutput.data(), currentBufferSize);

        // 2. EQ
        eq.process(noiseGateOutput.data(), eqOutput.data(), currentBufferSize);

        // 3. Optional De-Esser (apply it if enabled)
        if (deesserConfig.enabled) {
            // Copy EQ output into a temporary buffer of doubles
            vector<double> temp(eqOutput.begin(), eqOutput.end());
            // Call the de-esser function (assumed to modify temp in-place)
            applyDeEsser(temp, SAMPLE_RATE, deesserConfig.startFreq,
                         deesserConfig.endFreq, deesserConfig.reductionDB);
            // Convert back to float for further processing
            transform(temp.begin(), temp.end(), deessedData.begin(),
                      [](double x) { return static_cast<float>(x); });
        } else {
            // If not enabled, simply pass the EQ output
            deessedData = eqOutput;
        }

        // 4. Limiter: apply the limiter to the de-essed audio
        limiter.process(deessedData.data(), limiterOutput.data(), currentBufferSize);

        // 5. Push the single, final output to the output queue
        outputBuffer.push(limiterOutput);
    }
}


// Console UI management - NO CHANGES HERE
void clearConsoleLine() {
    cout << "\33[2K\r";
}

void moveCursorUp(int lines) {
    cout << "\033[" << lines << "A";
}

void printUI() {
    // Move up to the start of UI
    moveCursorUp(11);
    
    clearConsoleLine(); cout << "Noise Gate: " << (noiseGate.isEnabled() ? "Enabled " : "Disabled") << endl;
    clearConsoleLine(); cout << "EQ:         " << (eq.isEnabled() ? "Enabled " : "Disabled") << endl;
    clearConsoleLine(); std::cout << "Limiter:    " << (limiter.isEnabled() ? "Enabled " : "Disabled") << " (T:" << limiter.getThreshold() << ")" << std::endl;
    clearConsoleLine(); cout << "Low band:   " << eq.getBandGain(0) << endl;
    clearConsoleLine(); cout << "Mid band:   " << eq.getBandGain(1) << endl;
    clearConsoleLine(); cout << "High band:  " << eq.getBandGain(2) << endl;
    clearConsoleLine(); cout << "De-Esser:   " << (deesserConfig.enabled ? "Enabled " : "Disabled") << endl;
    clearConsoleLine(); cout << "Sib Freqs:  " << deesserConfig.startFreq << " Hz - " << deesserConfig.endFreq << " Hz" << endl;
    clearConsoleLine(); cout << "Reduction:  " << deesserConfig.reductionDB << " dB" << endl;

    clearConsoleLine(); cout << "Controls - e: Toggle Noise Gate, g: Toggle EQ, q: Quit" << endl; // NO CHANGE
    clearConsoleLine(); cout << "          1/z: Low+-, 2/x: Mid+-, 3/c: High+-" << endl;
    clearConsoleLine(); cout << "           d: Toggle De-Esser, +/-: Adjust Reduction" << endl;
    clearConsoleLine(); cout << "Input: ";
}

/*
 Good ol' main
 - Set up audio system
 - Spawn threads
*/
int main()
{
    try
    {
        // Create an RtAudio instance for our I/O
        #ifdef _WIN32
            RtAudio audio(RtAudio::Api::WINDOWS_WASAPI); // Use WASAPI on Windows
        #else
            RtAudio audio; // Use default API on other platforms
        #endif

        // Verify audio devices are available
        if (audio.getDeviceCount() < 1)
        {
            cerr << "How do you not have audio devices..." << endl;
            return 1;
        }

        // Configure audio input parameters
        RtAudio::StreamParameters inputParams;
        inputParams.deviceId = audio.getDefaultInputDevice();
        inputParams.nChannels = NUM_CHANNELS;
        inputParams.firstChannel = 0;

        // Configure audio output parameters
        RtAudio::StreamParameters outputParams;
        outputParams.deviceId = audio.getDefaultOutputDevice();
        outputParams.nChannels = NUM_CHANNELS;
        outputParams.firstChannel = 0;

        // Set up audio buffer size
        unsigned int bufferFrames = FRAMES_PER_BUFFER;

        // Open the audio stream with the callback function
        // Use nullptr for userData if not needed
        audio.openStream(
            &outputParams,       // Output configuration
            &inputParams,        // Input configuration
            RTAUDIO_FLOAT32,     // Sample format (32-bit float)
            SAMPLE_RATE,         // Sample rate
            &bufferFrames,       // Buffer size
            &audioCallback,      // Callback function
            nullptr              // Pass nullptr for userData
        );

        // Create and start the audio processing thread
        thread procThread(::processingThread);

        // Start the audio stream (call the callback)
        audio.startStream();
        
        cout << string(11, '\n');
        printUI();

        while (running.load()) {
            char key;
            cin >> key;
            switch (key) {
                case 'q':
                    running.store(false);
                    break;
                case '1':
                    eq.setBandGain(0, eq.getBandGain(0) + 0.1f);
                    break;
                case 'z':
                    eq.setBandGain(0, eq.getBandGain(0) - 0.1f);
                    break;
                case '2':
                    eq.setBandGain(1, eq.getBandGain(1) + 0.1f);
                    break;
                case 'x':
                    eq.setBandGain(1, eq.getBandGain(1) - 0.1f);
                    break;
                case '3':
                    eq.setBandGain(2, eq.getBandGain(2) + 0.1f);
                    break;
                case 'c':
                    eq.setBandGain(2, eq.getBandGain(2) - 0.1f);
                    break;
                case 'e':
                    noiseGate.setEnabled(!noiseGate.isEnabled());
                    break;
                case 'g':
                    eq.setEnabled(!eq.isEnabled());
                    break;
                case 'l':
                    limiter.setEnabled(!limiter.isEnabled());
                    break;
                case 'd':
                    deesserConfig.enabled = !deesserConfig.enabled;
                    break;
                case '+': 
                    deesserConfig.reductionDB += 1.0;
                    break;
                case '-': 
                    deesserConfig.reductionDB = std::max(0.0, deesserConfig.reductionDB - 1.0); 
                    break;
            }

             if (running.load()) {
                 printUI();
             }
        }

        // Begin clean shutdown
        cout << "\nShutting down..." << endl; // Add newline for clarity

        // Stop and close the audio stream
        // Check if stream is open before stopping/closing
        if (audio.isStreamOpen()) {
            if (audio.isStreamRunning()) {
                audio.stopStream();
            }
            audio.closeStream();
        }

        // Signal buffer queues to unblock waiting threads
        inputBuffer.setDone();
        outputBuffer.setDone();

        // Wait for processing thread to finish
        // Check if thread is joinable before joining
        if(procThread.joinable()) {
             procThread.join();
        }


        cout << "Shutdown complete." << endl;
    }
    catch (...)
    {
        cerr << "ERROR: An unexpected error occurred." << endl; // Simplified catch-all
        return 1;
    }

    return 0;
}