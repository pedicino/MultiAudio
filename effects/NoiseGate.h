#ifndef NOISE_GATE_H
#define NOISE_GATE_H

#include "AudioEffect.h"
#include "../common.h"

#include <vector>
#include <fftw3.h>

namespace audio {

// Small constant to prevent division by zero in coefficient calculation
constexpr float NG_TIME_EPSILON = 1e-6f;

/**
 * Spectral noise gate with attack/release smoothing.
 *
 * Analyzes audio in the frequency domain to detect signal presence
 * and applies smooth gain transitions based on configurable threshold.
 */
class NoiseGate : public AudioEffect
{
private:
    //--------------------------------------------------------------------------
    // Configuration
    //--------------------------------------------------------------------------
    unsigned int fftSize;
    float threshold;
    float attackTimeMs;
    float releaseTimeMs;
    float attackCoeff;
    float releaseCoeff;

    //--------------------------------------------------------------------------
    // FFTW Resources
    //--------------------------------------------------------------------------
    fftw_plan fftPlan;
    double* timeData;
    fftw_complex* frequencyData;

    //--------------------------------------------------------------------------
    // Internal State
    //--------------------------------------------------------------------------
    std::vector<double> bandEnergies;
    float currentGain;

    //--------------------------------------------------------------------------
    // Private Methods
    //--------------------------------------------------------------------------
    /**
     * Recalculates smoothing coefficients based on time settings.
     */
    void calculateCoeffs();

    /**
     * Calculates energy distribution across frequency bands.
     */
    void calculateBandEnergies();

    /**
     * Determines if signal exceeds threshold.
     * @param inputBuffer Audio data to analyze
     * @param numFrames Number of samples to process
     * @return 1.0f if signal exceeds threshold, 0.0f otherwise
     */
    float determineTargetGain(const float* inputBuffer, std::size_t numFrames);

public:
    //--------------------------------------------------------------------------
    // Lifecycle
    //--------------------------------------------------------------------------
    /**
     * Creates a noise gate with specified parameters.
     *
     * @param rate Sample rate in Hz (default: SAMPLE_RATE)
     * @param size FFT size for spectral analysis (default: FFT_SIZE)
     * @param thresh Amplitude threshold (0.0-1.0, default: 0.1)
     * @param attackMs Attack time in milliseconds (default: 5.0)
     * @param releaseMs Release time in milliseconds (default: 50.0)
     */
    explicit NoiseGate(unsigned int rate = SAMPLE_RATE,
                       unsigned int size = FFT_SIZE,
                       float thresh = 0.1f,
                       float attackMs = 5.0f,
                       float releaseMs = 50.0f);

    /**
     * Cleans up FFTW resources.
     */
    ~NoiseGate() override;

    //--------------------------------------------------------------------------
    // AudioEffect Interface
    //--------------------------------------------------------------------------
    /**
     * Processes audio through the noise gate.
     * @param inputBuffer Input audio data
     * @param outputBuffer Output buffer for processed audio
     * @param numFrames Number of samples to process
     */
    void process(const float* inputBuffer, float* outputBuffer, std::size_t numFrames) override;

    /**
     * Resets internal state to default values.
     */
    void reset() override;

    //--------------------------------------------------------------------------
    // Noise Gate Controls
    //--------------------------------------------------------------------------
    /**
     * Sets the gate threshold.
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
};

} // namespace audio

#endif // NOISE_GATE_H