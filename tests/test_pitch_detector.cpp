#include "dsp/PitchDetector.h"
#include "test_common.h"

#include <cmath>
#include <vector>

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
}
