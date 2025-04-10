#ifndef DEESSER_H
#define DEESSER_H

#include <vector>

void applyDeEsser(std::vector<double>& samples, int sampleRate, int startFreq, int endFreq, double reductionDB);

#endif // DEESSER_H
