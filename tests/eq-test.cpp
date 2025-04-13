#include <fftw3.h>
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

std::tuple<double, double, double> band_energy(const float* frame, int sampleRate) {
    fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (FRAME_SIZE / 2 + 1));
    double* in = (double*)fftw_malloc(sizeof(double) * FRAME_SIZE);

    for (int i = 0; i < FRAME_SIZE; ++i)
        in[i] = static_cast<double>(frame[i]);

    fftw_plan plan = fftw_plan_dft_r2c_1d(FRAME_SIZE, in, out, FFTW_ESTIMATE);
    fftw_execute(plan);

    double low = 0.0, mid = 0.0, high = 0.0;
    for (int i = 0; i <= FRAME_SIZE / 2; ++i) {
        double freq = (i * sampleRate) / static_cast<double>(FRAME_SIZE);
        double mag_sq = out[i][0]*out[i][0] + out[i][1]*out[i][1];

        if (freq <= 400)
            low += mag_sq;
        else if (freq <= 4000)
            mid += mag_sq;
        else
            high += mag_sq;
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return {low, mid, high};
}

int main() {
    std::vector<float> input, output;
    int sr_in = 0, sr_out = 0;

    if (!load_audio("eq-input.wav", input, sr_in) || 
        !load_audio("eq_output.wav", output, sr_out) || 
        sr_in != sr_out) {
        std::cerr << "File loading or sample rate mismatch.\n";
        return 1;
    }

    std::ofstream csv("eq_analysis.csv");
    csv << "Time (s),Low_Before,Low_After,Mid_Before,Mid_After,High_Before,High_After\n";

    size_t frame_count = std::min(input.size(), output.size()) / FRAME_SIZE;
    for (size_t i = 0; i < frame_count; ++i) {
        const float* in_ptr = &input[i * FRAME_SIZE];
        const float* out_ptr = &output[i * FRAME_SIZE];

        auto [low_in, mid_in, high_in] = band_energy(in_ptr, sr_in);
        auto [low_out, mid_out, high_out] = band_energy(out_ptr, sr_in);

        float time = static_cast<float>(i * FRAME_SIZE) / sr_in;
        csv << time << "," << low_in << "," << low_out << ","
                     << mid_in << "," << mid_out << ","
                     << high_in << "," << high_out << "\n";
    }

    csv.close();
    std::cout << "Exported eq_analysis.csv\n";
    return 0;
}
