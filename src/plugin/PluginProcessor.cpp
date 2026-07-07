#include "plugin/PluginProcessor.h"

#include "plugin/PluginEditor.h"

#include <algorithm>

namespace {
constexpr auto kFallbackBlockSize = 512;
}

SimpleLiveTuneAudioProcessor::SimpleLiveTuneAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts_(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    keyParam_ = apvts_.getRawParameterValue("key");
    scaleParam_ = apvts_.getRawParameterValue("scale");
    retuneParam_ = apvts_.getRawParameterValue("retune");
    transitionParam_ = apvts_.getRawParameterValue("transition");
    amountParam_ = apvts_.getRawParameterValue("amount");
    vibratoParam_ = apvts_.getRawParameterValue("vibrato");
    toleranceParam_ = apvts_.getRawParameterValue("tolerance");
    confidenceParam_ = apvts_.getRawParameterValue("confidence");
    gateParam_ = apvts_.getRawParameterValue("gate");
    bypassParam_ = apvts_.getRawParameterValue("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleLiveTuneAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { "key", 1 }, "Key",
        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { "scale", 1 }, "Scale",
        StringArray { "Chromatic", "Major", "Minor" }, 0));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { "retune", 1 }, "Retune Speed",
        NormalisableRange<float>(0.0f, 400.0f, 0.1f, 0.5f), 20.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { "transition", 1 }, "Note Transition",
        NormalisableRange<float>(0.0f, 200.0f, 0.1f, 0.5f), 60.0f,
        AudioParameterFloatAttributes().withLabel("ms")));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { "amount", 1 }, "Correction Amount",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { "vibrato", 1 }, "Vibrato",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { "tolerance", 1 }, "Tune Tolerance",
        NormalisableRange<float>(0.0f, 50.0f, 0.5f), 0.0f,
        AudioParameterFloatAttributes().withLabel("ct")));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { "confidence", 1 }, "Confidence Threshold",
        NormalisableRange<float>(0.5f, 0.95f, 0.01f), 0.95f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { "gate", 1 }, "Gate",
        NormalisableRange<float>(-80.0f, -20.0f, 0.5f), -45.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

void SimpleLiveTuneAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, samplesPerBlock);
    monoBuffer_.setSize(1, std::max(samplesPerBlock, kFallbackBlockSize));
    applyParametersToEngine();

    // The audio path is never buffered for analysis; the shifter's small
    // signal-dependent delay cannot be host-compensated, so we report zero.
    setLatencySamples(0);
}

void SimpleLiveTuneAudioProcessor::releaseResources()
{
    engine_.reset();
}

bool SimpleLiveTuneAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SimpleLiveTuneAudioProcessor::applyParametersToEngine()
{
    simplelivetune::engine::TuneParameters params;
    params.key = static_cast<int>(keyParam_->load(std::memory_order_relaxed));
    params.scale = static_cast<simplelivetune::dsp::ScaleType>(
        static_cast<int>(scaleParam_->load(std::memory_order_relaxed)));
    params.retuneTimeMs = retuneParam_->load(std::memory_order_relaxed);
    params.noteTransitionMs = transitionParam_->load(std::memory_order_relaxed);
    params.correctionAmount = amountParam_->load(std::memory_order_relaxed);
    params.vibratoAmount = vibratoParam_->load(std::memory_order_relaxed);
    params.toleranceCents = toleranceParam_->load(std::memory_order_relaxed);
    params.confidenceThreshold = confidenceParam_->load(std::memory_order_relaxed);
    params.gateDb = gateParam_->load(std::memory_order_relaxed);
    params.bypass = bypassParam_->load(std::memory_order_relaxed) >= 0.5f;
    engine_.setParameters(params);
}

void SimpleLiveTuneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0)
        return;

    applyParametersToEngine();

    // The engine is mono: stereo input is summed to mid before correction and
    // the corrected signal is written to all output channels.
    const auto capacity = monoBuffer_.getNumSamples();
    auto* mono = monoBuffer_.getWritePointer(0);

    for (int position = 0; position < numSamples;) {
        const auto count = std::min(numSamples - position, capacity);

        if (numChannels == 1) {
            std::copy_n(buffer.getReadPointer(0) + position, count, mono);
        } else {
            const auto gain = 1.0f / static_cast<float>(numChannels);
            std::copy_n(buffer.getReadPointer(0) + position, count, mono);
            for (int channel = 1; channel < numChannels; ++channel) {
                const auto* in = buffer.getReadPointer(channel) + position;
                for (int i = 0; i < count; ++i)
                    mono[i] += in[i];
            }
            for (int i = 0; i < count; ++i)
                mono[i] *= gain;
        }

        engine_.process(mono, mono, count);

        for (int channel = 0; channel < numChannels; ++channel)
            std::copy_n(mono, count, buffer.getWritePointer(channel) + position);

        position += count;
    }

    lastPitchHz_.store(engine_.lastDetectedPitchHz(), std::memory_order_relaxed);
    currentRatio_.store(engine_.currentRatio(), std::memory_order_relaxed);
    targetNote_.store(engine_.lastTargetMidiNote(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* SimpleLiveTuneAudioProcessor::createEditor()
{
    return new SimpleLiveTuneAudioProcessorEditor(*this);
}

void SimpleLiveTuneAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SimpleLiveTuneAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType()))
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleLiveTuneAudioProcessor();
}
