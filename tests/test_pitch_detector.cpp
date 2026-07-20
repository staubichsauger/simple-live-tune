#include "dsp/PitchDetector.h"
#include "test_common.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
void testIncrementalMatchesFullRecomputation()
{
    constexpr auto sampleRate = 12000.0;
    constexpr auto windowSize = 480;
    constexpr auto advance = 16;
    constexpr auto durationSeconds = 5.0;

    std::vector<float> signal(static_cast<std::size_t>(sampleRate * durationSeconds));
    auto phase = 0.0;
    for (std::size_t i = 0; i < signal.size(); ++i) {
        const auto t = static_cast<double>(i) / sampleRate;
        const auto frequency = 210.0 + 35.0 * std::sin(2.0 * M_PI * 0.7 * t);
        phase += 2.0 * M_PI * frequency / sampleRate;
        signal[i] = 0.42f * static_cast<float>(std::sin(phase))
            + 0.08f * static_cast<float>(std::sin(2.0 * phase));
    }

    simplelivetune::dsp::PitchDetector full;
    simplelivetune::dsp::PitchDetector incremental;
    full.prepare(sampleRate, 70.0f, 1000.0f, windowSize);
    incremental.prepare(sampleRate, 70.0f, 1000.0f, windowSize);

    auto comparedFrames = 0;
    for (std::size_t start = 0; start + windowSize <= signal.size(); start += advance) {
        const auto* window = signal.data() + start;
        const auto expected = full.detect(window, windowSize);
        const auto actual = incremental.detectIncremental(
            window, windowSize, start == 0 ? windowSize : advance);
        FT_EXPECT_TRUE(actual.voiced == expected.voiced);
        if (!expected.voiced)
            continue;
        FT_EXPECT_NEAR(centsBetween(actual.f0Hz, expected.f0Hz), 0.0f, 0.02f);
        FT_EXPECT_NEAR(actual.confidence, expected.confidence, 1.0e-5f);
        ++comparedFrames;
    }
    FT_EXPECT_TRUE(comparedFrames > 3000);
}

void testIncrementalLongRunDrift()
{
    constexpr auto sampleRate = 12000.0;
    constexpr auto windowSize = 480;
    constexpr auto advance = 16;
    constexpr auto durationSeconds = 20.0;

    std::vector<float> signal(static_cast<std::size_t>(sampleRate * durationSeconds));
    for (std::size_t i = 0; i < signal.size(); ++i) {
        const auto t = static_cast<double>(i) / sampleRate;
        signal[i] = 0.4f * static_cast<float>(std::sin(2.0 * M_PI * 233.0 * t));
    }

    simplelivetune::dsp::PitchDetector incremental;
    incremental.prepare(sampleRate, 70.0f, 1000.0f, windowSize);

    auto hop = 0;
    for (std::size_t start = 0; start + windowSize <= signal.size(); start += advance, ++hop) {
        const auto* window = signal.data() + start;
        const auto actual = incremental.detectIncremental(
            window, windowSize, start == 0 ? windowSize : advance);
        if (hop % 257 != 0)
            continue;

        simplelivetune::dsp::PitchDetector reference;
        reference.prepare(sampleRate, 70.0f, 1000.0f, windowSize);
        const auto expected = reference.detect(window, windowSize);
        FT_EXPECT_TRUE(actual.voiced == expected.voiced);
        if (expected.voiced) {
            FT_EXPECT_NEAR(centsBetween(actual.f0Hz, expected.f0Hz), 0.0f, 0.02f);
            FT_EXPECT_NEAR(actual.confidence, expected.confidence, 1.0e-5f);
        }
    }
}
}

int main()
{
    constexpr auto sampleRate = 48000.0;
    simplelivetune::dsp::PitchDetector detector;
    detector.prepare(sampleRate);

    for (auto frequency : { 82.41f, 110.0f, 220.0f, 440.0f, 880.0f }) {
        std::vector<float> signal(static_cast<std::size_t>(sampleRate * 0.05));
        for (std::size_t i = 0; i < signal.size(); ++i)
            signal[i] = std::sin(2.0 * M_PI * frequency * static_cast<double>(i) / sampleRate);

        const auto result = detector.detect(signal.data(), static_cast<int>(signal.size()));
        FT_EXPECT_TRUE(result.voiced);
        FT_EXPECT_TRUE(result.confidence > 0.9f);
        FT_EXPECT_NEAR(centsBetween(result.f0Hz, frequency), 0.0f, 5.0f);
    }

    std::vector<float> silence(2400, 0.0f);
    const auto silent = detector.detect(silence.data(), static_cast<int>(silence.size()));
    FT_EXPECT_TRUE(!silent.voiced);

    // A very quiet tone (-60 dBFS) is below the default -45 dB gate and must
    // stay unvoiced despite being perfectly periodic; lowering the gate
    // lets it through again.
    std::vector<float> quiet(static_cast<std::size_t>(sampleRate * 0.05));
    for (std::size_t i = 0; i < quiet.size(); ++i)
        quiet[i] = 0.001f * std::sin(2.0 * M_PI * 220.0 * static_cast<double>(i) / sampleRate);
    FT_EXPECT_TRUE(!detector.detect(quiet.data(), static_cast<int>(quiet.size())).voiced);
    detector.setMinimumRmsLevel(0.0001f); // -80 dB
    FT_EXPECT_TRUE(detector.detect(quiet.data(), static_cast<int>(quiet.size())).voiced);

    testIncrementalMatchesFullRecomputation();
    testIncrementalLongRunDrift();
}
