#include <iostream>             // I/O operations
#include <vector>               // Dynamically-sized, memory-efficient arrays
#include <cmath>                // Mathematical functions
#include <thread>               // Multithreading
#include <mutex>                // Prevent multiple threads from accessing shared data at once
#include <condition_variable>   // Allow threads to wait until specific events
#include <queue>                // FIFO structure for buffer
#include <atomic>               // Thread-safe variables that don't need locks
#include <algorithm>            // Data structure algorithms e.g. min(), max()
#include <fftw3.h>              // Fast Fourier Transforms (time-domain audio to frequency domain)
#include <rtaudio/RtAudio.h>    // Cross-platform audio library

// I hate double colons ::
using namespace std;

// Global constants
const unsigned int SAMPLE_RATE = 44100;     // Audio samples per second (Hz), CD quality
const unsigned int FRAMES_PER_BUFFER = 512; // Number of audio samples processed as a group within one buffer
const unsigned int FFT_SIZE = 1024;         // Fast Fourier Transform window size
const unsigned int NUM_CHANNELS = 1;        // Mono audio
const unsigned int NUM_BANDS = 4;           // Max 4 bands for simplicity

/*
Thread-safe queue of audio buffers
    - Passes audio between threads
    - Producers add audio data to the buffer
    - Consumers receive data for further processing
*/
class BufferQueue 
{
private:
    queue<vector<float>> bufferQueue;   // Queue that contains multiple audio buffers, each having multiple samples
    size_t queueCapacity;               // Maximum number of buffers allowed in the queue
    mutex mtx;                          // Ensure only one thread accesses the queue at a time

    condition_variable queueHasData;    // Tell consumers when new data is ready for processing
    condition_variable queueHasSpace;   // Tell producers when there's room for more in the queue

    atomic<bool> done;                  // Thread-safe shutdown flag

public:
    // Constructor - creates an EMPTY queue that can hold up to "capacity" buffers
    BufferQueue(size_t capacity = 10) : queueCapacity(capacity), done(false) 
    {
        // (No buffers in the queue yet)
    }
    
    // Producer - add a new audio buffer to the queue
    void push(const vector<float>& buffer) 
    {
        // Lock to prevent other threads from accessing the queue
        unique_lock<mutex> lock(mtx);

        // Wait until there's space in the queue OR shut down
        // Lambda function performs repeated checks
        queueHasSpace.wait(lock, [this]() { return bufferQueue.size() < queueCapacity || done.load(); });
        
        // If shutting down, stop and don't add more data
        if (done.load())
        {
            return;
        }

        // Add the new buffer to the queue
        bufferQueue.push(buffer);

        // Release lock
        lock.unlock();

        // Notify one waiting consumer thread that data is ready
        queueHasData.notify_one();
    }
    
    // Consumer - Remove the next audio buffer from the queue and return it
    bool pop(vector<float>& buffer) 
    {
        // Lock to prevent other threads from accessing the queue
        unique_lock<mutex> lock(mtx);

        // Wait until audio data is available in the queue OR shut down
        queueHasData.wait(lock, [this]() { return !bufferQueue.empty() || done.load(); });
        
        // No data and shutdown triggered
        if (bufferQueue.empty() && done.load())
        {
            return false; // Process failed :(
        }
        
        // Save the buffer at the front of the queue
        buffer = bufferQueue.front();

        // Remove that buffer
        bufferQueue.pop();

        // Release lock
        lock.unlock();

        // Wake up one waiting producer thread
        queueHasSpace.notify_one();

        return true; // Success :)
    }
    
    // Shutdown - Signal all waiting threads
    void setDone() 
    {
        // Atomic "done" flag set to true
        done.store(true);

        // Wake up all waiting consumers
        queueHasData.notify_all();

        // Wake up all waiting producers
        queueHasSpace.notify_all();
    }
};

/*
Noise Gate - Simple version
    - Analyze audio in the frequency domain
    - Apply a simple threshold to reduce background noise
    - More features like attack/release coefficients and frequency analysis for later
*/
class NoiseGate 
{
private:
    // Specifications
    unsigned int sampleRate;        // Audio samples per second (Hz)
    unsigned int fftSize;           // Size of the frequency transform window
    float threshold;                // Cutoff, audio below this should be reduced

    atomic<bool> ngActive;         // Whether the noise gate is enabled or not
    
    // Fast Fourier Transform variables - for converting audio to frequency data
    fftw_plan fftPlan;              // Instructions for FFTW library
    double* timeData;               // Audio samples in time domain (what we hear)
    fftw_complex* frequencyData;    // Audio represented as frequencies (after transform)
    
    // Frequency band analysis storage
    vector<double> bandEnergies;    // Current energy (volume) in each frequency band
    
    /*
    Calculate the volume energy in each frequency band
    */
    void calculateBandEnergies() 
    {
        // Reset all band energies to 0
        fill(bandEnergies.begin(), bandEnergies.end(), 0.0);
        
        // Process each frequency bin (skip DC at index 0)
        for (unsigned int i = 1; i < fftSize / 2; i++) 
        {
            // Calculate magnitude of this frequency component
            double real = frequencyData[i][0];
            double imaginary = frequencyData[i][1];
            double magnitude = sqrt(real * real + imaginary * imaginary);
            
            // Determine which band this frequency belongs to
            unsigned int band = static_cast<unsigned int>(
                NUM_BANDS * log2(i) / log2(fftSize / 2)
            );
            
            // Ensure we don't exceed the number of bands
            if (band < NUM_BANDS) 
            {
                // Add this energy (magnitude^2) to the band
                bandEnergies[band] += magnitude * magnitude;
            }
        }
        
        // Normalize band energies
        for (unsigned int i = 0; i < NUM_BANDS; i++) 
        {
            bandEnergies[i] /= fftSize;
        }
    }
    
    /*
    Gate entry decision
        - Implement smoothening later
        - Implement envelope following later
    */
    float determineGateState() 
    {
        double totalEnergy = 0.0;
        
        // Sum up the energy across all frequency bands
        for (unsigned int i = 0; i < NUM_BANDS; i++) 
        {
            totalEnergy += bandEnergies[i];
        }
        
        // Average the energy across bands
        double avgEnergy = totalEnergy / NUM_BANDS;
        
        // Binary decision (prototype) - open or closed?
        return avgEnergy > threshold ? 1.0f : 0.0f;
    }
    
    /*
    Actually apply the gate based on frequency analysis
    */
    float calculateGateGain(const float* inputBuffer, size_t bufferSize) 
    {
        // Zero out the FFT input buffer
        fill_n(timeData, fftSize, 0.0);
        
        // Copy input data to FFT buffer (no windowing yet)
        size_t copySize = min(bufferSize, static_cast<size_t>(fftSize));
        for (size_t i = 0; i < copySize; i++) 
        {
            timeData[i] = inputBuffer[i];
        }
        
        // Perform forward FFT to convert time domain to frequency domain
        fftw_execute(fftPlan);
        
        // Calculate the energy in each frequency band
        calculateBandEnergies();
        
        // Finally, determine if gate should be open or closed
        return determineGateState();
    }

public:
    /*
    Constructor - noise gate with specified parameters
    */
    NoiseGate
    (
        unsigned int rate = SAMPLE_RATE, 
        unsigned int size = FFT_SIZE, 
        float thresh = 0.9f // CHANGE THIS VARIABLE TO MODIFY THE NOISE GATE
    ): 
        sampleRate(rate), 
        fftSize(size), 
        threshold(thresh), 
        ngActive(true), 
        bandEnergies(NUM_BANDS, 0.0)
    {
        // Allocate memory for FFT processing
        timeData = fftw_alloc_real(fftSize);
        frequencyData = fftw_alloc_complex(fftSize / 2 + 1);
        
        // Create FFT plan
        fftPlan = fftw_plan_dft_r2c_1d(fftSize, timeData, frequencyData, FFTW_MEASURE);
    }
    
    /*
    Destructor - free all allocated resources when noise gate is destroyed
    */
    ~NoiseGate() 
    {
        // Clean up FFT resources
        fftw_destroy_plan(fftPlan);
        fftw_free(timeData);
        fftw_free(frequencyData);
    }
    
    /*
    Process a buffer of audio through the noise gate
        - Take input audio and determine if it goes above/below the noise threshold
    */
    void process(const float* inputBuffer, float* outputBuffer, size_t bufferSize) 
    {
        // If the gate is disabled, just copy the original audio input to the new output unchanged
        if (!ngActive.load()) 
        {
            copy(inputBuffer, inputBuffer + bufferSize, outputBuffer);
            return;
        }
        
        // Calculate the gate gain (0.0 = silent, 1.0 = pass through)
        float gateGain = calculateGateGain(inputBuffer, bufferSize);
        
        // Apply the gain to each sample in the buffer
        for (size_t i = 0; i < bufferSize; i++) 
        {
            outputBuffer[i] = inputBuffer[i] * gateGain;
        }
    }
    
    /*
    Set the gate threshold
        - Lower value means more sound passes through
        - Higher value means more sound is filtered out
    */
    void setThreshold(float newThreshold) 
    {
        threshold = max(0.0f, min(1.0f, newThreshold));
    }
    
    /*
    Enable/disable the noise gate entirely
    */
    void setEnabled(bool isEnabled) 
    {
        ngActive.store(isEnabled);
    }
    
    /*
    Boolean to check if the noise gate is currently enabled
    */
    bool isEnabled() const 
    {
        return ngActive.load();
    }
};

// Global variables used by m ultiple threads
BufferQueue inputBuffer;      // Queue for raw microphone input data
BufferQueue outputBuffer;     // Queue for processed output data
NoiseGate noiseGate;          // The noise gate effect instance
atomic<bool> running(true);   // Signal to control thread execution

/*
RtAudio Callback function
    - Called by audio API for each audio buffer
    - Receive the raw audio from the microphone, pass it to thread, send processed audio back
*/
int audioCallback
    (
    void *outputBuffer,         // Where output samples should be written
    void *inputBuffer,          // Where microphone input comes from
    unsigned int numFrames,     // Number of frames in the current buffer
    double streamTime,          // Current stream time in seconds
    RtAudioStreamStatus status, // Status flags from RtAudio
    void *userData              // User-provided data pointer
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
    vector<float> inputData;                        // Holds incoming audio
    vector<float> outputData(FRAMES_PER_BUFFER); // Holds processed audio
    
    // Process audio until told to stop
    while (running.load()) 
    {
        // Try to get input data fromn the queue
        if (!inputBuffer.pop(inputData)) 
        {
            // If we can't get data, prepare for shutdown (probably)
            break;
        }
        
        // Apply the noise gate effect to the audio
        noiseGate.process(inputData.data(), outputData.data(), inputData.size());
        
        // Send the "gated" audio to the output queue, ta-da
        outputBuffer.push(outputData);
    }
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
        
        // Program seems to be running?
        cout << "Attempting to run noise gate effect. Press Enter to quit." << endl;
        
        // Wait for user to quit
        cin.get();
        
        // Begin clean shutdown
        cout << "Shutting down..." << endl;
        
        // Signal processing thread to stop
        running.store(false);
        
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