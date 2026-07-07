#pragma once

namespace simplelivetune::dsp {

class RatioSmoother {
public:
    void prepare(double sampleRate);
    void reset(float ratio = 1.0f);
    void setRetuneTimeMs(float milliseconds);
    void setNoteTransitionTimeMs(float milliseconds);
    void setCorrectionAmount(float amount);

    // Switches to the (usually faster) transition time constant until the
    // smoothed ratio has converged onto the new target, then falls back to
    // the retune speed for in-note drift. Call when the target note changes.
    void beginNoteTransition() { transitionActive_ = true; }

    [[nodiscard]] float process(float targetRatio, int numSamples);
    [[nodiscard]] float currentRatio() const { return currentRatio_; }

private:
    static constexpr float kConvergedCents = 10.0f;

    void updateAlpha();
    [[nodiscard]] static float alphaForTime(double sampleRate, float milliseconds);

    double sampleRate_ = 48000.0;
    float retuneTimeMs_ = 80.0f;
    float noteTransitionMs_ = 20.0f;
    float correctionAmount_ = 1.0f;
    float currentCents_ = 0.0f;
    float currentRatio_ = 1.0f;
    float alphaPerSample_ = 1.0f;
    float transitionAlpha_ = 1.0f;
    bool transitionActive_ = false;
};

} // namespace simplelivetune::dsp
