#include "dsp/ScaleMapper.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace simplelivetune::dsp {

namespace {
float midiToHz(int midiNote)
{
    return 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
}

float hzToMidiFloat(float hz)
{
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}
}

void ScaleMapper::setKey(int semitoneFromC)
{
    key_ = ((semitoneFromC % 12) + 12) % 12;
}

void ScaleMapper::setScale(ScaleType scale)
{
    scale_ = scale;
}

void ScaleMapper::setCustomMask(std::array<bool, 12> mask)
{
    customMask_ = mask;
}

void ScaleMapper::reset()
{
    heldMidiNote_ = -1;
}

bool ScaleMapper::isAllowedMidi(int midiNote) const
{
    const auto degree = ((midiNote - key_) % 12 + 12) % 12;
    if (scale_ == ScaleType::Chromatic)
        return true;
    if (scale_ == ScaleType::Custom)
        return customMask_[static_cast<std::size_t>(degree)];

    static constexpr std::array<bool, 12> major { true, false, true, false, true, true, false, true, false, true, false, true };
    static constexpr std::array<bool, 12> minor { true, false, true, true, false, true, false, true, true, false, true, false };
    return scale_ == ScaleType::Major ? major[static_cast<std::size_t>(degree)] : minor[static_cast<std::size_t>(degree)];
}

float ScaleMapper::mapFrequency(float f0Hz)
{
    if (f0Hz <= 0.0f)
        return 0.0f;

    const auto midi = hzToMidiFloat(f0Hz);
    if (heldMidiNote_ >= 0 && isAllowedMidi(heldMidiNote_)) {
        const auto centsFromHeld = (midi - static_cast<float>(heldMidiNote_)) * 100.0f;
        if (std::abs(centsFromHeld) <= hysteresisCents_)
            return midiToHz(heldMidiNote_);
    }

    auto nearest = 0;
    auto bestDistance = std::numeric_limits<float>::infinity();
    const auto center = static_cast<int>(std::lround(midi));
    for (int note = center - 24; note <= center + 24; ++note) {
        if (!isAllowedMidi(note))
            continue;
        const auto distance = std::abs(midi - static_cast<float>(note));
        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = note;
        }
    }

    heldMidiNote_ = nearest;
    return midiToHz(nearest);
}

float ScaleMapper::targetRatio(float f0Hz)
{
    const auto target = mapFrequency(f0Hz);
    return target > 0.0f ? target / f0Hz : 1.0f;
}

} // namespace simplelivetune::dsp
