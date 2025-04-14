#include "Limiter.h"

#include <algorithm>
#include <cmath>

namespace audio {

// Small constant to prevent division by zero
constexpr float TIME_EPSILON = 1e-6f;

//--------------------------------------------------------------------------
// Lifecycle
//--------------------------------------------------------------------------

Limiter::Limiter(unsigned int rate, float thresh, float attackMs, float releaseMs)
    : sampleRate(rate),
      currentGain(1.0f),
      limiterActive(false)
{
    setThreshold(thresh);
    setAttackTime(attackMs);
    setReleaseTime(releaseMs);
}

//--------------------------------------------------------------------------
// Private Methods
//--------------------------------------------------------------------------

void Limiter::calculateCoeffs()
{
    float attackSeconds = std::max(TIME_EPSILON, attackTimeMs / 1000.0f);
    float releaseSeconds = std::max(TIME_EPSILON, releaseTimeMs / 1000.0f);

    // Calculate smoothing coefficients for exponential gain control
    attackCoeff = std::exp(-1.0f / (attackSeconds * sampleRate));
    releaseCoeff = std::exp(-1.0f / (releaseSeconds * sampleRate));
}

//--------------------------------------------------------------------------
// Audio Processing Interface
//--------------------------------------------------------------------------

void Limiter::process(const float* inputBuffer, float* outputBuffer, std::size_t bufferSize)
{
    if (!limiterActive.load() || bufferSize == 0)
    {
        std::copy(inputBuffer, inputBuffer + bufferSize, outputBuffer);
        return;
    }

    for (std::size_t i = 0; i < bufferSize; ++i)
    {
        float inputAbs = std::abs(inputBuffer[i]);

        // Calculate target gain - unity if below threshold, otherwise reduce to threshold
        float targetGain = (inputAbs <= threshold) ?
                           1.0f :
                           threshold / (inputAbs + TIME_EPSILON);

        // Apply attack/release smoothing based on whether gain decreases or increases
        if (targetGain < currentGain)
        {
            // Attack phase - gain reduction
            currentGain = attackCoeff * currentGain + (1.0f - attackCoeff) * targetGain;
            currentGain = std::max(currentGain, targetGain);
        }
        else
        {
            // Release phase - gain recovery
            currentGain = releaseCoeff * currentGain + (1.0f - releaseCoeff) * targetGain;
            currentGain = std::min(currentGain, 1.0f);
        }

        outputBuffer[i] = inputBuffer[i] * currentGain;
    }
}

//--------------------------------------------------------------------------
// Limiter Controls
//--------------------------------------------------------------------------

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
    if (!isEnabled)
    {
        currentGain = 1.0f;
    }
}

bool Limiter::isEnabled() const
{
    return limiterActive.load();
}

} // namespace audio
