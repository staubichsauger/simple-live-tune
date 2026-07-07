#pragma once

#include <cstddef>
#include <vector>

namespace simplelivetune::dsp {

// Windowed-sinc FIR decimator for the analysis path. Coefficients are
// designed in prepare(); processSample() is allocation-free and only
// evaluates the filter on emitted (decimated) samples.
class Downsampler {
public:
    void prepare(double inputSampleRate, double outputSampleRate = 12000.0);
    void reset();
    bool processSample(float input, float& output);
    void process(const float* input, int numSamples, std::vector<float>& output);

    [[nodiscard]] double inputSampleRate() const { return inputSampleRate_; }
    [[nodiscard]] double outputSampleRate() const { return outputSampleRate_; }
    [[nodiscard]] int factor() const { return factor_; }

private:
    double inputSampleRate_ = 48000.0;
    double outputSampleRate_ = 12000.0;
    int factor_ = 4;
    int phase_ = 0;
    std::vector<float> taps_;
    std::vector<float> history_;
    std::size_t historyIndex_ = 0;
};

} // namespace simplelivetune::dsp
