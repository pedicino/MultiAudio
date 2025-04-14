#include "ThreeBandEQ.h"

#include <iostream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace audio {

//--------------------------------------------------------------------------
// Lifecycle
//--------------------------------------------------------------------------

ThreeBandEQ::ThreeBandEQ(unsigned int rate, unsigned int frameSize)
    : AudioEffect(rate),
      hopSize(frameSize),
      fftSize(frameSize * 2),
      timeData(nullptr),
      frequencyData(nullptr),
      fftForwardPlan(nullptr),
      fftInversePlan(nullptr)
{
    if (hopSize == 0)
    {
        // Invalid hop size, disable effect
        effectActive.store(false);
        return;
    }

    // Initialize default band cutoffs and gains
    const float DEFAULT_LOW_MID_CUTOFF = 250.0f;
    const float DEFAULT_MID_HIGH_CUTOFF = 4000.0f;
    const float MAX_FREQ = sampleRate / 2.0f;

    setBandCutoff(0, DEFAULT_LOW_MID_CUTOFF);
    setBandCutoff(1, DEFAULT_MID_HIGH_CUTOFF);
    setBandCutoff(2, MAX_FREQ);

    for (unsigned int i = 0; i < NUM_EQ_BANDS; ++i)
    {
        setBandGain(i, 1.0f);
    }

    bool setupOk = true;

    // Allocate FFTW resources
    try
    {
        timeData = fftw_alloc_real(fftSize);
        frequencyData = fftw_alloc_complex(fftSize / 2 + 1);

        if (!timeData || !frequencyData)
        {
            setupOk = false;
        }
        else
        {
            fftForwardPlan = fftw_plan_dft_r2c_1d(fftSize, timeData, frequencyData, FFTW_ESTIMATE);
            fftInversePlan = fftw_plan_dft_c2r_1d(fftSize, frequencyData, timeData, FFTW_ESTIMATE);

            if (!fftForwardPlan || !fftInversePlan)
            {
                setupOk = false;
            }
        }

        if (setupOk)
        {
            // Initialize buffers
            window.resize(fftSize);
            inputBufferInternal.resize(fftSize, 0.0);
            outputOverlapBuffer.resize(fftSize - hopSize, 0.0);
            calculateWindow();
        }
    }
    catch (...)
    {
        setupOk = false;
    }

    if (!setupOk)
    {
        // Cleanup on failure
        effectActive.store(false);
        if (fftForwardPlan) fftw_destroy_plan(fftForwardPlan);
        if (fftInversePlan) fftw_destroy_plan(fftInversePlan);
        if (timeData) fftw_free(timeData);
        if (frequencyData) fftw_free(frequencyData);
    }
    else
    {
        reset();
    }
}

ThreeBandEQ::~ThreeBandEQ()
{
    // Free FFTW resources
    if (fftForwardPlan) fftw_destroy_plan(fftForwardPlan);
    if (fftInversePlan) fftw_destroy_plan(fftInversePlan);
    if (timeData) fftw_free(timeData);
    if (frequencyData) fftw_free(frequencyData);
}

//--------------------------------------------------------------------------
// Private Methods
//--------------------------------------------------------------------------

void ThreeBandEQ::calculateWindow()
{
    if (window.size() != fftSize)
    {
        window.resize(fftSize);
    }

    // Generate Hann window
    for (std::size_t i = 0; i < fftSize; ++i)
    {
        window[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (fftSize - 1)));
    }
}

float ThreeBandEQ::getSmoothGain(float frequency)
{
    // Define transition regions around band cutoffs
    float transition1Start = bandCutoffs[0] * 0.8f;
    float transition1End = bandCutoffs[0] * 1.2f;
    float transition2Start = bandCutoffs[1] * 0.8f;
    float transition2End = bandCutoffs[1] * 1.2f;

    // Piecewise gain selection and smoothing
    if (frequency < transition1Start)
    {
        return bandGains[0];
    }
    else if (frequency > transition1End && frequency < transition2Start)
    {
        return bandGains[1];
    }
    else if (frequency > transition2End)
    {
        return bandGains[2];
    }
    else if (frequency >= transition1Start && frequency <= transition1End)
    {
        // Smooth transition low → mid
        float t = (frequency - transition1Start) / (transition1End - transition1Start);
        t = (1.0f - std::cos(t * M_PI)) * 0.5f;
        return bandGains[0] * (1.0f - t) + bandGains[1] * t;
    }
    else
    {
        // Smooth transition mid → high
        float t = (frequency - transition2Start) / (transition2End - transition2Start);
        t = (1.0f - std::cos(t * M_PI)) * 0.5f;
        return bandGains[1] * (1.0f - t) + bandGains[2] * t;
    }
}

void ThreeBandEQ::applyEQGain()
{
    if (!frequencyData) return;

    // Apply gain to DC bin
    frequencyData[0][0] *= bandGains[0];
    frequencyData[0][1] *= bandGains[0];

    // Apply gain to other bins with phase preservation
    for (unsigned int i = 1; i < fftSize / 2; ++i)
    {
        float frequency = static_cast<float>(i) * sampleRate / fftSize;
        float gain = getSmoothGain(frequency);

        double real = frequencyData[i][0];
        double imag = frequencyData[i][1];
        double magnitude = std::sqrt(real * real + imag * imag);
        double phase = std::atan2(imag, real);

        magnitude *= gain;

        frequencyData[i][0] = magnitude * std::cos(phase);
        frequencyData[i][1] = magnitude * std::sin(phase);
    }

    // Apply gain to Nyquist bin
    unsigned int nyquist_bin = fftSize / 2;
    if (nyquist_bin < fftSize)
    {
        frequencyData[nyquist_bin][0] *= bandGains[2];
        frequencyData[nyquist_bin][1] *= bandGains[2];
    }
}

//--------------------------------------------------------------------------
// AudioEffect Interface
//--------------------------------------------------------------------------

void ThreeBandEQ::process(const float* inputBuffer, float* outputBuffer, std::size_t numFrames)
{
    if (!effectActive.load() || numFrames == 0)
    {
        // Effect bypass or invalid input
        if (numFrames > 0 && inputBuffer && outputBuffer)
        {
            std::copy(inputBuffer, inputBuffer + numFrames, outputBuffer);
        }
        if (!effectActive.load())
        {
            reset();
        }
        return;
    }

    if (numFrames != hopSize)
    {
        // Incorrect block size
        if (outputBuffer) std::fill_n(outputBuffer, numFrames, 0.0f);
        return;
    }

    if (!fftForwardPlan || !fftInversePlan || !timeData || !frequencyData ||
        inputBufferInternal.size() != fftSize || outputOverlapBuffer.size() != (fftSize - hopSize) ||
        window.size() != fftSize || !inputBuffer || !outputBuffer)
    {
        // Resource validation failed
        if (outputBuffer) std::fill_n(outputBuffer, numFrames, 0.0f);
        return;
    }

    // Shift previous input for overlap
    std::memmove(inputBufferInternal.data(), inputBufferInternal.data() + hopSize,
                 (fftSize - hopSize) * sizeof(double));

    // Copy new input
    for (std::size_t i = 0; i < hopSize; ++i)
    {
        inputBufferInternal[fftSize - hopSize + i] = static_cast<double>(inputBuffer[i]);
    }

    // Copy to FFT input buffer
    std::memcpy(timeData, inputBufferInternal.data(), fftSize * sizeof(double));

    // Apply window function
    for (std::size_t i = 0; i < fftSize; ++i)
    {
        timeData[i] *= window[i];
    }

    // Forward FFT
    fftw_execute(fftForwardPlan);

    // EQ gain application
    applyEQGain();

    // Inverse FFT
    fftw_execute(fftInversePlan);

    // Overlap-add output
    for (std::size_t i = 0; i < fftSize - hopSize; ++i)
    {
        outputOverlapBuffer[i] += timeData[i] / fftSize;
    }

    for (std::size_t i = 0; i < hopSize; ++i)
    {
        outputBuffer[i] = static_cast<float>(outputOverlapBuffer[i]);
    }

    // Shift overlap buffer
    std::memmove(outputOverlapBuffer.data(), outputOverlapBuffer.data() + hopSize,
                 (fftSize - hopSize - hopSize) * sizeof(double));

    // Fill new overlap portion
    for (std::size_t i = 0; i < hopSize; ++i)
    {
        outputOverlapBuffer[fftSize - hopSize - hopSize + i] = timeData[fftSize - hopSize + i] / fftSize;
    }
}

void ThreeBandEQ::reset()
{
    if (!inputBufferInternal.empty())
    {
        std::fill(inputBufferInternal.begin(), inputBufferInternal.end(), 0.0);
    }
    if (!outputOverlapBuffer.empty())
    {
        std::fill(outputOverlapBuffer.begin(), outputOverlapBuffer.end(), 0.0);
    }
}

//--------------------------------------------------------------------------
// EQ Controls
//--------------------------------------------------------------------------

void ThreeBandEQ::setBandGain(unsigned int bandIndex, float gain)
{
    if (bandIndex < NUM_EQ_BANDS)
    {
        bandGains[bandIndex] = std::max(0.0f, std::min(6.0f, gain));
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
        float nyquist = sampleRate / 2.0f;
        bandCutoffs[bandIndex] = std::max(20.0f, std::min(nyquist, frequency));
    }
}

float ThreeBandEQ::getBandCutoff(unsigned int bandIndex) const
{
    return (bandIndex < NUM_EQ_BANDS) ? bandCutoffs[bandIndex] : 0.0f;
}

} // namespace audio
