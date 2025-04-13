#include <sndfile.h>
#include <vector>
#include <fstream>
#include <cmath>
#include <iostream>

#define FRAME_SIZE 2048

bool load_audio(const char* filename, std::vector<float>& data, int& sampleRate) {
    SF_INFO sfinfo;
    SNDFILE* file = sf_open(filename, SFM_READ, &sfinfo);
    if (!file) {
        std::cerr << "Error reading file: " << filename << std::endl;
        return false;
    }

    sampleRate = sfinfo.samplerate;
    data.resize(sfinfo.frames);
    sf_read_float(file, data.data(), sfinfo.frames);
    sf_close(file);
    return true;
}

float compute_rms(const float* frame, int size) {
    double sum_sq = 0.0;
    for (int i = 0; i < size; ++i)
        sum_sq += frame[i] * frame[i];
    return std::sqrt(sum_sq / size);
}

int main() {
    std::vector<float> input, output;
    int sr_in = 0, sr_out = 0;

    if (!load_audio("noisegate_input.wav", input, sr_in) || 
        !load_audio("noisegate_output.wav", output, sr_out) || 
        sr_in != sr_out) {
        std::cerr << "File loading or sample rate mismatch.\n";
        return 1;
    }

    std::ofstream csv("noisegate_analysis.csv");
    csv << "Time (s),RMS_Before,RMS_After\n";

    size_t frame_count = std::min(input.size(), output.size()) / FRAME_SIZE;
    for (size_t i = 0; i < frame_count; ++i) {
        const float* in_ptr = &input[i * FRAME_SIZE];
        const float* out_ptr = &output[i * FRAME_SIZE];

        float rms_in = compute_rms(in_ptr, FRAME_SIZE);
        float rms_out = compute_rms(out_ptr, FRAME_SIZE);
        float time = static_cast<float>(i * FRAME_SIZE) / sr_in;

        csv << time << "," << rms_in << "," << rms_out << "\n";
    }

    csv.close();
    std::cout << "Exported noisegate_analysis.csv\n";
    return 0;
}
