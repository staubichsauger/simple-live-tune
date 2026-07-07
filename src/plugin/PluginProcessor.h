#pragma once

#include "engine/TuneEngine.h"

#include <JuceHeader.h>

#include <atomic>

class SimpleLiveTuneAudioProcessor : public juce::AudioProcessor {
public:
    SimpleLiveTuneAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& parameters() { return apvts_; }

    // Snapshots for the editor (updated on the audio thread).
    [[nodiscard]] float lastPitchHz() const { return lastPitchHz_.load(std::memory_order_relaxed); }
    [[nodiscard]] float currentRatio() const { return currentRatio_.load(std::memory_order_relaxed); }
    [[nodiscard]] int lastTargetMidiNote() const { return targetNote_.load(std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyParametersToEngine();

    juce::AudioProcessorValueTreeState apvts_;
    simplelivetune::engine::TuneEngine engine_;
    juce::AudioBuffer<float> monoBuffer_;

    std::atomic<float>* keyParam_ = nullptr;
    std::atomic<float>* scaleParam_ = nullptr;
    std::atomic<float>* retuneParam_ = nullptr;
    std::atomic<float>* transitionParam_ = nullptr;
    std::atomic<float>* amountParam_ = nullptr;
    std::atomic<float>* vibratoParam_ = nullptr;
    std::atomic<float>* toleranceParam_ = nullptr;
    std::atomic<float>* confidenceParam_ = nullptr;
    std::atomic<float>* gateParam_ = nullptr;
    std::atomic<float>* bypassParam_ = nullptr;

    std::atomic<float> lastPitchHz_ { 0.0f };
    std::atomic<float> currentRatio_ { 1.0f };
    std::atomic<int> targetNote_ { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleLiveTuneAudioProcessor)
};
