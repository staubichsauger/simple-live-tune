#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

namespace simplelivetune::dsp {

class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity = 0) { resize(capacity); }

    void resize(std::size_t capacity)
    {
        buffer_.assign(capacity, 0.0f);
        writeIndex_ = 0;
        written_ = 0;
    }

    void clear()
    {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        writeIndex_ = 0;
        written_ = 0;
    }

    void push(float sample)
    {
        assert(!buffer_.empty());
        buffer_[writeIndex_] = sample;
        writeIndex_ = (writeIndex_ + 1) % buffer_.size();
        ++written_;
    }

    float getRecent(std::size_t age) const
    {
        assert(age < buffer_.size());
        const auto index = (writeIndex_ + buffer_.size() - 1 - age) % buffer_.size();
        return buffer_[index];
    }

    // Copies the most recent `count` samples into `dest` in oldest-to-newest
    // order. Allocation-free; safe on the audio thread.
    void copyLatest(float* dest, std::size_t count) const
    {
        assert(count <= buffer_.size());
        for (std::size_t i = 0; i < count; ++i)
            dest[count - 1 - i] = getRecent(i);
    }

    [[nodiscard]] std::size_t capacity() const { return buffer_.size(); }
    [[nodiscard]] std::size_t samplesWritten() const { return written_; }
    [[nodiscard]] bool filled(std::size_t count) const { return written_ >= count; }

private:
    std::vector<float> buffer_;
    std::size_t writeIndex_ = 0;
    std::size_t written_ = 0;
};

} // namespace simplelivetune::dsp
