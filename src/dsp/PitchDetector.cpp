#include "dsp/PitchDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace simplelivetune::dsp {

void PitchDetector::prepare(double sampleRate, float minFrequencyHz, float maxFrequencyHz)
{
    sampleRate_ = sampleRate;
    minFrequencyHz_ = minFrequencyHz;
    maxFrequencyHz_ = maxFrequencyHz;
    previousF0Hz_ = 0.0f;
    unvoicedStreak_ = 0;
    // Preallocate for the largest possible lag so detect() never allocates.
    const auto maxTauCap = static_cast<std::size_t>(std::ceil(sampleRate_ / minFrequencyHz_)) + 2;
    nsdf_.assign(maxTauCap, 0.0f);
}

PitchResult PitchDetector::detect(const float* samples, int numSamples)
{
    PitchResult result;
    const auto unvoiced = [this, &result]() -> PitchResult {
        if (++unvoicedStreak_ >= kUnvoicedFramesToReset)
            previousF0Hz_ = 0.0f;
        return result;
    };

    if (numSamples < 16)
        return unvoiced();

    const auto minTau = std::max(2, static_cast<int>(std::floor(sampleRate_ / maxFrequencyHz_)));
    const auto maxTau = std::min({ numSamples - 2,
        static_cast<int>(std::ceil(sampleRate_ / minFrequencyHz_)),
        static_cast<int>(nsdf_.size()) - 1 });
    if (minTau >= maxTau)
        return unvoiced();

    double energy = 0.0;
    for (int i = 0; i < numSamples; ++i)
        energy += static_cast<double>(samples[i]) * samples[i];
    const auto rms = std::sqrt(energy / static_cast<double>(numSamples));
    if (rms < static_cast<double>(minimumRms_))
        return unvoiced();

    for (int tau = 0; tau <= maxTau; ++tau) {
        double acf = 0.0;
        double divisor = 0.0;
        for (int i = 0; i < numSamples - tau; ++i) {
            const auto a = static_cast<double>(samples[i]);
            const auto b = static_cast<double>(samples[i + tau]);
            acf += a * b;
            divisor += a * a + b * b;
        }
        nsdf_[static_cast<std::size_t>(tau)] = divisor > 0.0 ? static_cast<float>((2.0 * acf) / divisor) : 0.0f;
    }

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
