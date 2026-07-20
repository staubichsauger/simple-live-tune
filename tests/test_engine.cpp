#include "engine/TuneEngine.h"
#include "test_common.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
constexpr auto sampleRate = 48000.0;

std::vector<float> makeSine(float frequency, double seconds, float amplitude)
{
    std::vector<float> signal(static_cast<std::size_t>(sampleRate * seconds));
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = amplitude * static_cast<float>(std::sin(2.0 * M_PI * frequency * static_cast<double>(i) / sampleRate));
    return signal;
}

void testCorrectionAndClicks()
{
    constexpr auto detunedA = 430.0f;
    const auto input = makeSine(detunedA, 1.2, 0.4f);
    std::vector<float> output(input.size());

    simplelivetune::engine::TuneEngine engine;
    engine.prepare(sampleRate, 64);
    simplelivetune::engine::TuneParameters params;
    params.retuneTimeMs = 0.0f;
    params.scale = simplelivetune::dsp::ScaleType::Chromatic;
    engine.setParameters(params);

    for (std::size_t pos = 0; pos < input.size(); pos += 37) {
        const auto count = static_cast<int>(std::min<std::size_t>(37, input.size() - pos));
        engine.process(input.data() + pos, output.data() + pos, count);
    }

    FT_EXPECT_TRUE(engine.lastDetectedPitchHz() > 420.0f);

    simplelivetune::dsp::PitchDetector detector;
    detector.prepare(sampleRate);
    const auto offset = static_cast<int>(sampleRate * 0.6);
    const auto result = detector.detect(output.data() + offset, static_cast<int>(sampleRate * 0.08));
    FT_EXPECT_TRUE(result.voiced);
    FT_EXPECT_NEAR(centsBetween(result.f0Hz, 440.0f), 0.0f, 45.0f);

    // No clicks, including when correction first engages: sample-to-sample
    // delta bounded by ~3x the analytic slope of the corrected sine.
    const auto maxSlope = 0.4f * 2.0f * static_cast<float>(M_PI) * 440.0f / static_cast<float>(sampleRate);
    auto maxDiff = 0.0f;
    for (std::size_t i = static_cast<std::size_t>(sampleRate * 0.02); i < output.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(output[i] - output[i - 1]));
    FT_EXPECT_TRUE(maxDiff <= 3.0f * maxSlope);
}

void testZeroLatencyOnUnvoiced()
{
    // White noise stays unvoiced, so the ratio stays 1 and the engine must be
    // an exact 2-sample delay (interpolator lookahead only) - no buffering.
    std::uint32_t state = 123456789u;
    std::vector<float> input(static_cast<std::size_t>(sampleRate * 0.5));
    for (auto& sample : input) {
        state = state * 1664525u + 1013904223u;
        sample = (static_cast<float>(state >> 8) / 8388608.0f - 1.0f) * 0.3f;
    }

    std::vector<float> output(input.size());
    simplelivetune::engine::TuneEngine engine;
    engine.prepare(sampleRate, 64);
    engine.setParameters({});
    engine.process(input.data(), output.data(), static_cast<int>(input.size()));

    const auto lag = static_cast<std::size_t>(simplelivetune::dsp::PitchShifter::kMinDelaySamples);
    for (std::size_t i = lag + 1; i < input.size(); ++i)
        FT_EXPECT_NEAR(output[i], input[i - lag], 1.0e-5f);
}

// Sings 430 Hz with +/-40 cent 6 Hz vibrato. With vibrato preservation off
// the output pitch should wobble much less than with it fully on.
float measureOutputPitchSpread(float vibratoAmount)
{
    constexpr auto carrierHz = 430.0;
    constexpr auto vibratoHz = 6.0;
    constexpr auto vibratoDepthCents = 40.0;

    std::vector<float> input(static_cast<std::size_t>(sampleRate * 1.4));
    double phase = 0.0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto t = static_cast<double>(i) / sampleRate;
        const auto hz = carrierHz * std::exp2((vibratoDepthCents / 1200.0) * std::sin(2.0 * M_PI * vibratoHz * t));
        phase += 2.0 * M_PI * hz / sampleRate;
        input[i] = 0.4f * static_cast<float>(std::sin(phase));
    }

    std::vector<float> output(input.size());
    simplelivetune::engine::TuneEngine engine;
    engine.prepare(sampleRate, 64);
    simplelivetune::engine::TuneParameters params;
    params.retuneTimeMs = 0.0f;
    params.vibratoAmount = vibratoAmount;
    engine.setParameters(params);
    engine.process(input.data(), output.data(), static_cast<int>(input.size()));

    simplelivetune::dsp::PitchDetector detector;
    detector.prepare(sampleRate);
    auto minCents = 1.0e9f;
    auto maxCents = -1.0e9f;
    for (auto start = 0.4; start + 0.06 < 1.4; start += 0.05) {
        const auto offset = static_cast<int>(sampleRate * start);
        const auto result = detector.detect(output.data() + offset, static_cast<int>(sampleRate * 0.06));
        if (!result.voiced)
            continue;
        const auto c = centsBetween(result.f0Hz, 440.0f);
        minCents = std::min(minCents, c);
        maxCents = std::max(maxCents, c);
    }
    FT_EXPECT_TRUE(maxCents >= minCents); // at least one voiced window
    return maxCents - minCents;
}

void testVibratoPreservation()
{
    const auto spreadFlattened = measureOutputPitchSpread(0.0f);
    const auto spreadPreserved = measureOutputPitchSpread(1.0f);
    FT_EXPECT_TRUE(spreadPreserved > spreadFlattened + 10.0f);
}

// 430 Hz is ~-40 cents from A4. With a 10-cent tolerance the output should
// land at the tolerance edge (-10 ct), and with a 50-cent tolerance the
// input is inside the zone and must pass through untouched.
float measureOutputCents(float toleranceCents)
{
    const auto input = makeSine(430.0f, 1.0, 0.4f);
    std::vector<float> output(input.size());

    simplelivetune::engine::TuneEngine engine;
    engine.prepare(sampleRate, 64);
    simplelivetune::engine::TuneParameters params;
    params.retuneTimeMs = 0.0f;
    params.toleranceCents = toleranceCents;
    engine.setParameters(params);
    engine.process(input.data(), output.data(), static_cast<int>(input.size()));

    simplelivetune::dsp::PitchDetector detector;
    detector.prepare(sampleRate);
    const auto offset = static_cast<int>(sampleRate * 0.6);
    const auto result = detector.detect(output.data() + offset, static_cast<int>(sampleRate * 0.08));
    FT_EXPECT_TRUE(result.voiced);
    return centsBetween(result.f0Hz, 440.0f);
}

void testTuneTolerance()
{
    FT_EXPECT_NEAR(measureOutputCents(0.0f), 0.0f, 6.0f);
    FT_EXPECT_NEAR(measureOutputCents(10.0f), -10.0f, 6.0f);
    FT_EXPECT_NEAR(measureOutputCents(50.0f), -40.0f, 6.0f);
}

void testBlockSizeIndependence()
{
    const auto input = makeSine(435.0f, 0.6, 0.4f);
    std::vector<float> outputA(input.size());
    std::vector<float> outputB(input.size());

    simplelivetune::engine::TuneEngine engineA;
    simplelivetune::engine::TuneEngine engineB;
    engineA.prepare(sampleRate, 512);
    engineB.prepare(sampleRate, 512);
    simplelivetune::engine::TuneParameters params;
    params.retuneTimeMs = 25.0f;
    engineA.setParameters(params);
    engineB.setParameters(params);

    for (std::size_t pos = 0; pos < input.size(); ++pos)
        engineA.process(input.data() + pos, outputA.data() + pos, 1);

    for (std::size_t pos = 0; pos < input.size(); pos += 512) {
        const auto count = static_cast<int>(std::min<std::size_t>(512, input.size() - pos));
        engineB.process(input.data() + pos, outputB.data() + pos, count);
    }

    for (std::size_t i = 0; i < input.size(); ++i)
        FT_EXPECT_NEAR(outputA[i], outputB[i], 1.0e-9f);
}

void testRepeatedUnchangedParametersPreserveOutput()
{
    const auto input = makeSine(435.0f, 0.8, 0.4f);
    std::vector<float> outputA(input.size());
    std::vector<float> outputB(input.size());

    simplelivetune::engine::TuneParameters params;
    params.retuneTimeMs = 17.0f;
    params.noteTransitionMs = 43.0f;
    params.gateDb = -52.0f;

    simplelivetune::engine::TuneEngine engineA;
    simplelivetune::engine::TuneEngine engineB;
    engineA.prepare(sampleRate, 32);
    engineB.prepare(sampleRate, 32);
    engineA.setParameters(params);
    engineB.setParameters(params);

    for (std::size_t pos = 0; pos < input.size(); pos += 32) {
        const auto count = static_cast<int>(std::min<std::size_t>(32, input.size() - pos));
        engineA.process(input.data() + pos, outputA.data() + pos, count);
        engineB.setParameters(params);
        engineB.process(input.data() + pos, outputB.data() + pos, count);
    }

    for (std::size_t i = 0; i < input.size(); ++i)
        FT_EXPECT_NEAR(outputA[i], outputB[i], 1.0e-9f);
}
}

int main()
{
    testCorrectionAndClicks();
    testZeroLatencyOnUnvoiced();
    testVibratoPreservation();
    testTuneTolerance();
    testBlockSizeIndependence();
    testRepeatedUnchangedParametersPreserveOutput();
}
