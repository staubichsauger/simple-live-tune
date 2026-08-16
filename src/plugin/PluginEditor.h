#pragma once

#include "plugin/PluginProcessor.h"

#include <JuceHeader.h>

#include <vector>

// Big detected-note display with a +/-50 cent deviation meter: grey marker
// shows the raw input's deviation from the target note, green shows the
// corrected output's deviation.
class NoteMeterComponent : public juce::Component {
public:
    NoteMeterComponent() = default;

    void update(float inputHz, float correctedHz, int targetMidiNote);

    void paint(juce::Graphics& g) override;

private:
    // Displayed deviations are smoothed (one-pole, ~250 ms at the UI rate) to
    // keep the markers readable; they snap when the target note changes.
    static constexpr float kSmoothingAlpha = 0.12f;

    bool active_ = false;
    int targetNote_ = -1;
    float inputCents_ = 0.0f;
    float correctedCents_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteMeterComponent)
};

// Scrolling display of detected input pitch vs corrected output pitch on a
// log-frequency axis with octave gridlines.
class PitchTraceComponent : public juce::Component {
public:
    PitchTraceComponent();

    // Called from the editor timer; hz <= 0 marks an unvoiced gap.
    void push(float inputHz, float correctedHz);

    void paint(juce::Graphics& g) override;

private:
    struct TracePoint {
        float inputHz = 0.0f;
        float correctedHz = 0.0f;
    };

    [[nodiscard]] float yForHz(float hz, juce::Rectangle<float> bounds) const;

    std::vector<TracePoint> history_;
    std::size_t writeIndex_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchTraceComponent)
};

// Compact linear controls are easier to scan and adjust than a row of rotary
// controls. This look-and-feel keeps the track understated and uses the same
// cyan accent as the rest of the editor.
class ParameterSliderLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        juce::Slider::SliderStyle style, juce::Slider& slider) override;
};

class SimpleLiveTuneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer {
public:
    explicit SimpleLiveTuneAudioProcessorEditor(SimpleLiveTuneAudioProcessor& owner);
    ~SimpleLiveTuneAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void setUpLinear(juce::Slider& slider, juce::Label& label, const juce::String& text,
        double defaultValue, const juce::String& suffix = {});

    SimpleLiveTuneAudioProcessor& processor_;

    juce::Label titleLabel_;
    juce::Label pitchLabel_;

    NoteMeterComponent noteMeter_;
    PitchTraceComponent trace_;
    juce::MidiKeyboardState keyboardState_;
    juce::MidiKeyboardComponent keyboard_ { keyboardState_, juce::MidiKeyboardComponent::horizontalKeyboard };
    int shownTargetNote_ = -1;

    juce::ComboBox keyBox_;
    juce::ComboBox scaleBox_;
    juce::Label keyLabel_;
    juce::Label scaleLabel_;
    juce::ToggleButton bypassButton_ { "Bypass" };

    ParameterSliderLookAndFeel parameterSliderLookAndFeel_;
    juce::Label correctionGroupLabel_;
    juce::Label detectionGroupLabel_;
    juce::Rectangle<int> correctionPanelBounds_;
    juce::Rectangle<int> detectionPanelBounds_;

    juce::Slider retuneSlider_;
    juce::Slider transitionSlider_;
    juce::Slider amountSlider_;
    juce::Slider vibratoSlider_;
    juce::Slider toleranceSlider_;
    juce::Slider confidenceSlider_;
    juce::Slider gateSlider_;
    juce::Label retuneLabel_;
    juce::Label transitionLabel_;
    juce::Label amountLabel_;
    juce::Label vibratoLabel_;
    juce::Label toleranceLabel_;
    juce::Label confidenceLabel_;
    juce::Label gateLabel_;

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAttachment> keyAttachment_;
    std::unique_ptr<ComboAttachment> scaleAttachment_;
    std::unique_ptr<SliderAttachment> retuneAttachment_;
    std::unique_ptr<SliderAttachment> transitionAttachment_;
    std::unique_ptr<SliderAttachment> amountAttachment_;
    std::unique_ptr<SliderAttachment> vibratoAttachment_;
    std::unique_ptr<SliderAttachment> toleranceAttachment_;
    std::unique_ptr<SliderAttachment> confidenceAttachment_;
    std::unique_ptr<SliderAttachment> gateAttachment_;
    std::unique_ptr<ButtonAttachment> bypassAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleLiveTuneAudioProcessorEditor)
};
