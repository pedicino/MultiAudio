#ifndef LIMITER_H
#define LIMITER_H

#include "../common.h"

#include <atomic>

namespace audio {

/**
 * Audio limiter that prevents signals from exceeding a threshold.
 *
 * Applies dynamic gain reduction with configurable attack and release
 * characteristics to maintain peak levels within the specified threshold.
 */
class Limiter
{
private:
    //--------------------------------------------------------------------------
    // Internal State
    //--------------------------------------------------------------------------
    unsigned int sampleRate;
    float threshold;        // Max amplitude (0.0-1.0)
    float attackTimeMs;     // Attack time in milliseconds
    float releaseTimeMs;    // Release time in milliseconds
    float attackCoeff;      // Attack smoothing coefficient
    float releaseCoeff;     // Release smoothing coefficient
    float currentGain;      // Current gain reduction amount
    std::atomic<bool> limiterActive;

    //--------------------------------------------------------------------------
    // Private Methods
    //--------------------------------------------------------------------------
    /**
     * Calculates smoothing coefficients based on attack/release times.
     */
    void calculateCoeffs();

public:
    //--------------------------------------------------------------------------
    // Lifecycle
    //--------------------------------------------------------------------------
    /**
     * Creates a limiter with specified parameters.
     *
     * @param rate Sample rate in Hz (default: SAMPLE_RATE from common.h)
     * @param thresh Amplitude threshold (0.0-1.0, default: 0.02)
     * @param attackMs Attack time in milliseconds (default: 5.0)
     * @param releaseMs Release time in milliseconds (default: 100.0)
     */
    explicit Limiter(unsigned int rate = SAMPLE_RATE,
                     float thresh = 0.02f,
                     float attackMs = 5.0f,
                     float releaseMs = 100.0f);

    ~Limiter() = default;

    //--------------------------------------------------------------------------
    // Audio Processing Interface
    //--------------------------------------------------------------------------
    /**
     * Processes audio through the limiter.
     *
     * @param inputBuffer Source audio samples
     * @param outputBuffer Destination for processed samples
     * @param bufferSize Number of samples to process
     */
    void process(const float* inputBuffer, float* outputBuffer, std::size_t bufferSize);

    //--------------------------------------------------------------------------
    // Limiter Controls
    //--------------------------------------------------------------------------
    /**
     * Sets the amplitude threshold.
     * @param newThreshold Threshold value (0.0-1.0)
     */
    void setThreshold(float newThreshold);

    /**
     * Gets the current threshold setting.
     * @return Current threshold value
     */
    float getThreshold() const;

    /**
     * Sets the attack time.
     * @param ms Attack time in milliseconds (minimum 0.1ms)
     */
    void setAttackTime(float ms);

    /**
     * Gets the current attack time.
     * @return Attack time in milliseconds
     */
    float getAttackTime() const;

    /**
     * Sets the release time.
     * @param ms Release time in milliseconds (minimum 1.0ms)
     */
    void setReleaseTime(float ms);

    /**
     * Gets the current release time.
     * @return Release time in milliseconds
     */
    float getReleaseTime() const;

    /**
     * Enables or disables the limiter.
     * @param isEnabled true to enable, false to disable
     */
    void setEnabled(bool isEnabled);

    /**
     * Checks if the limiter is currently enabled.
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const;
};

} // namespace audio

#endif // LIMITER_H
