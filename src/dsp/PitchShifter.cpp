#include "dsp/PitchShifter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace simplelivetune::dsp {

void PitchShifter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    buffer_.assign(static_cast<std::size_t>(std::max(8192, static_cast<int>(sampleRate_ * 0.12))), 0.0f);
    maxDelaySamples_ = static_cast<float>(buffer_.size() - 4);
    maxFadeSamples_ = std::max(16, static_cast<int>(sampleRate_ * 0.003));
    reset();
}

void PitchShifter::reset()
{
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writeIndex_ = 0;
    ratio_ = 1.0f;
    periodSamples_ = static_cast<float>(sampleRate_ / 220.0);
    delay_ = kMinDelaySamples;
    fadeDelay_ = kMinDelaySamples;
    fadeLength_ = 0;
    fadeRemaining_ = 0;
}

void PitchShifter::setRatio(float ratio)
{
    ratio_ = std::clamp(ratio, 0.5f, 2.0f);
}

void PitchShifter::setPitchPeriodSamples(float periodSamples)
{
    periodSamples_ = std::clamp(periodSamples, 8.0f, std::min(2048.0f, maxDelaySamples_ * 0.5f));
}

float PitchShifter::readInterpolated(float delaySamples) const
{
    delaySamples = std::clamp(delaySamples, 1.0f, maxDelaySamples_);
    const auto readPos = static_cast<float>(writeIndex_) - delaySamples;
    auto base = static_cast<int>(std::floor(readPos));
    const auto frac = readPos - static_cast<float>(base);
    const auto size = static_cast<int>(buffer_.size());
    while (base < 0)
        base += size;

    const auto at = [&](int offset) {
        return buffer_[static_cast<std::size_t>((base + offset + size) % size)];
    };

    const auto y0 = at(-1);
    const auto y1 = at(0);
    const auto y2 = at(1);
    const auto y3 = at(2);
    const auto c0 = y1;
    const auto c1 = 0.5f * (y2 - y0);
    const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void PitchShifter::startCrossfade(float jumpAmount)
{
    const auto drift = std::abs(1.0f - ratio_);
    auto fadeLen = static_cast<float>(maxFadeSamples_);
    if (drift > 1.0e-4f)
        fadeLen = std::min(fadeLen, 0.45f * periodSamples_ / drift);
    fadeLength_ = std::max(8, static_cast<int>(fadeLen));
    fadeRemaining_ = fadeLength_;
    fadeDelay_ = delay_;
    delay_ += jumpAmount;
}

float PitchShifter::processSample(float input)
{
    if (buffer_.empty())
        return input;

    buffer_[static_cast<std::size_t>(writeIndex_)] = input;

    // The tap drifts at (1 - ratio) samples per sample; ratio == 1 freezes it,
    // so engaging/disengaging correction never causes a delay discontinuity.
    const auto drift = 1.0f - ratio_;
    delay_ += drift;
    if (fadeRemaining_ > 0)
        fadeDelay_ += drift;

    // Guard so the outgoing tap cannot drift past the write head (or too far
    // back) while a crossfade is still in progress.
    const auto fadeGuard = std::abs(drift) * static_cast<float>(maxFadeSamples_);
    if (delay_ < kMinDelaySamples + fadeGuard)
        startCrossfade(periodSamples_);
    else if (delay_ > kMinDelaySamples + periodSamples_ + fadeGuard)
        startCrossfade(-periodSamples_);

    auto out = readInterpolated(delay_);
    if (fadeRemaining_ > 0) {
        const auto progress = 1.0f - static_cast<float>(fadeRemaining_) / static_cast<float>(fadeLength_);
        const auto newWeight = 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * progress);
        out = newWeight * out + (1.0f - newWeight) * readInterpolated(fadeDelay_);
        --fadeRemaining_;
    }

    writeIndex_ = (writeIndex_ + 1) % static_cast<int>(buffer_.size());
    return out;
}

void PitchShifter::process(const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        output[i] = processSample(input[i]);
}

} // namespace simplelivetune::dsp
