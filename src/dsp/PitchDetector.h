#pragma once

#include <vector>

namespace simplelivetune::dsp {

struct PitchResult {
    float f0Hz = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
};

class PitchDetector {
public:
    void prepare(double sampleRate, float minFrequencyHz = 70.0f, float maxFrequencyHz = 1000.0f);
    void setConfidenceThreshold(float threshold) { confidenceThreshold_ = threshold; }
    // NSDF confidence is amplitude-normalized, so quiet breaths/noise can
    // still look "confident"; frames below this RMS level are always unvoiced.
    void setMinimumRmsLevel(float linearRms) { minimumRms_ = linearRms; }
    [[nodiscard]] PitchResult detect(const float* samples, int numSamples);

private:
    // NSDF local maxima above this floor are peak-pick candidates (MPM
    // convention); the voiced decision uses confidenceThreshold_ separately.
    static constexpr float kPeakPickFloor = 0.3f;
    // Consecutive unvoiced frames after which pitch continuity state resets.
    static constexpr int kUnvoicedFramesToReset = 40;

    double sampleRate_ = 12000.0;
    float minFrequencyHz_ = 70.0f;
    float maxFrequencyHz_ = 1000.0f;
    float confidenceThreshold_ = 0.72f;
    float minimumRms_ = 0.005623f; // -45 dBFS
    float previousF0Hz_ = 0.0f;
    int unvoicedStreak_ = 0;
    std::vector<float> nsdf_;
};

} // namespace simplelivetune::dsp
