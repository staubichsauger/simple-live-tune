#include "dsp/PitchDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace simplelivetune::dsp {

void PitchDetector::prepare(
    double sampleRate, float minFrequencyHz, float maxFrequencyHz, int maximumWindowSize)
{
    sampleRate_ = sampleRate;
    minFrequencyHz_ = minFrequencyHz;
    maxFrequencyHz_ = maxFrequencyHz;
    // Preallocate for the largest possible lag and window so neither detection
    // path allocates on the audio thread.
    const auto maxTauCap = static_cast<std::size_t>(std::ceil(sampleRate_ / minFrequencyHz_)) + 2;
    nsdf_.assign(maxTauCap, 0.0f);
    acf_.assign(maxTauCap, 0.0);
    divisor_.assign(maxTauCap, 0.0);
    previousWindow_.assign(static_cast<std::size_t>(std::max(16, maximumWindowSize)), 0.0f);
    reset();
}

void PitchDetector::reset()
{
    previousF0Hz_ = 0.0f;
    unvoicedStreak_ = 0;
    cachedWindowSize_ = 0;
    correlationCacheValid_ = false;
}

PitchResult PitchDetector::detect(const float* samples, int numSamples)
{
    return detectImpl(samples, numSamples, numSamples, false);
}

PitchResult PitchDetector::detectIncremental(
    const float* samples, int numSamples, int advancedSamples)
{
    return detectImpl(samples, numSamples, advancedSamples, true);
}

PitchResult PitchDetector::detectImpl(
    const float* samples, int numSamples, int advancedSamples, bool allowIncremental)
{
    const auto unvoiced = [this]() -> PitchResult {
        if (++unvoicedStreak_ >= kUnvoicedFramesToReset)
            previousF0Hz_ = 0.0f;
        return {};
    };

    if (numSamples < 16)
        return unvoiced();

    const auto minTau = std::max(2, static_cast<int>(std::floor(sampleRate_ / maxFrequencyHz_)));
    const auto maxTau = std::min({ numSamples - 2,
        static_cast<int>(std::ceil(sampleRate_ / minFrequencyHz_)),
        static_cast<int>(nsdf_.size()) - 1 });
    if (minTau >= maxTau)
        return unvoiced();

    auto energy = 0.0;
    for (int i = 0; i < numSamples; ++i)
        energy += static_cast<double>(samples[i]) * samples[i];
    const auto rms = std::sqrt(energy / static_cast<double>(numSamples));

    const auto canUpdate = allowIncremental
        && correlationCacheValid_
        && cachedWindowSize_ == numSamples
        && advancedSamples > 0
        && advancedSamples < numSamples
        && numSamples <= static_cast<int>(previousWindow_.size());
    if (canUpdate)
        updateCorrelations(samples, numSamples, maxTau, advancedSamples);
    else
        recomputeCorrelations(samples, numSamples, maxTau);
    storeWindow(samples, numSamples);

    if (rms < static_cast<double>(minimumRms_))
        return unvoiced();

    return pickResult(minTau, maxTau);
}

void PitchDetector::recomputeCorrelations(const float* samples, int numSamples, int maxTau)
{
    for (int tau = 0; tau <= maxTau; ++tau) {
        auto acf = 0.0;
        auto divisor = 0.0;
        for (int i = 0; i < numSamples - tau; ++i) {
            const auto a = static_cast<double>(samples[i]);
            const auto b = static_cast<double>(samples[i + tau]);
            acf += a * b;
            divisor += a * a + b * b;
        }
        acf_[static_cast<std::size_t>(tau)] = acf;
        divisor_[static_cast<std::size_t>(tau)] = divisor;
        nsdf_[static_cast<std::size_t>(tau)] = divisor > 0.0
            ? static_cast<float>((2.0 * acf) / divisor)
            : 0.0f;
    }
}

void PitchDetector::updateCorrelations(
    const float* samples, int numSamples, int maxTau, int advancedSamples)
{
    for (int tau = 0; tau <= maxTau; ++tau) {
        const auto pairCount = numSamples - tau;
        if (advancedSamples >= pairCount) {
            auto acf = 0.0;
            auto divisor = 0.0;
            for (int i = 0; i < pairCount; ++i) {
                const auto a = static_cast<double>(samples[i]);
                const auto b = static_cast<double>(samples[i + tau]);
                acf += a * b;
                divisor += a * a + b * b;
            }
            acf_[static_cast<std::size_t>(tau)] = acf;
            divisor_[static_cast<std::size_t>(tau)] = divisor;
        } else {
            auto& acf = acf_[static_cast<std::size_t>(tau)];
            auto& divisor = divisor_[static_cast<std::size_t>(tau)];
            for (int i = 0; i < advancedSamples; ++i) {
                const auto a = static_cast<double>(previousWindow_[static_cast<std::size_t>(i)]);
                const auto b = static_cast<double>(previousWindow_[static_cast<std::size_t>(i + tau)]);
                acf -= a * b;
                divisor -= a * a + b * b;
            }
            for (int i = pairCount - advancedSamples; i < pairCount; ++i) {
                const auto a = static_cast<double>(samples[i]);
                const auto b = static_cast<double>(samples[i + tau]);
                acf += a * b;
                divisor += a * a + b * b;
            }
        }

        const auto divisor = divisor_[static_cast<std::size_t>(tau)];
        nsdf_[static_cast<std::size_t>(tau)] = divisor > 0.0
            ? static_cast<float>((2.0 * acf_[static_cast<std::size_t>(tau)]) / divisor)
            : 0.0f;
    }
}

void PitchDetector::storeWindow(const float* samples, int numSamples)
{
    if (numSamples > static_cast<int>(previousWindow_.size())) {
        cachedWindowSize_ = 0;
        correlationCacheValid_ = false;
        return;
    }
    std::copy_n(samples, numSamples, previousWindow_.data());
    cachedWindowSize_ = numSamples;
    correlationCacheValid_ = true;
}

PitchResult PitchDetector::pickResult(int minTau, int maxTau)
{
    PitchResult result;
    const auto unvoiced = [this, &result]() -> PitchResult {
        if (++unvoicedStreak_ >= kUnvoicedFramesToReset)
            previousF0Hz_ = 0.0f;
        return result;
    };

    const auto isLocalPeak = [this](int tau) {
        return nsdf_[static_cast<std::size_t>(tau)] > nsdf_[static_cast<std::size_t>(tau - 1)]
            && nsdf_[static_cast<std::size_t>(tau)] >= nsdf_[static_cast<std::size_t>(tau + 1)]
            && nsdf_[static_cast<std::size_t>(tau)] > kPeakPickFloor;
    };

    // MPM: find the highest candidate peak, then pick the first (shortest-lag)
    // peak within 93% of it. Two passes, no allocations.
    auto globalPeak = 0.0f;
    for (int tau = minTau + 1; tau < maxTau; ++tau) {
        if (isLocalPeak(tau))
            globalPeak = std::max(globalPeak, nsdf_[static_cast<std::size_t>(tau)]);
    }
    if (globalPeak <= 0.0f)
        return unvoiced();

    auto bestTau = 0;
    auto bestValue = 0.0f;
    for (int tau = minTau + 1; tau < maxTau; ++tau) {
        if (isLocalPeak(tau) && nsdf_[static_cast<std::size_t>(tau)] >= 0.93f * globalPeak) {
            bestTau = tau;
            bestValue = nsdf_[static_cast<std::size_t>(tau)];
            break;
        }
    }

    if (bestTau == 0 || bestValue < confidenceThreshold_)
        return unvoiced();

    float refinedTau = static_cast<float>(bestTau);
    const auto ym1 = nsdf_[static_cast<std::size_t>(bestTau - 1)];
    const auto y0 = nsdf_[static_cast<std::size_t>(bestTau)];
    const auto yp1 = nsdf_[static_cast<std::size_t>(bestTau + 1)];
    const auto denom = ym1 - 2.0f * y0 + yp1;
    if (std::abs(denom) > 1.0e-6f)
        refinedTau += 0.5f * (ym1 - yp1) / denom;

    result.f0Hz = static_cast<float>(sampleRate_ / refinedTau);
    result.confidence = std::clamp(bestValue, 0.0f, 1.0f);
    result.voiced = true;

    if (previousF0Hz_ > 0.0f) {
        const auto jump = std::abs(1200.0f * std::log2(result.f0Hz / previousF0Hz_));
        if (jump > 900.0f && result.confidence < 0.9f)
            result.voiced = false;
    }

    if (!result.voiced)
        return unvoiced();

    previousF0Hz_ = result.f0Hz;
    unvoicedStreak_ = 0;
    return result;
}

} // namespace simplelivetune::dsp
