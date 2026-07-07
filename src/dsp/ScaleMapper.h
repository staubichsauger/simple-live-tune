#pragma once

#include <array>

namespace simplelivetune::dsp {

enum class ScaleType {
    Chromatic,
    Major,
    Minor,
    Custom
};

class ScaleMapper {
public:
    void setKey(int semitoneFromC);
    void setScale(ScaleType scale);
    void setCustomMask(std::array<bool, 12> mask);
    void reset();

    [[nodiscard]] float mapFrequency(float f0Hz);
    [[nodiscard]] float targetRatio(float f0Hz);
    [[nodiscard]] int lastMappedMidiNote() const { return heldMidiNote_; }

private:
    [[nodiscard]] bool isAllowedMidi(int midiNote) const;

    int key_ = 0;
    ScaleType scale_ = ScaleType::Chromatic;
    std::array<bool, 12> customMask_ {};
    int heldMidiNote_ = -1;
    float hysteresisCents_ = 60.0f;
};

} // namespace simplelivetune::dsp
