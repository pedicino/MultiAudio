#ifndef NOISE_GATE_H
#define NOISE_GATE_H

#include "../common.h"

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
    unsigned int sampleRate;
    unsigned int fftSize;
    float threshold;
    atomic<bool> ngActive;
    
    // Fast Fourier Transform variables
    fftw_plan fftPlan;
    double* timeData;
    fftw_complex* frequencyData;
    
    // Frequency band analysis storage
    vector<double> bandEnergies;
    
    // Calculate the volume energy in each frequency band
    void calculateBandEnergies();
    
    // Gate entry decision
    float determineGateState();
    
    // Actually apply the gate based on frequency analysis
    float calculateGateGain(const float* inputBuffer, size_t bufferSize);

public:
    // Constructor
    NoiseGate(unsigned int rate = SAMPLE_RATE, 
              unsigned int size = FFT_SIZE, 
              float thresh = 0.05f);
    
    // Destructor
    ~NoiseGate();
    
    // Process a buffer of audio through the noise gate
    void process(const float* inputBuffer, float* outputBuffer, size_t bufferSize);
    
    // Set the gate threshold
    void setThreshold(float newThreshold);
    
    // Enable/disable the noise gate entirely
    void setEnabled(bool isEnabled);
    
    // Boolean to check if the noise gate is currently enabled
    bool isEnabled() const;
};

#endif // NOISE_GATE_H