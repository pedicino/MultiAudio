#include "common.h"
#include "audio/BufferQueue.h"
#include "effects/NoiseGate.h"
#include "effects/ThreeBandEQ.h"

// Global variables used by multiple threads
BufferQueue inputBuffer;      // Queue for raw microphone input data
BufferQueue outputBuffer;     // Queue for processed output data
NoiseGate noiseGate;          // The noise gate effect instance
ThreeBandEQ eq;               // The 3-band EQ effect instance
atomic<bool> running(true);   // Signal to control thread execution

/*
RtAudio Callback function
    - Called by audio API for each audio buffer
    - Receive the raw audio from the microphone, pass it to thread, send processed audio back
*/
int audioCallback(
    void *outputBuffer,        // Where output samples should be written
    void *inputBuffer,         // Where microphone input comes from
    unsigned int numFrames,    // Number of frames in the current buffer
    double streamTime,         // Current stream time in seconds
    RtAudioStreamStatus status,// Status flags from RtAudio
    void *userData             // User-provided data pointer
) 
{
    // Cast void pointers to actual audio sample type
    float *input = (float*)inputBuffer;
    float *output = (float*)outputBuffer;
    
    // Handle stream overflow
    if (status) 
    {
        cerr << "ERROR (non-fatal): Stream overflow :(" << endl;
        return 0; // Keep going
    }
    
    // Create a vector from the input samples, add them to the processing queue
    vector<float> inBuffer(input, input + numFrames);
    ::inputBuffer.push(inBuffer);
    
    // Try to get processed output data
    vector<float> outBuffer;
    bool success = ::outputBuffer.pop(outBuffer);
    
    // If we managed to get the data and its the right size
    if (success && outBuffer.size() == numFrames) 
    {
        // Copy the processed data to the output buffer (audible)
        for (unsigned int i = 0; i < numFrames; i++) 
        {
            output[i] = outBuffer[i];
        }
    } 
    else 
    {
        // If we couldn't get any data, fill with silence
        for (unsigned int i = 0; i < numFrames; i++) 
        {
            output[i] = 0.0f;
        }
    }
    
    return 0; // Success :)
}

/*
Audio processing thread function
    - Runs in a separate thread from the audio I/O
    - Takes audio data from the input queue
    - Processes it through the noise gate
    - Places results in output queue
*/
void processingThread() 
{
    vector<float> inputData;                       // Holds incoming audio
    vector<float> noiseGateData(FRAMES_PER_BUFFER); // Holds audio after NoiseGate
    vector<float> outputData(FRAMES_PER_BUFFER);    // Holds processed audio
    
    // Process audio until told to stop
    while (running.load()) 
    {
        // Try to get input data from the queue
        if (!inputBuffer.pop(inputData)) 
        {
            // If we can't get data, prepare for shutdown (probably)
            break;
        }
        
        // Apply the noise gate effect to the audio
        noiseGate.process(inputData.data(), noiseGateData.data(), inputData.size());

        // Apply the EQ effect to the audio
        eq.process(noiseGateData.data(), outputData.data(), inputData.size());
        
        // Send the processed audio to the output queue, ta-da
        outputBuffer.push(outputData);
    }
}

// Console UI management
void clearConsoleLine() {
    cout << "\33[2K\r";
}

void moveCursorUp(int lines) {
    cout << "\033[" << lines << "A";
}

void printUI() {
    // Move up to the start of UI
    moveCursorUp(8);
    
    clearConsoleLine(); cout << "Noise Gate: " << (noiseGate.isEnabled() ? "Enabled " : "Disabled") << endl;
    clearConsoleLine(); cout << "EQ:         " << (eq.isEnabled() ? "Enabled " : "Disabled") << endl;
    clearConsoleLine(); cout << "Low band:   " << eq.getBandGain(0) << endl;
    clearConsoleLine(); cout << "Mid band:   " << eq.getBandGain(1) << endl;
    clearConsoleLine(); cout << "High band:  " << eq.getBandGain(2) << endl;

    clearConsoleLine(); cout << "Controls - e: Toggle Noise Gate, g: Toggle EQ, q: Quit" << endl;
    clearConsoleLine(); cout << "          1/z: Low+-, 2/x: Mid+-, 3/c: High+-" << endl;
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
        RtAudio audio;
        
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
        audio.openStream(
            &outputParams,       // Output configuration
            &inputParams,        // Input configuration
            RTAUDIO_FLOAT32,     // Sample format (32-bit float)
            SAMPLE_RATE,         // Sample rate (e.g. 44100 Hz)
            &bufferFrames,       // Buffer size
            &audioCallback       // Callback function
        );

        // Create and start the audio processing thread
        thread procThread(::processingThread);
        
        // Start the audio stream (call the callback)
        audio.startStream();
        
        cout << string(8, '\n');
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
            }
        
            // Refresh the UI
            printUI();
        }
        
        // Begin clean shutdown
        cout << "Shutting down..." << endl;
                
        // Stop and close the audio stream
        audio.stopStream();
        audio.closeStream();
        
        // Signal buffer queues to unblock waiting threads
        inputBuffer.setDone();
        outputBuffer.setDone();
        
        // Wait for processing thread to finish
        procThread.join();
        
        cout << "Shutdown complete..." << endl;
    } 
    catch (...) 
    {
        cerr << "ERROR: wtf happened" << endl;
        return 1;
    }
    
    return 0;
}
