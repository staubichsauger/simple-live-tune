#include "dsp/RatioSmoother.h"

#include <algorithm>
#include <cmath>

namespace simplelivetune::dsp {

void RatioSmoother::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    updateAlpha();
    reset();
}

void RatioSmoother::reset(float ratio)
{
    currentRatio_ = std::max(ratio, 0.01f);
    currentCents_ = 1200.0f * std::log2(currentRatio_);
    transitionActive_ = false;
}

void RatioSmoother::setRetuneTimeMs(float milliseconds)
{
    retuneTimeMs_ = std::max(0.0f, milliseconds);
    updateAlpha();
}

void RatioSmoother::setNoteTransitionTimeMs(float milliseconds)
{
    noteTransitionMs_ = std::max(0.0f, milliseconds);
    updateAlpha();
}

void RatioSmoother::setCorrectionAmount(float amount)
{
    correctionAmount_ = std::clamp(amount, 0.0f, 1.0f);
}

float RatioSmoother::alphaForTime(double sampleRate, float milliseconds)
{
    if (milliseconds <= 0.001f)
        return 1.0f;
    const auto tauSamples = sampleRate * static_cast<double>(milliseconds) * 0.001;
    return static_cast<float>(1.0 - std::exp(-1.0 / tauSamples));
}

void RatioSmoother::updateAlpha()
{
    alphaPerSample_ = alphaForTime(sampleRate_, retuneTimeMs_);
    transitionAlpha_ = alphaForTime(sampleRate_, noteTransitionMs_);
}

float RatioSmoother::process(float targetRatio, int numSamples)
{
    targetRatio = std::max(targetRatio, 0.01f);
    const auto targetCents = correctionAmount_ * 1200.0f * std::log2(targetRatio);

    auto alpha = transitionActive_ ? transitionAlpha_ : alphaPerSample_;
    if (alpha >= 1.0f) {
        currentCents_ = targetCents;
    } else {
        if (numSamples > 1)
            alpha = 1.0f - std::pow(1.0f - alpha, static_cast<float>(numSamples));
        currentCents_ += alpha * (targetCents - currentCents_);
    }

    if (transitionActive_ && std::abs(targetCents - currentCents_) < kConvergedCents)
        transitionActive_ = false;

    currentRatio_ = std::pow(2.0f, currentCents_ / 1200.0f);
    return currentRatio_;
}

} // namespace simplelivetune::dsp
