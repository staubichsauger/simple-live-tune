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

void ParameterSliderLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
    int width, int height, float sliderPos, float minSliderPos, float maxSliderPos,
    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal) {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
            minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto left = static_cast<float>(x) + 7.0f;
    const auto right = static_cast<float>(x + width) - 7.0f;
    const auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const auto track = juce::Rectangle<float>(left, centreY - 2.5f, right - left, 5.0f);
    const auto thumbX = juce::jlimit(left, right, sliderPos);
    const auto enabledAlpha = slider.isEnabled() ? 1.0f : 0.4f;

    g.setColour(juce::Colour(0xff30343f).withMultipliedAlpha(enabledAlpha));
    g.fillRoundedRectangle(track, 2.5f);

    if (thumbX > left) {
        auto activeTrack = track;
        activeTrack.setRight(thumbX);
        g.setColour(juce::Colour(0xff40a8d0).withMultipliedAlpha(enabledAlpha));
        g.fillRoundedRectangle(activeTrack, 2.5f);
    }

    const auto radius = slider.isMouseOverOrDragging() ? 7.0f : 6.0f;
    g.setColour(juce::Colour(slider.isMouseOverOrDragging() ? 0xff69c5e7 : 0xff40a8d0)
                    .withMultipliedAlpha(enabledAlpha));
    g.fillEllipse(thumbX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    if (slider.hasKeyboardFocus(false)) {
        g.setColour(juce::Colour(0xff8adbf4).withAlpha(0.7f));
        g.drawEllipse(thumbX - radius - 2.0f, centreY - radius - 2.0f,
            (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f, 1.0f);
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

    const auto setUpGroupLabel = [this](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xffaeb5c4));
        addAndMakeVisible(label);
    };
    setUpGroupLabel(correctionGroupLabel_, "CORRECTION");
    setUpGroupLabel(detectionGroupLabel_, "DETECTION");

    setUpLinear(retuneSlider_, retuneLabel_, "Retune speed", 20.0, " ms");
    setUpLinear(transitionSlider_, transitionLabel_, "Note transition", 60.0, " ms");
    setUpLinear(amountSlider_, amountLabel_, "Correction", 1.0);
    setUpLinear(vibratoSlider_, vibratoLabel_, "Vibrato", 0.0);
    setUpLinear(toleranceSlider_, toleranceLabel_, "Tolerance", 0.0, " ct");
    setUpLinear(confidenceSlider_, confidenceLabel_, "Confidence", 0.95);
    setUpLinear(gateSlider_, gateLabel_, "Gate", -45.0, " dB");

    retuneSlider_.setTooltip("0 ms is fast and hard; 400 ms is slower and more natural");
    transitionSlider_.setTooltip("How smoothly correction moves between notes");
    amountSlider_.setTooltip("Overall strength of pitch correction");
    vibratoSlider_.setTooltip("How much of the singer's vibrato is preserved");
    toleranceSlider_.setTooltip("Pitch deviation allowed before correction begins");
    confidenceSlider_.setTooltip("Minimum pitch-detection confidence required for correction");
    gateSlider_.setTooltip("Input level below which pitch correction is disabled");

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

    // SliderAttachment installs the parameter's default value-to-text callbacks,
    // so UI-specific percentage formatting has to be applied afterwards.
    const auto setUpPercentage = [](juce::Slider& slider) {
        slider.textFromValueFunction = [](double value) {
            return juce::String(static_cast<int>(std::lround(value * 100.0))) + "%";
        };
        slider.valueFromTextFunction = [](const juce::String& text) {
            return text.getDoubleValue() / 100.0;
        };
        slider.updateText();
    };
    setUpPercentage(amountSlider_);
    setUpPercentage(vibratoSlider_);
    setUpPercentage(confidenceSlider_);

    setSize(680, 620);
    startTimerHz(kTraceRateHz);
}

SimpleLiveTuneAudioProcessorEditor::~SimpleLiveTuneAudioProcessorEditor()
{
    for (auto* slider : { &retuneSlider_, &transitionSlider_, &amountSlider_, &vibratoSlider_,
             &toleranceSlider_, &confidenceSlider_, &gateSlider_ })
        slider->setLookAndFeel(nullptr);
}

void SimpleLiveTuneAudioProcessorEditor::setUpLinear(juce::Slider& slider, juce::Label& label,
    const juce::String& text, double defaultValue, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 24);
    slider.setDoubleClickReturnValue(true, defaultValue);
    slider.setTextValueSuffix(suffix);
    slider.setLookAndFeel(&parameterSliderLookAndFeel_);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff3f4f7));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff171920));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff353946));
    slider.setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xff40a8d0));
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffe4e7ed));
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void SimpleLiveTuneAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1d1f27));

    g.setColour(juce::Colour(0xff181a21));
    g.fillRoundedRectangle(correctionPanelBounds_.toFloat(), 7.0f);
    g.fillRoundedRectangle(detectionPanelBounds_.toFloat(), 7.0f);

    g.setColour(juce::Colour(0xff2b2f39));
    g.drawRoundedRectangle(correctionPanelBounds_.toFloat(), 7.0f, 1.0f);
    g.drawRoundedRectangle(detectionPanelBounds_.toFloat(), 7.0f, 1.0f);
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
    constexpr auto panelGap = 10;
    const auto panelWidth = (area.getWidth() - panelGap) / 2;
    correctionPanelBounds_ = area.removeFromLeft(panelWidth);
    area.removeFromLeft(panelGap);
    detectionPanelBounds_ = area;

    const auto layoutPanel = [](juce::Rectangle<int> panel, juce::Label& groupLabel,
                                 std::initializer_list<std::pair<juce::Label*, juce::Slider*>> controls) {
        panel.reduce(12, 8);
        groupLabel.setBounds(panel.removeFromTop(22));
        panel.removeFromTop(2);
        const auto rowHeight = panel.getHeight() / static_cast<int>(controls.size());
        for (const auto& [label, slider] : controls) {
            auto row = panel.removeFromTop(rowHeight);
            label->setBounds(row.removeFromLeft(104));
            slider->setBounds(row);
        }
    };

    layoutPanel(correctionPanelBounds_, correctionGroupLabel_, {
        { &retuneLabel_, &retuneSlider_ },
        { &transitionLabel_, &transitionSlider_ },
        { &amountLabel_, &amountSlider_ },
        { &vibratoLabel_, &vibratoSlider_ },
    });
    layoutPanel(detectionPanelBounds_, detectionGroupLabel_, {
        { &toleranceLabel_, &toleranceSlider_ },
        { &confidenceLabel_, &confidenceSlider_ },
        { &gateLabel_, &gateSlider_ },
    });
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
