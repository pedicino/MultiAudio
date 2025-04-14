#include "DeEsser.h"

#include <cmath>
#include <fftw3.h>
#include <iostream>
#include <algorithm>

namespace audio {

//--------------------------------------------------------------------------
// De-Esser Processing
//--------------------------------------------------------------------------

/**
 * Applies de-essing effect to reduce sibilance in audio samples.
 *
 * @param samples Audio samples to process (modified in-place)
 * @param sampleRate Sample rate in Hz
 * @param startFreq Lower frequency bound for reduction (Hz)
 * @param endFreq Upper frequency bound for reduction (Hz)
 * @param reductionDB Amount of gain reduction in decibels
 */
void applyDeEsser(std::vector<double>& samples, int sampleRate,
                  int startFreq, int endFreq, double reductionDB)
{
    constexpr int FRAME_SIZE = 2048;

    if (samples.empty())
    {
        return;
    }

    // Convert dB reduction to linear gain multiplier
    double reduction = std::pow(10.0, -reductionDB / 20.0);

    // Allocate FFTW resources
    fftw_complex* in = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * FRAME_SIZE));
    fftw_complex* out = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * FRAME_SIZE));
    fftw_plan planF = fftw_plan_dft_1d(FRAME_SIZE, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan planI = fftw_plan_dft_1d(FRAME_SIZE, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);

    // Process audio frame-by-frame
    for (std::size_t i = 0; i < samples.size(); i += FRAME_SIZE)
    {
        std::size_t frameLength = std::min(static_cast<std::size_t>(FRAME_SIZE), samples.size() - i);

        // Load frame data and zero-pad if needed
        for (std::size_t j = 0; j < FRAME_SIZE; j++)
        {
            in[j][0] = (j < frameLength) ? samples[i + j] : 0.0;
            in[j][1] = 0.0;
        }

        // Forward FFT (time → frequency domain)
        fftw_execute(planF);

        // Apply frequency-selective gain reduction
        for (int j = 0; j < FRAME_SIZE / 2; j++)
        {
            double freq = static_cast<double>(j) * sampleRate / FRAME_SIZE;
            if (freq >= startFreq && freq <= endFreq)
            {
                // Apply to both positive and negative frequencies (complex conjugates)
                out[j][0] *= reduction;
                out[j][1] *= reduction;
                out[FRAME_SIZE - j][0] *= reduction;
                out[FRAME_SIZE - j][1] *= reduction;
            }
        }

        // Inverse FFT (frequency → time domain)
        fftw_execute(planI);

        // Normalize and store result
        for (std::size_t j = 0; j < frameLength; j++)
        {
            samples[i + j] = in[j][0] / FRAME_SIZE;
        }
    }

    // Clean up FFTW resources
    fftw_destroy_plan(planF);
    fftw_destroy_plan(planI);
    fftw_free(in);
    fftw_free(out);
}

} // namespace audio