#include "Limiter.h"
#include <iostream>

const float TIME_EPSILON = 1e-6f;

Limiter::Limiter(unsigned int rate, float thresh, float attackMs, float releaseMs)
    : sampleRate(rate),
      currentGain(1.0f),
      limiterActive(false)
{
    setThreshold(thresh);
    setAttackTime(attackMs);
    setReleaseTime(releaseMs);
}

void Limiter::calculateCoeffs()
{

    attackCoeff = expf(-1.0f / (std::max(TIME_EPSILON, attackTimeMs / 1000.0f) * sampleRate));
    releaseCoeff = expf(-1.0f / (std::max(TIME_EPSILON, releaseTimeMs / 1000.0f) * sampleRate));
}

void Limiter::process(const float* inputBuffer, float* outputBuffer, size_t bufferSize)
{
    if (!limiterActive.load() || bufferSize == 0)
    {
        std::copy(inputBuffer, inputBuffer + bufferSize, outputBuffer);
        return;
    }
    

    float peak = 0.0f;
    for (size_t i = 0; i < bufferSize; ++i)
    {
        peak = std::max(peak, std::abs(inputBuffer[i]));
    }

    // Calculate the desired gain reduction based on the peak
    // If peak is below threshold, target gain is 1.0 (no reduction)
    // If peak is above threshold, target gain reduces the peak *to* the threshold
    float targetGain = (peak <= threshold) ? 1.0f : threshold / peak;

    // --- Smooth the gain change using attack/release ---
    // We apply smoothing once per block based on the block's peak.
    // A more sophisticated limiter would do this sample-by-sample or use a lookahead buffer.
    
    if (targetGain < currentGain)
    {
        currentGain = attackCoeff * currentGain + (1.0f - attackCoeff) * targetGain;
        currentGain = std::max(currentGain, targetGain);
    }
    else
    {
        currentGain = releaseCoeff * currentGain + (1.0f - releaseCoeff) * targetGain;
        currentGain = std::min(currentGain, 1.0f);
    }

    for (size_t i = 0; i < bufferSize; ++i)
    {
        outputBuffer[i] = inputBuffer[i] * currentGain;
    }
}

// --- Parameter Setters ---

void Limiter::setThreshold(float newThreshold)
{
    threshold = std::max(0.0f, std::min(1.0f, newThreshold));
}

float Limiter::getThreshold() const
{
    return threshold;
}

void Limiter::setAttackTime(float ms)
{
    attackTimeMs = std::max(0.1f, ms);
    calculateCoeffs();
}

float Limiter::getAttackTime() const
{
    return attackTimeMs;
}

void Limiter::setReleaseTime(float ms)
{
    releaseTimeMs = std::max(1.0f, ms);
    calculateCoeffs();
}

float Limiter::getReleaseTime() const
{
    return releaseTimeMs;
}

void Limiter::setEnabled(bool isEnabled)
{
    limiterActive.store(isEnabled);
}

bool Limiter::isEnabled() const
{
    return limiterActive.load();
}