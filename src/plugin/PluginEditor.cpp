#include "plugin/PluginEditor.h"

#include <cmath>

namespace {
constexpr auto kTraceSeconds = 8;
constexpr auto kTraceRateHz = 30;
constexpr auto kMinTraceHz = 65.41f; // C2
constexpr auto kMaxTraceHz = 1046.5f; // C6
constexpr auto kKeyboardLowNote = 36; // C2
constexpr auto kKeyboardHighNote = 96; // C7

const char* const kNoteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

float midiFromHz(float hz)
{
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}

juce::String describePitch(float hz)
{
    if (hz <= 0.0f)
        return "listening...";

    const auto midi = midiFromHz(hz);
    const auto note = static_cast<int>(std::lround(midi));
    const auto cents = static_cast<int>(std::lround((midi - static_cast<float>(note)) * 100.0f));
    const auto octave = note / 12 - 1;
    return juce::String(kNoteNames[((note % 12) + 12) % 12]) + juce::String(octave)
        + "  " + juce::String(hz, 1) + " Hz  ("
        + (cents >= 0 ? "+" : "") + juce::String(cents) + " ct)";
}
}

void NoteMeterComponent::update(float inputHz, float correctedHz, int targetMidiNote)
{
    if (targetMidiNote < 0 || inputHz <= 0.0f) {
        if (active_) {
            active_ = false;
            repaint();
        }
        return;
    }

    const auto rawInputCents = (midiFromHz(inputHz) - static_cast<float>(targetMidiNote)) * 100.0f;
    const auto rawCorrectedCents = correctedHz > 0.0f
        ? (midiFromHz(correctedHz) - static_cast<float>(targetMidiNote)) * 100.0f
        : rawInputCents;

    if (!active_ || targetMidiNote != targetNote_) {
        // New note (or coming out of silence): snap instead of sweeping.
        inputCents_ = rawInputCents;
        correctedCents_ = rawCorrectedCents;
    } else {
        inputCents_ += kSmoothingAlpha * (rawInputCents - inputCents_);
        correctedCents_ += kSmoothingAlpha * (rawCorrectedCents - correctedCents_);
    }

    active_ = true;
    targetNote_ = targetMidiNote;
    repaint();
}

void NoteMeterComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff14161c));
    g.fillRoundedRectangle(bounds, 6.0f);

    // Cents ruler along the bottom.
    auto ruler = bounds.reduced(14.0f, 10.0f).removeFromBottom(34.0f);
    const auto scaleY = ruler.getY() + 12.0f;
    const auto centsToX = [&ruler](float cents) {
        return ruler.getX() + (juce::jlimit(-50.0f, 50.0f, cents) + 50.0f) / 100.0f * ruler.getWidth();
    };

    g.setColour(juce::Colour(0xff2a2d38));
    g.drawHorizontalLine(static_cast<int>(scaleY), ruler.getX(), ruler.getRight());
    for (int cents = -50; cents <= 50; cents += 10) {
        const auto x = centsToX(static_cast<float>(cents));
        const auto major = cents % 25 == 0;
        g.setColour(juce::Colour(major ? 0xff565b6b : 0xff2a2d38));
        g.drawVerticalLine(static_cast<int>(x), scaleY - (major ? 6.0f : 3.0f), scaleY + (major ? 6.0f : 3.0f));
    }
    g.setColour(juce::Colour(0xff565b6b));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("-50", static_cast<int>(centsToX(-50.0f)) - 12, static_cast<int>(scaleY) + 8, 24, 10, juce::Justification::centred);
    g.drawText("0", static_cast<int>(centsToX(0.0f)) - 12, static_cast<int>(scaleY) + 8, 24, 10, juce::Justification::centred);
    g.drawText("+50", static_cast<int>(centsToX(50.0f)) - 12, static_cast<int>(scaleY) + 8, 24, 10, juce::Justification::centred);

    // Note name.
    auto noteArea = bounds.withTrimmedBottom(44.0f);
    if (!active_) {
        g.setColour(juce::Colour(0xff3a3e4c));
        g.setFont(juce::FontOptions(52.0f, juce::Font::bold));
        g.drawText("-", noteArea, juce::Justification::centred);
        return;
    }

    const auto noteName = juce::String(kNoteNames[((targetNote_ % 12) + 12) % 12]);
    const auto octave = targetNote_ / 12 - 1;
    g.setColour(juce::Colours::lightgreen);
    g.setFont(juce::FontOptions(52.0f, juce::Font::bold));
    g.drawText(noteName + juce::String(octave), noteArea, juce::Justification::centred);

    // Deviation bars growing from the 0-cent centre: grey = input,
    // green = corrected output.
    const auto drawBar = [&](float cents, float top, juce::Colour colour) {
        const auto centre = centsToX(0.0f);
        const auto end = centsToX(cents);
        const auto width = std::max(std::abs(end - centre), 2.0f);
        g.setColour(colour);
        g.fillRoundedRectangle(std::min(centre, end), top, width, 5.0f, 2.0f);
    };
    drawBar(inputCents_, scaleY - 16.0f, juce::Colour(0xff8a8fa0));
    drawBar(correctedCents_, scaleY - 9.0f, juce::Colours::lightgreen);

    g.setColour(juce::Colour(0xff8a8fa0));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(juce::String(inputCents_ >= 0 ? "+" : "") + juce::String(static_cast<int>(std::lround(inputCents_))) + " ct",
        noteArea.reduced(10.0f, 6.0f), juce::Justification::bottomRight);
}

PitchTraceComponent::PitchTraceComponent()
{
    history_.resize(static_cast<std::size_t>(kTraceSeconds * kTraceRateHz));
    setInterceptsMouseClicks(false, false);
}

void PitchTraceComponent::push(float inputHz, float correctedHz)
{
    history_[writeIndex_] = { inputHz, correctedHz };
    writeIndex_ = (writeIndex_ + 1) % history_.size();
    repaint();
}

float PitchTraceComponent::yForHz(float hz, juce::Rectangle<float> bounds) const
{
    const auto norm = (std::log2(hz) - std::log2(kMinTraceHz))
        / (std::log2(kMaxTraceHz) - std::log2(kMinTraceHz));
    return bounds.getBottom() - juce::jlimit(0.0f, 1.0f, norm) * bounds.getHeight();
}

void PitchTraceComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff14161c));
    g.fillRoundedRectangle(bounds, 6.0f);

    // Octave gridlines (C2..C6).
    g.setFont(juce::FontOptions(11.0f));
    for (int octave = 2; octave <= 6; ++octave) {
        const auto hz = kMinTraceHz * std::pow(2.0f, static_cast<float>(octave - 2));
        const auto y = yForHz(hz, bounds);
        g.setColour(juce::Colour(0xff2a2d38));
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
        g.setColour(juce::Colour(0xff565b6b));
        g.drawText("C" + juce::String(octave), static_cast<int>(bounds.getX()) + 4,
            static_cast<int>(y) - 13, 30, 12, juce::Justification::left);
    }

    // Oldest-to-newest across the width; gaps where unvoiced.
    const auto count = history_.size();
    const auto xStep = (bounds.getWidth() - 8.0f) / static_cast<float>(count - 1);
    const auto xStart = bounds.getX() + 4.0f;

    const auto drawTrace = [&](juce::Colour colour, auto getHz) {
        juce::Path path;
        auto pathActive = false;
        for (std::size_t i = 0; i < count; ++i) {
            const auto& point = history_[(writeIndex_ + i) % count];
            const auto hz = getHz(point);
            if (hz <= 0.0f) {
                pathActive = false;
                continue;
            }
            const auto x = xStart + static_cast<float>(i) * xStep;
            const auto y = yForHz(hz, bounds);
            if (pathActive)
                path.lineTo(x, y);
            else
                path.startNewSubPath(x, y);
            pathActive = true;
        }
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(1.6f));
    };

    drawTrace(juce::Colour(0xff8a8fa0), [](const TracePoint& p) { return p.inputHz; });
    drawTrace(juce::Colours::lightgreen, [](const TracePoint& p) { return p.correctedHz; });
}

SimpleLiveTuneAudioProcessorEditor::SimpleLiveTuneAudioProcessorEditor(SimpleLiveTuneAudioProcessor& owner)
    : juce::AudioProcessorEditor(&owner)
    , processor_(owner)
{
    auto& apvts = processor_.parameters();

    addAndMakeVisible(noteMeter_);
    addAndMakeVisible(trace_);

    keyboard_.setAvailableRange(kKeyboardLowNote, kKeyboardHighNote);
    keyboard_.setOctaveForMiddleC(4);
    keyboard_.setScrollButtonsVisible(false);
    keyboard_.setInterceptsMouseClicks(false, false);
    keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
        juce::Colours::lightgreen.withAlpha(0.85f));
    addAndMakeVisible(keyboard_);

    titleLabel_.setText("Simple Live Tune", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel_);

    pitchLabel_.setText("listening...", juce::dontSendNotification);
    pitchLabel_.setJustificationType(juce::Justification::centredRight);
    pitchLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(pitchLabel_);

    const auto setUpCombo = [this, &apvts](juce::ComboBox& box, juce::Label& label,
                                const juce::String& text, const juce::String& parameterID) {
        if (const auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(parameterID)))
            box.addItemList(choice->choices, 1);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(box);
        addAndMakeVisible(label);
    };
    setUpCombo(keyBox_, keyLabel_, "Key", "key");
    setUpCombo(scaleBox_, scaleLabel_, "Scale", "scale");

    setUpRotary(retuneSlider_, retuneLabel_, "Retune");
    setUpRotary(transitionSlider_, transitionLabel_, "Transition");
    setUpRotary(amountSlider_, amountLabel_, "Amount");
    setUpRotary(vibratoSlider_, vibratoLabel_, "Vibrato");
    setUpRotary(toleranceSlider_, toleranceLabel_, "Tolerance");
    setUpRotary(confidenceSlider_, confidenceLabel_, "Confidence");
    setUpRotary(gateSlider_, gateLabel_, "Gate");

    addAndMakeVisible(bypassButton_);

    keyAttachment_ = std::make_unique<ComboAttachment>(apvts, "key", keyBox_);
    scaleAttachment_ = std::make_unique<ComboAttachment>(apvts, "scale", scaleBox_);
    retuneAttachment_ = std::make_unique<SliderAttachment>(apvts, "retune", retuneSlider_);
    transitionAttachment_ = std::make_unique<SliderAttachment>(apvts, "transition", transitionSlider_);
    amountAttachment_ = std::make_unique<SliderAttachment>(apvts, "amount", amountSlider_);
    vibratoAttachment_ = std::make_unique<SliderAttachment>(apvts, "vibrato", vibratoSlider_);
    toleranceAttachment_ = std::make_unique<SliderAttachment>(apvts, "tolerance", toleranceSlider_);
    confidenceAttachment_ = std::make_unique<SliderAttachment>(apvts, "confidence", confidenceSlider_);
    gateAttachment_ = std::make_unique<SliderAttachment>(apvts, "gate", gateSlider_);
    bypassAttachment_ = std::make_unique<ButtonAttachment>(apvts, "bypass", bypassButton_);

    setSize(680, 620);
    startTimerHz(kTraceRateHz);
}

SimpleLiveTuneAudioProcessorEditor::~SimpleLiveTuneAudioProcessorEditor() = default;

void SimpleLiveTuneAudioProcessorEditor::setUpRotary(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 18);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void SimpleLiveTuneAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1d1f27));
}

void SimpleLiveTuneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto header = area.removeFromTop(36);
    titleLabel_.setBounds(header.removeFromLeft(180));
    pitchLabel_.setBounds(header);

    area.removeFromTop(8);
    auto displayRow = area.removeFromTop(170);
    noteMeter_.setBounds(displayRow.removeFromLeft(200));
    displayRow.removeFromLeft(8);
    trace_.setBounds(displayRow);

    area.removeFromTop(8);
    auto keyboardArea = area.removeFromTop(64);
    keyboard_.setBounds(keyboardArea);
    constexpr auto numWhiteKeys = 36.0f; // C2..C7 inclusive
    keyboard_.setKeyWidth(static_cast<float>(keyboardArea.getWidth()) / numWhiteKeys);

    area.removeFromTop(10);
    auto selectors = area.removeFromTop(52);
    const auto selectorWidth = selectors.getWidth() / 3;

    auto keyArea = selectors.removeFromLeft(selectorWidth);
    keyLabel_.setBounds(keyArea.removeFromTop(18));
    keyBox_.setBounds(keyArea.reduced(8, 4));

    auto scaleArea = selectors.removeFromLeft(selectorWidth);
    scaleLabel_.setBounds(scaleArea.removeFromTop(18));
    scaleBox_.setBounds(scaleArea.reduced(8, 4));

    bypassButton_.setBounds(selectors.reduced(8, 12));

    area.removeFromTop(10);
    const auto knobWidth = area.getWidth() / 7;

    const auto layoutKnob = [&area, knobWidth](juce::Label& label, juce::Slider& slider) {
        auto knobArea = area.removeFromLeft(knobWidth);
        label.setBounds(knobArea.removeFromTop(18));
        slider.setBounds(knobArea.reduced(4));
    };
    layoutKnob(retuneLabel_, retuneSlider_);
    layoutKnob(transitionLabel_, transitionSlider_);
    layoutKnob(amountLabel_, amountSlider_);
    layoutKnob(vibratoLabel_, vibratoSlider_);
    layoutKnob(toleranceLabel_, toleranceSlider_);
    layoutKnob(confidenceLabel_, confidenceSlider_);
    layoutKnob(gateLabel_, gateSlider_);
}

void SimpleLiveTuneAudioProcessorEditor::timerCallback()
{
    const auto pitchHz = processor_.lastPitchHz();
    const auto ratio = processor_.currentRatio();
    auto text = describePitch(pitchHz);
    if (pitchHz > 0.0f && std::abs(ratio - 1.0f) > 0.0005f) {
        const auto correctionCents = static_cast<int>(std::lround(1200.0f * std::log2(ratio)));
        text += "  ->  " + juce::String(correctionCents >= 0 ? "+" : "") + juce::String(correctionCents) + " ct";
    }
    pitchLabel_.setText(text, juce::dontSendNotification);

    const auto correctedHz = pitchHz > 0.0f ? pitchHz * ratio : 0.0f;
    trace_.push(pitchHz, correctedHz);

    const auto targetNote = processor_.lastTargetMidiNote();
    noteMeter_.update(pitchHz, correctedHz, targetNote);
    if (targetNote != shownTargetNote_) {
        keyboardState_.allNotesOff(1);
        if (targetNote >= kKeyboardLowNote && targetNote <= kKeyboardHighNote)
            keyboardState_.noteOn(1, targetNote, 1.0f);
        shownTargetNote_ = targetNote;
    }
}
