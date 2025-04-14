#include "NoiseGate.h"

#include <algorithm>
#include <cmath>

namespace audio {

//--------------------------------------------------------------------------
// Lifecycle
//--------------------------------------------------------------------------

NoiseGate::NoiseGate(unsigned int rate, unsigned int size, float thresh, float attackMs, float releaseMs)
    : AudioEffect(rate),
      fftSize(size),
      currentGain(0.0f),
      bandEnergies(NUM_BANDS, 0.0)
{
    setThreshold(thresh);
    setAttackTime(attackMs);
    setReleaseTime(releaseMs);

    timeData = fftw_alloc_real(fftSize);
    frequencyData = fftw_alloc_complex(fftSize / 2 + 1);
    fftPlan = fftw_plan_dft_r2c_1d(fftSize, timeData, frequencyData, FFTW_ESTIMATE);

    if (!timeData || !frequencyData || !fftPlan)
    {
        effectActive.store(false);
    }

    reset();
}

NoiseGate::~NoiseGate()
{
    if (fftPlan)
    {
        fftw_destroy_plan(fftPlan);
    }
    if (timeData)
    {
        fftw_free(timeData);
    }
    if (frequencyData)
    {
        fftw_free(frequencyData);
    }
}

//--------------------------------------------------------------------------
// Private Methods
//--------------------------------------------------------------------------

void NoiseGate::calculateCoeffs()
{
    float attackSeconds = std::max(NG_TIME_EPSILON, attackTimeMs / 1000.0f);
    float releaseSeconds = std::max(NG_TIME_EPSILON, releaseTimeMs / 1000.0f);

    attackCoeff = std::exp(-1.0f / (attackSeconds * sampleRate));
    releaseCoeff = std::exp(-1.0f / (releaseSeconds * sampleRate));
}

void NoiseGate::calculateBandEnergies()
{
    std::fill(bandEnergies.begin(), bandEnergies.end(), 0.0);

    for (unsigned int i = 1; i < fftSize / 2; ++i)
    {
        double real = frequencyData[i][0];
        double imaginary = frequencyData[i][1];
        double energy = real * real + imaginary * imaginary;

        if (i > 0)
        {
            unsigned int band = static_cast<unsigned int>(
                (NUM_BANDS - 1) * std::log2(static_cast<double>(i)) /
                std::log2(static_cast<double>(fftSize / 2 - 1))
            );
            band = std::min(band, static_cast<unsigned int>(NUM_BANDS - 1));

            if (band < bandEnergies.size())
            {
                bandEnergies[band] += energy;
            }
        }
    }
}

float NoiseGate::determineTargetGain(const float* inputBuffer, std::size_t numFrames)
{
    std::fill_n(timeData, fftSize, 0.0);
    std::size_t copySize = std::min(numFrames, static_cast<std::size_t>(fftSize));
    for (std::size_t i = 0; i < copySize; ++i)
    {
        timeData[i] = static_cast<double>(inputBuffer[i]);
    }

    if (fftPlan)
    {
        fftw_execute(fftPlan);
    }
    else
    {
        return 1.0f;
    }

    calculateBandEnergies();

    double totalEnergy = 0.0;
    for (double energy : bandEnergies)
    {
        totalEnergy += energy;
    }

    double avgEnergy = (NUM_BANDS > 0) ? (totalEnergy / NUM_BANDS) : 0.0;
    double normalizationFactor = static_cast<double>(fftSize);
    double normalizedAvgEnergy = avgEnergy / normalizationFactor;

    return (normalizedAvgEnergy > (threshold * threshold)) ? 1.0f : 0.0f;
}

//--------------------------------------------------------------------------
// AudioEffect Interface
//--------------------------------------------------------------------------

void NoiseGate::process(const float* inputBuffer, float* outputBuffer, std::size_t numFrames)
{
    if (!effectActive.load() || numFrames == 0)
    {
        std::copy(inputBuffer, inputBuffer + numFrames, outputBuffer);
        if (!effectActive.load())
        {
            currentGain = 0.0f;
        }
        return;
    }

    float targetGain = determineTargetGain(inputBuffer, numFrames);

    for (std::size_t i = 0; i < numFrames; ++i)
    {
        if (targetGain > currentGain)
        {
            currentGain = attackCoeff * currentGain + (1.0f - attackCoeff) * targetGain;
            currentGain = std::min(currentGain, targetGain);
        }
        else
        {
            currentGain = releaseCoeff * currentGain + (1.0f - releaseCoeff) * targetGain;
            currentGain = std::max(currentGain, targetGain);
        }

        outputBuffer[i] = inputBuffer[i] * currentGain;
    }
}

void NoiseGate::reset()
{
    std::fill(bandEnergies.begin(), bandEnergies.end(), 0.0);
    currentGain = 0.0f;
}

//--------------------------------------------------------------------------
// Noise Gate Controls
//--------------------------------------------------------------------------

void NoiseGate::setThreshold(float newThreshold)
{
    threshold = std::max(0.0f, std::min(1.0f, newThreshold));
}

float NoiseGate::getThreshold() const
{
    return threshold;
}

void NoiseGate::setAttackTime(float ms)
{
    attackTimeMs = std::max(0.1f, ms);
    calculateCoeffs();
}

float NoiseGate::getAttackTime() const
{
    return attackTimeMs;
}

void NoiseGate::setReleaseTime(float ms)
{
    releaseTimeMs = std::max(1.0f, ms);
    calculateCoeffs();
}

float NoiseGate::getReleaseTime() const
{
    return releaseTimeMs;
}

} // namespace audio