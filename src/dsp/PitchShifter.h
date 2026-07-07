#pragma once

#include <vector>

namespace simplelivetune::dsp {

// Pitch-synchronous delay-line shifter with near-zero latency.
//
// A single read tap drifts relative to the write head at (1 - ratio) samples
// per sample. It is kept within [kMinDelaySamples, kMinDelaySamples + period
// + fade guard]; when it leaves that range it jumps by exactly one detected
// pitch period with a raised-cosine crossfade, which keeps the waveform
// phase-continuous for periodic input. Worst-case delay is therefore about
// one pitch period; at ratio == 1 the tap freezes and the shifter is an
// exact kMinDelaySamples-sample delay (interpolator lookahead only).
class PitchShifter {
public:
    static constexpr float kMinDelaySamples = 2.0f; // cubic interpolator lookahead

    void prepare(double sampleRate);
    void reset();
    void setRatio(float ratio);
    void setPitchPeriodSamples(float periodSamples);
    float processSample(float input);
    void process(const float* input, float* output, int numSamples);

    [[nodiscard]] float currentDelaySamples() const { return delay_; }

private:
    void startCrossfade(float jumpAmount);
    [[nodiscard]] float readInterpolated(float delaySamples) const;

    double sampleRate_ = 48000.0;
    std::vector<float> buffer_;
    int writeIndex_ = 0;
    float ratio_ = 1.0f;
    float periodSamples_ = 218.0f;
    float maxDelaySamples_ = 2048.0f;
    float delay_ = kMinDelaySamples;
    float fadeDelay_ = kMinDelaySamples;
    int fadeLength_ = 0;
    int fadeRemaining_ = 0;
    int maxFadeSamples_ = 144;
};

} // namespace simplelivetune::dsp
