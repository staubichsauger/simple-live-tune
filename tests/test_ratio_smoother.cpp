#include "dsp/RatioSmoother.h"
#include "test_common.h"

#include <cmath>

namespace {
float cents(float ratio)
{
    return 1200.0f * std::log2(ratio);
}

float processFor(simplelivetune::dsp::RatioSmoother& smoother, float targetRatio, int numSamples)
{
    auto ratio = 1.0f;
    for (int i = 0; i < numSamples; ++i)
        ratio = smoother.process(targetRatio, 1);
    return ratio;
}
}

int main()
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto tenMs = 480;
    const auto semitoneUp = std::exp2(100.0f / 1200.0f);

    simplelivetune::dsp::RatioSmoother smoother;
    smoother.prepare(sampleRate);
    smoother.setRetuneTimeMs(200.0f);
    smoother.setNoteTransitionTimeMs(5.0f);

    // Without a note transition, the slow retune speed applies:
    // 10 ms with tau = 200 ms covers ~5% of the 100-cent step.
    auto ratio = processFor(smoother, semitoneUp, tenMs);
    FT_EXPECT_TRUE(cents(ratio) < 20.0f);

    // With a note transition, the fast time constant applies:
    // 10 ms with tau = 5 ms covers ~86% of the step.
    smoother.reset();
    smoother.beginNoteTransition();
    ratio = processFor(smoother, semitoneUp, tenMs);
    FT_EXPECT_TRUE(cents(ratio) > 60.0f);

    // Once converged, the smoother must fall back to the slow retune speed
    // for in-note drift: a subsequent 30-cent target move should barely move
    // within 10 ms.
    // (the last ~10 cents are covered at the slow retune speed, tau = 200 ms)
    ratio = processFor(smoother, semitoneUp, tenMs * 100); // 1 s: fully converge
    FT_EXPECT_NEAR(cents(ratio), 100.0f, 1.0f);
    const auto newTarget = std::exp2(130.0f / 1200.0f);
    ratio = processFor(smoother, newTarget, tenMs);
    FT_EXPECT_TRUE(cents(ratio) < 105.0f);

    // Zero retune time still snaps instantly regardless of transition state.
    smoother.reset();
    smoother.setRetuneTimeMs(0.0f);
    ratio = smoother.process(semitoneUp, 1);
    FT_EXPECT_NEAR(cents(ratio), 100.0f, 0.01f);
}
