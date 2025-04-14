#ifndef THREE_BAND_EQ_H
#define THREE_BAND_EQ_H

#include "AudioEffect.h"
#include "../common.h"

#include <vector>
#include <fftw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace audio {

/**
 * Three Band EQ with Overlap-Add processing.
 *
 * Implements spectral processing with separate gain control
 * for low, mid, and high frequency bands using FFT analysis.
 * Uses 50% overlap-add with Hann windowing to minimize artifacts.
 */
class ThreeBandEQ : public AudioEffect
{
private:
    //--------------------------------------------------------------------------
    // Configuration
    //--------------------------------------------------------------------------
    unsigned int fftSize;
    unsigned int hopSize;

    //--------------------------------------------------------------------------
    // FFTW Resources
    //--------------------------------------------------------------------------
    fftw_plan fftForwardPlan;
    fftw_plan fftInversePlan;
    double* timeData;
    fftw_complex* frequencyData;

    //--------------------------------------------------------------------------
    // EQ Parameters
    //--------------------------------------------------------------------------
    float bandCutoffs[NUM_EQ_BANDS];
    float bandGains[NUM_EQ_BANDS];

    //--------------------------------------------------------------------------
    // OLA Buffers & Window
    //--------------------------------------------------------------------------
    std::vector<double> window;
    std::vector<double> inputBufferInternal;
    std::vector<double> outputOverlapBuffer;

    //--------------------------------------------------------------------------
    // Private Methods
    //--------------------------------------------------------------------------
    /**
     * Applies EQ gain to frequency-domain data.
     */
    void applyEQGain();

    /**
     * Calculates Hann window function for 50% overlap.
     */
    void calculateWindow();

    /**
     * Gets interpolated gain for smooth transitions between bands.
     * @param frequency Frequency in Hz to get gain for
     * @return Interpolated gain value
     */
    float getSmoothGain(float frequency);

public:
    //--------------------------------------------------------------------------
    // Lifecycle
    //--------------------------------------------------------------------------
    /**
     * Creates a three-band equalizer with FFT processing.
     * @param rate Sample rate in Hz (default: SAMPLE_RATE)
     * @param frameSize Processing frame size (default: FRAMES_PER_BUFFER)
     */
    ThreeBandEQ(unsigned int rate = SAMPLE_RATE,
                unsigned int frameSize = FRAMES_PER_BUFFER);

    /**
     * Cleans up FFTW resources.
     */
    ~ThreeBandEQ() override;

    //--------------------------------------------------------------------------
    // AudioEffect Interface
    //--------------------------------------------------------------------------
    /**
     * Processes audio through the three-band equalizer.
     * @param inputBuffer Source audio samples
     * @param outputBuffer Destination for processed audio
     * @param numFrames Number of frames to process
     */
    void process(const float* inputBuffer, float* outputBuffer, std::size_t numFrames) override;

    /**
     * Resets internal state.
     */
    void reset() override;

    //--------------------------------------------------------------------------
    // EQ Controls
    //--------------------------------------------------------------------------
    /**
     * Sets the gain for a frequency band.
     * @param bandIndex Band to adjust (0=low, 1=mid, 2=high)
     * @param gain Gain multiplier (0.0-6.0)
     */
    void setBandGain(unsigned int bandIndex, float gain);

    /**
     * Gets the current gain for a frequency band.
     * @param bandIndex Band to query
     * @return Current gain value
     */
    float getBandGain(unsigned int bandIndex) const;

    /**
     * Sets the cutoff frequency between bands.
     * @param bandIndex Band cutoff to adjust (0=low-mid, 1=mid-high)
     * @param frequency Cutoff frequency in Hz
     */
    void setBandCutoff(unsigned int bandIndex, float frequency);

    /**
     * Gets the current cutoff frequency.
     * @param bandIndex Band cutoff to query
     * @return Current cutoff frequency in Hz
     */
    float getBandCutoff(unsigned int bandIndex) const;
};

} // namespace audio

#endif // THREE_BAND_EQ_H
