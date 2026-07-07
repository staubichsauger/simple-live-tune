#include "dsp/PitchDetector.h"
#include "dsp/PitchShifter.h"
#include "test_common.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr auto sampleRate = 48000.0;
constexpr auto inputHz = 220.0f;
constexpr auto periodSamples = static_cast<float>(sampleRate / inputHz);

std::vector<float> makeSine(float frequency, double seconds, float amplitude)
{
    std::vector<float> signal(static_cast<std::size_t>(sampleRate * seconds));
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = amplitude * static_cast<float>(std::sin(2.0 * M_PI * frequency * static_cast<double>(i) / sampleRate));
    return signal;
}
}

int main()
{
    using simplelivetune::dsp::PitchShifter;

    const auto input = makeSine(inputHz, 1.0, 0.5f);
    std::vector<float> output(input.size());

    // 1. Shift ratio accuracy, delay bound, and click-freeness.
    PitchShifter shifter;
    shifter.prepare(sampleRate);
    shifter.setPitchPeriodSamples(periodSamples);
    shifter.setRatio(1.25f);

    auto maxDelay = 0.0f;
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = shifter.processSample(input[i]);
        maxDelay = std::max(maxDelay, shifter.currentDelaySamples());
    }

    simplelivetune::dsp::PitchDetector detector;
    detector.prepare(sampleRate);
    const auto offset = static_cast<int>(sampleRate * 0.25);
    const auto result = detector.detect(output.data() + offset, static_cast<int>(sampleRate * 0.08));
    FT_EXPECT_TRUE(result.voiced);
    FT_EXPECT_NEAR(centsBetween(result.f0Hz, inputHz * 1.25f), 0.0f, 35.0f);

    // Latency bound: the read tap must stay within one pitch period plus the
    // crossfade guard of the write head.
    const auto fadeGuard = 0.25f * static_cast<float>(static_cast<int>(sampleRate * 0.003));
    FT_EXPECT_TRUE(maxDelay <= PitchShifter::kMinDelaySamples + periodSamples + fadeGuard + 2.0f);

    // Click detection: sample-to-sample delta bounded by ~3x the analytic
    // slope of the shifted sine.
    const auto maxSlope = 0.5f * 2.0f * static_cast<float>(M_PI) * inputHz * 1.25f / static_cast<float>(sampleRate);
    auto maxDiff = 0.0f;
    for (std::size_t i = static_cast<std::size_t>(sampleRate * 0.1); i < output.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(output[i] - output[i - 1]));
    FT_EXPECT_TRUE(maxDiff <= 3.0f * maxSlope);

    // 2. Ratio 1 is an exact kMinDelaySamples-sample delay (zero added latency
    //    beyond the interpolator lookahead) with no discontinuities.
    shifter.reset();
    shifter.setRatio(1.0f);
    shifter.process(input.data(), output.data(), 4096);
    const auto lag = static_cast<std::size_t>(PitchShifter::kMinDelaySamples);
    for (std::size_t i = lag + 1; i < 4096; ++i)
        FT_EXPECT_NEAR(output[i], input[i - lag], 1.0e-6f);
}
