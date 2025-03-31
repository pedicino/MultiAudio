#include "NoiseGate.h"

NoiseGate::NoiseGate(unsigned int rate, unsigned int size, float thresh)
    : sampleRate(rate), 
      fftSize(size), 
      threshold(thresh), 
      ngActive(false), 
      bandEnergies(NUM_BANDS, 0.0)
{
    // Allocate memory for FFT processing
    timeData = fftw_alloc_real(fftSize);
    frequencyData = fftw_alloc_complex(fftSize / 2 + 1);
    
    // Create FFT plan
    fftPlan = fftw_plan_dft_r2c_1d(fftSize, timeData, frequencyData, FFTW_MEASURE);
}

NoiseGate::~NoiseGate() 
{
    // Clean up FFT resources
    fftw_destroy_plan(fftPlan);
    fftw_free(timeData);
    fftw_free(frequencyData);
}

void NoiseGate::calculateBandEnergies() 
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

float NoiseGate::determineGateState() 
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

float NoiseGate::calculateGateGain(const float* inputBuffer, size_t bufferSize) 
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

void NoiseGate::process(const float* inputBuffer, float* outputBuffer, size_t bufferSize) 
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

void NoiseGate::setThreshold(float newThreshold) 
{
    threshold = max(0.0f, min(1.0f, newThreshold));
}

void NoiseGate::setEnabled(bool isEnabled) 
{
    ngActive.store(isEnabled);
}

bool NoiseGate::isEnabled() const 
{
    return ngActive.load();
}