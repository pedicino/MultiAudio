#ifndef DEESSER_H
#define DEESSER_H

#include <vector>

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
                  int startFreq, int endFreq, double reductionDB);

} // namespace audio

#endif // DEESSER_H
