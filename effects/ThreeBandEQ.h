#ifndef THREE_BAND_EQ_H
#define THREE_BAND_EQ_H

#include "common.h"

/*
Three Band EQ -
    - Analyze audio in the frequency domain
    - Apply a gain factor to each band
    - Transform back to time domain
*/
class ThreeBandEQ
{
private:
    unsigned int sampleRate;
    unsigned int fftSize;
    atomic<bool> eqActive;

    // Fast Fourier Transform variables
    fftw_plan fftForwardPlan;
    fftw_plan fftInversePlan;
    double* timeData;
    fftw_complex* frequencyData;

    float bandCutoffs[NUM_EQ_BANDS];
    float bandGains[NUM_EQ_BANDS];

    // Get the index of the band for a given frequency
    int getBandIndex(float frequency);

    // Apply the EQ on each band
    void applyEQ();

public:
    // Constructor
    ThreeBandEQ(unsigned int rate = SAMPLE_RATE, 
                unsigned int size = FFT_SIZE);

    // Destructor
    ~ThreeBandEQ();

    // Process audio through the EQ
    void process(const float* inputBuffer, float* outputBuffer, size_t bufferSize);

    // Manage gains for each band
    void setBandGain(unsigned int bandIndex, float gain);
    float getBandGain(unsigned int bandIndex) const;

    // Manage cutoff frequencies for each band
    void setBandCutoff(unsigned int bandIndex, float frequency);
    float getBandCutoff(unsigned int bandIndex) const;

    // Enable/disable the EQ
    void setEnabled(bool isEnabled);
    bool isEnabled() const;
};

#endif // THREE_BAND_EQ_H