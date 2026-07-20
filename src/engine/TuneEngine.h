#pragma once

#include "dsp/Downsampler.h"
#include "dsp/PitchDetector.h"
#include "dsp/PitchShifter.h"
#include "dsp/RatioSmoother.h"
#include "dsp/RingBuffer.h"
#include "dsp/ScaleMapper.h"

#include <vector>

namespace simplelivetune::engine {

struct TuneParameters {
    int key = 0;
    dsp::ScaleType scale = dsp::ScaleType::Chromatic;
    float retuneTimeMs = 20.0f;
    float noteTransitionMs = 60.0f;
    float correctionAmount = 1.0f;
    float vibratoAmount = 0.0f; // 0 = flatten, 1 = keep the input's fast pitch modulation
    // Dead zone around the target note: input is only pulled to within
    // +/- toleranceCents of the note centre; input already inside the zone
    // is left untouched.
    float toleranceCents = 0.0f;
    float confidenceThreshold = 0.95f;
    float gateDb = -45.0f; // input below this RMS level is never corrected
    bool bypass = false;
};

class TuneEngine {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const TuneParameters& parameters);
    void process(const float* input, float* output, int numSamples);

    [[nodiscard]] float lastDetectedPitchHz() const { return lastPitchHz_; }
    [[nodiscard]] float currentRatio() const { return smoother_.currentRatio(); }
    // Target note of the most recent correction, or -1 while unvoiced.
    [[nodiscard]] int lastTargetMidiNote() const { return lastPitchHz_ > 0.0f ? previousMappedNote_ : -1; }

private:
    void runControlHop();

    double sampleRate_ = 48000.0;
    double analysisSampleRate_ = 12000.0;
    int hopSize_ = 64;
    int hopCounter_ = 0;
    int analysisSamplesSinceDetection_ = 0;
    int detectorWindow_ = 512;
    TuneParameters parameters_;
    dsp::RingBuffer analysisRing_;
    std::vector<float> analysisWindow_;
    dsp::Downsampler downsampler_;
    dsp::PitchDetector detector_;
    dsp::ScaleMapper mapper_;
    dsp::RatioSmoother smoother_;
    dsp::PitchShifter shifter_;
    float targetRatio_ = 1.0f;
    float lastPitchHz_ = 0.0f;
    // Vibrato preservation: slow tracker of the input pitch (in cents); the
    // fast remainder is the vibrato that vibratoAmount adds back.
    bool vibratoTrackerActive_ = false;
    float slowInputCents_ = 0.0f;
    float vibratoAlpha_ = 0.0125f;
    int previousMappedNote_ = -1;
    int unvoicedHops_ = 0;
    bool parametersInitialized_ = false;
};

} // namespace simplelivetune::engine
