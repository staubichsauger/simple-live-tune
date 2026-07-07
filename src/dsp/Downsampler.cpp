#include "dsp/Downsampler.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace simplelivetune::dsp {

void Downsampler::prepare(double inputSampleRate, double outputSampleRate)
{
    inputSampleRate_ = std::max(inputSampleRate, 1.0);
    const auto requestedOut = std::clamp(outputSampleRate, 1.0, inputSampleRate_);
    factor_ = std::max(1, static_cast<int>(std::lround(inputSampleRate_ / requestedOut)));
    outputSampleRate_ = inputSampleRate_ / static_cast<double>(factor_);

    if (factor_ == 1) {
        taps_.assign(1, 1.0f);
    } else {
        // Windowed-sinc (Blackman) lowpass at 0.45x the output Nyquist.
        constexpr auto pi = std::numbers::pi;
        const auto numTaps = 24 * factor_ + 1;
        const auto mid = static_cast<double>(numTaps - 1) / 2.0;
        const auto fc = 0.45 * 0.5 / static_cast<double>(factor_); // cycles per input sample
        taps_.assign(static_cast<std::size_t>(numTaps), 0.0f);
        double sum = 0.0;
        for (int n = 0; n < numTaps; ++n) {
            const auto x = 2.0 * pi * fc * (static_cast<double>(n) - mid);
            const auto sinc = std::abs(x) < 1.0e-12 ? 1.0 : std::sin(x) / x;
            const auto t = static_cast<double>(n) / static_cast<double>(numTaps - 1);
            const auto window = 0.42 - 0.5 * std::cos(2.0 * pi * t) + 0.08 * std::cos(4.0 * pi * t);
            const auto h = 2.0 * fc * sinc * window;
            taps_[static_cast<std::size_t>(n)] = static_cast<float>(h);
            sum += h;
        }
        for (auto& tap : taps_)
            tap = static_cast<float>(tap / sum); // unity DC gain
    }

    history_.assign(taps_.size(), 0.0f);
    reset();
}

void Downsampler::reset()
{
    std::fill(history_.begin(), history_.end(), 0.0f);
    historyIndex_ = 0;
    phase_ = 0;
}

bool Downsampler::processSample(float input, float& output)
{
    history_[historyIndex_] = input;

    const auto emit = phase_ == 0;
    if (emit) {
        auto acc = 0.0f;
        auto index = historyIndex_;
        const auto size = history_.size();
        for (std::size_t k = 0; k < taps_.size(); ++k) {
            acc += taps_[k] * history_[index];
            index = index == 0 ? size - 1 : index - 1;
        }
        output = acc;
    }

    historyIndex_ = (historyIndex_ + 1) % history_.size();
    phase_ = (phase_ + 1) % factor_;
    return emit;
}

void Downsampler::process(const float* input, int numSamples, std::vector<float>& output)
{
    output.clear();
    if (numSamples <= 0)
        return;

    output.reserve(static_cast<std::size_t>(numSamples / factor_ + 1));

    for (int i = 0; i < numSamples; ++i) {
        float sample = 0.0f;
        if (processSample(input[i], sample))
            output.push_back(sample);
    }
}

} // namespace simplelivetune::dsp
