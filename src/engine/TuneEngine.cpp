#include "engine/TuneEngine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace simplelivetune::engine {

namespace {
constexpr auto kVibratoTrackerCutoffHz = 1.5;
}

void TuneEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;
    downsampler_.prepare(sampleRate_, 12000.0);
    analysisSampleRate_ = downsampler_.outputSampleRate();
    detectorWindow_ = static_cast<int>(std::clamp(analysisSampleRate_ * 0.04, 384.0, 4096.0));
    // Ring and window are in analysis-rate samples; sized in prepare so the
    // audio thread never allocates.
    analysisRing_.resize(static_cast<std::size_t>(detectorWindow_ * 2));
    analysisWindow_.assign(static_cast<std::size_t>(detectorWindow_), 0.0f);
    detector_.prepare(analysisSampleRate_, 70.0f, 1000.0f, detectorWindow_);
    smoother_.prepare(sampleRate_);
    shifter_.prepare(sampleRate_);
    const auto hopRate = sampleRate_ / static_cast<double>(hopSize_);
    vibratoAlpha_ = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * kVibratoTrackerCutoffHz / hopRate));
    parametersInitialized_ = false;
    reset();
}

void TuneEngine::reset()
{
    analysisRing_.clear();
    downsampler_.reset();
    mapper_.reset();
    detector_.reset();
    smoother_.reset();
    shifter_.reset();
    hopCounter_ = 0;
    analysisSamplesSinceDetection_ = 0;
    targetRatio_ = 1.0f;
    lastPitchHz_ = 0.0f;
    vibratoTrackerActive_ = false;
    slowInputCents_ = 0.0f;
    previousMappedNote_ = -1;
    unvoicedHops_ = 0;
}

void TuneEngine::setParameters(const TuneParameters& parameters)
{
    const auto force = !parametersInitialized_;
    if (force || parameters.key != parameters_.key)
        mapper_.setKey(parameters.key);
    if (force || parameters.scale != parameters_.scale)
        mapper_.setScale(parameters.scale);
    if (force || parameters.confidenceThreshold != parameters_.confidenceThreshold)
        detector_.setConfidenceThreshold(parameters.confidenceThreshold);
    if (force || parameters.gateDb != parameters_.gateDb)
        detector_.setMinimumRmsLevel(std::pow(10.0f, parameters.gateDb / 20.0f));
    if (force || parameters.retuneTimeMs != parameters_.retuneTimeMs)
        smoother_.setRetuneTimeMs(parameters.retuneTimeMs);
    if (force || parameters.noteTransitionMs != parameters_.noteTransitionMs)
        smoother_.setNoteTransitionTimeMs(parameters.noteTransitionMs);
    if (force || parameters.correctionAmount != parameters_.correctionAmount)
        smoother_.setCorrectionAmount(parameters.correctionAmount);
    parameters_ = parameters;
    parametersInitialized_ = true;
}

void TuneEngine::runControlHop()
{
    if (!analysisRing_.filled(static_cast<std::size_t>(detectorWindow_))) {
        targetRatio_ = 1.0f;
        return;
    }

    analysisRing_.copyLatest(analysisWindow_.data(), analysisWindow_.size());
    const auto pitch = detector_.detectIncremental(
        analysisWindow_.data(),
        static_cast<int>(analysisWindow_.size()),
        analysisSamplesSinceDetection_);
    analysisSamplesSinceDetection_ = 0;
    if (pitch.voiced) {
        lastPitchHz_ = pitch.f0Hz;
        unvoicedHops_ = 0;
        shifter_.setPitchPeriodSamples(static_cast<float>(sampleRate_ / pitch.f0Hz));

        const auto inputCents = 1200.0f * std::log2(pitch.f0Hz / 440.0f);
        if (!vibratoTrackerActive_) {
            slowInputCents_ = inputCents;
            vibratoTrackerActive_ = true;
        }
        slowInputCents_ += vibratoAlpha_ * (inputCents - slowInputCents_);
        const auto vibratoCents = inputCents - slowInputCents_;

        const auto mappedHz = mapper_.mapFrequency(pitch.f0Hz);
        if (mappedHz > 0.0f) {
            const auto mappedNote = mapper_.lastMappedMidiNote();
            if (mappedNote != previousMappedNote_) {
                smoother_.beginNoteTransition();
                previousMappedNote_ = mappedNote;
            }
            const auto noteCents = 1200.0f * std::log2(mappedHz / 440.0f);
            // Only pull the pitch to the edge of the tolerance zone; leave it
            // untouched if it is already within +/- toleranceCents of the note.
            const auto deviationCents = inputCents - noteCents;
            const auto allowedDeviation = std::clamp(deviationCents,
                -parameters_.toleranceCents, parameters_.toleranceCents);
            const auto targetOutputCents = noteCents + allowedDeviation
                + parameters_.vibratoAmount * vibratoCents;
            targetRatio_ = std::exp2((targetOutputCents - inputCents) / 1200.0f);
        } else {
            targetRatio_ = 1.0f;
        }
    } else {
        targetRatio_ = 1.0f;
        vibratoTrackerActive_ = false;
        // Hold the last pitch briefly (display continuity across consonants),
        // then report silence so UIs can show gaps.
        if (++unvoicedHops_ >= 30)
            lastPitchHz_ = 0.0f;
    }
}

void TuneEngine::process(const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i) {
        float analysisSample = 0.0f;
        if (downsampler_.processSample(input[i], analysisSample)) {
            analysisRing_.push(analysisSample);
            ++analysisSamplesSinceDetection_;
        }
        if (hopCounter_ == 0)
            runControlHop();
        hopCounter_ = (hopCounter_ + 1) % hopSize_;

        const auto ratio = smoother_.process(parameters_.bypass ? 1.0f : targetRatio_, 1);
        shifter_.setRatio(ratio);
        // Always feed the shifter so its buffer stays warm across bypass
        // toggles; bypass only selects the output.
        const auto shifted = shifter_.processSample(input[i]);
        output[i] = parameters_.bypass ? input[i] : shifted;
    }
}

} // namespace simplelivetune::engine
