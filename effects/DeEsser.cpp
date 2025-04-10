#include "deesser.h" 
#include <cmath>
#include <fftw3.h>

#define FRAME_SIZE 2048

// DeEsser function to apply a de-esser effect on the audio samples
// This function uses FFTW for fast Fourier transform and applies a reduction in the specified frequency range
void applyDeEsser(std::vector<double>& samples, int sampleRate, int startFreq, int endFreq, double reductionDB) {
    // Check if the input samples are empty
    if (samples.empty()) {
        return;
    }
    size_t numFrames = samples.size() / FRAME_SIZE;
    double reduction = pow(10, -reductionDB / 20.0);

    // Allocate memory for FFTW input and output arrays
    fftw_complex *in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FRAME_SIZE);
    fftw_complex *out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FRAME_SIZE);
    fftw_plan planF = fftw_plan_dft_1d(FRAME_SIZE, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan planI = fftw_plan_dft_1d(FRAME_SIZE, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);

    // Process each frame of audio samples
    for (size_t i = 0; i < samples.size(); i += FRAME_SIZE) {
        size_t frameLength = std::min((size_t)FRAME_SIZE, samples.size() - i);

        // Fill the input array with the current frame of samples
        for (size_t j = 0; j < FRAME_SIZE; j++) {
            in[j][0] = (j < frameLength) ? samples[i + j] : 0.0;
            in[j][1] = 0.0;
        }

        // Execute the FFT to get the frequency domain representation
        fftw_execute(planF);

        // Apply reduction to the specified frequency range
        for (int j = 0; j < FRAME_SIZE / 2; j++) {
            double freq = j * (double)sampleRate / FRAME_SIZE;
            if (freq >= startFreq && freq <= endFreq) {
                out[j][0] *= reduction;
                out[j][1] *= reduction;
                out[FRAME_SIZE - j][0] *= reduction;
                out[FRAME_SIZE - j][1] *= reduction;
            }
        }

        // Execute the inverse FFT to get the modified samples back
        fftw_execute(planI);

        // Normalize the output samples and store them back in the original array
        for (size_t j = 0; j < frameLength; j++) {
            samples[i + j] = in[j][0] / FRAME_SIZE;
        }
    }

    fftw_destroy_plan(planF);
    fftw_destroy_plan(planI);
    fftw_free(in);
    fftw_free(out);
}