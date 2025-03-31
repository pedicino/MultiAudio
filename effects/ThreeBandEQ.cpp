#include "ThreeBandEQ.h"

ThreeBandEQ::ThreeBandEQ(unsigned int rate, unsigned int size)
    : sampleRate(rate), 
      fftSize(size), 
      eqActive(false)
{
    const float EQBAND_START = 200.0f;  // Start frequency for the first band
    const float EQBAND_FACTOR = 10.0f;  // Factor to determine the cutoff frequency of the next band
    
    // Set default cutoff frequencies
    bandCutoffs[0] = EQBAND_START;                      // Low band cutoff (200Hz)
    bandCutoffs[1] = bandCutoffs[0] * EQBAND_FACTOR;    // Mid band cutoff (2000Hz)
    bandCutoffs[2] = bandCutoffs[1] * EQBAND_FACTOR;    // High band upper limit (20000Hz)
    
    // Set default gains
    bandGains[0] = 1.0f;        // Low band gain
    bandGains[1] = 1.0f;        // Mid band gain
    bandGains[2] = 1.0f;        // High band gain

    // Allocate memory for FFT processing
    timeData = fftw_alloc_real(fftSize);
    frequencyData = fftw_alloc_complex(fftSize / 2 + 1);
    
    // Create FFT plans
    fftForwardPlan = fftw_plan_dft_r2c_1d(fftSize, timeData, frequencyData, FFTW_MEASURE);
    fftInversePlan = fftw_plan_dft_c2r_1d(fftSize, frequencyData, timeData, FFTW_MEASURE);
}

ThreeBandEQ::~ThreeBandEQ() 
{
    fftw_destroy_plan(fftForwardPlan);
    fftw_destroy_plan(fftInversePlan);
    fftw_free(timeData);
    fftw_free(frequencyData);
}

int ThreeBandEQ::getBandIndex(float frequency) 
{
    for (int i = 0; i < NUM_EQ_BANDS - 1; i++) {
        if (frequency < bandCutoffs[i]) {
            return i;
        }
    }
    return NUM_EQ_BANDS - 1;
}

void ThreeBandEQ::applyEQ() 
{
    for (unsigned int i = 1; i < fftSize / 2; i++) 
    {
        float frequency = i * (float)sampleRate / fftSize;
        int bandIndex = getBandIndex(frequency);
        
        float gain = bandGains[bandIndex];
        
        frequencyData[i][0] *= gain;
        frequencyData[i][1] *= gain;
    }
}

void ThreeBandEQ::process(const float* inputBuffer, float* outputBuffer, size_t bufferSize) 
{
    // If eq is disabled, output regular audio
    if (!eqActive.load()) 
    {
        copy(inputBuffer, inputBuffer + bufferSize, outputBuffer);
        return;
    }

    // Zero out the FFT input buffer
    fill_n(timeData, fftSize, 0.0);

    // Copy input data to FFT buffer (basic rectangular window)
    size_t copySize = min(bufferSize, static_cast<size_t>(fftSize));
    for (size_t i = 0; i < copySize; i++) 
    {
        timeData[i] = inputBuffer[i];
    }
    
    // Perform forward FFT to convert time domain to frequency domain
    fftw_execute(fftForwardPlan);
    
    // Adjust volume of each frequency band
    applyEQ();
    
    // Inverse FFT, conver frequency domain back to time domain
    fftw_execute(fftInversePlan);

    // Copy normalized time data to output buffer
    for (size_t i = 0; i < copySize; i++) 
    {
        outputBuffer[i] = static_cast<float>(timeData[i] / fftSize);
    }
    
    // Zero out any remaining samples
    for (size_t i = copySize; i < bufferSize; i++) 
    {
        outputBuffer[i] = 0.0f;
    }
}

void ThreeBandEQ::setBandGain(unsigned int bandIndex, float gain) 
{
    if (bandIndex < NUM_EQ_BANDS) 
    {
        // Ensure gain is reasonable (0-4 range)
        bandGains[bandIndex] = max(0.0f, min(4.0f, gain));
    }
}

float ThreeBandEQ::getBandGain(unsigned int bandIndex) const 
{
    return (bandIndex < NUM_EQ_BANDS) ? bandGains[bandIndex] : 1.0f;
}

void ThreeBandEQ::setBandCutoff(unsigned int bandIndex, float frequency) 
{
    if (bandIndex < NUM_EQ_BANDS) 
    {
        // Ensure frequency is in the audible range (20Hz-20kHz)
        bandCutoffs[bandIndex] = max(20.0f, min(20000.0f, frequency));
    }
}

float ThreeBandEQ::getBandCutoff(unsigned int bandIndex) const 
{
    return (bandIndex < NUM_EQ_BANDS) ? bandCutoffs[bandIndex] : 0.0f;
}

void ThreeBandEQ::setEnabled(bool isEnabled) 
{
    eqActive.store(isEnabled);
}

bool ThreeBandEQ::isEnabled() const 
{
    return eqActive.load();
}