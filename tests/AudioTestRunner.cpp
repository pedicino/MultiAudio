// AudioTestRunner.cpp
// A driver program to apply audio processors and log raw vs. processed RMS values to CSV
// Command to compile: g++ -std=c++17 -Ieffects tests/AudioTestRunner.cpp effects/DeEsser.cpp effects/Limiter.cpp effects/NoiseGate.cpp effects/ThreeBandEQ.cpp -lsndfile -lfftw3 -o audiotest
// Command to run: ./audiotest

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <sndfile.h>

// Include your processor headers here
#include "../effects/DeEsser.h"
#include "../effects/Limiter.h"
#include "../effects/NoiseGate.h"
#include "../effects/ThreeBandEQ.h"

float calculateRMS(const std::vector<float>& buffer) {
    double sumSquares = 0.0;
    for (float sample : buffer) {
        sumSquares += sample * sample;
    }
    return std::sqrt(sumSquares / buffer.size());
}

void writeCSV(const std::string& path, const std::vector<float>& rawRMS, const std::vector<float>& processedRMS) {
    std::ofstream file(path);
    file << "Frame,Raw RMS,Processed RMS\n";
    for (size_t i = 0; i < rawRMS.size(); ++i) {
        file << i << "," << rawRMS[i] << "," << processedRMS[i] << "\n";
    }
    file.close();
}

int main() {
    // === HARDCODED INPUT ===
    std::string inputPath = "tests/eq-input.wav";
    std::string outputPath = "tests/eq-output.wav";
    std::string effectType = "eq";  // Options: "deesser", "limiter", "noisegate", "eq"

    SF_INFO sfInfo;
    SNDFILE* inFile = sf_open(inputPath.c_str(), SFM_READ, &sfInfo);
    if (!inFile) {
        std::cerr << "Error reading input WAV file: " << inputPath << std::endl;
        return 1;
    }

    std::vector<float> inputBuffer(sfInfo.frames * sfInfo.channels);
    sf_readf_float(inFile, inputBuffer.data(), sfInfo.frames);
    sf_close(inFile);

    std::vector<float> outputBuffer(inputBuffer.size(), 0.0f);
    std::vector<float> rawRMS, processedRMS;

    const size_t frameSize = 2048;

    for (size_t i = 0; i < inputBuffer.size(); i += frameSize) {
        size_t end = std::min(i + frameSize, inputBuffer.size());
        std::vector<float> raw(inputBuffer.begin() + i, inputBuffer.begin() + end);
        std::vector<float> processed = raw;

        if (effectType == "deesser") {
            std::vector<double> samples(raw.begin(), raw.end());
            audio::applyDeEsser(samples, sfInfo.samplerate, 4000, 10000, 6.0);
            for (size_t j = 0; j < samples.size(); ++j) processed[j] = (float)samples[j];
        } else if (effectType == "limiter") {
            audio::Limiter lim(sfInfo.samplerate, 0.6f, 10.0f, 100.0f);
            lim.setEnabled(true);
            lim.process(raw.data(), processed.data(), raw.size());
        } else if (effectType == "noisegate") {
            audio::NoiseGate gate(sfInfo.samplerate, frameSize, 0.1f, 20.0f, 200.0f);
            gate.setEnabled(true);
            gate.process(raw.data(), processed.data(), raw.size());
        } else if (effectType == "eq") {
            audio::ThreeBandEQ eq(sfInfo.samplerate, frameSize);
            eq.setEnabled(true);
            eq.setBandGain(0, 1.5f); // bass
            eq.setBandGain(1, 0.8f); // mid
            eq.setBandGain(2, 1.2f); // treble
            eq.process(raw.data(), processed.data(), raw.size());
        }

        rawRMS.push_back(calculateRMS(raw));
        processedRMS.push_back(calculateRMS(processed));

        std::copy(processed.begin(), processed.end(), outputBuffer.begin() + i);
    }

    SF_INFO outInfo = sfInfo;
    SNDFILE* outFile = sf_open(outputPath.c_str(), SFM_WRITE, &outInfo);
    if (!outFile) {
        std::cerr << "Error writing output WAV file: " << outputPath << std::endl;
        return 1;
    }

    sf_writef_float(outFile, outputBuffer.data(), sfInfo.frames);
    sf_close(outFile);

    writeCSV("analysis.csv", rawRMS, processedRMS);
    std::cout << "Done. Output saved to " << outputPath << " and analysis to analysis.csv\n";
    return 0;
}