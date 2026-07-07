#include "engine/TuneEngine.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct WavData {
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    std::vector<float> samples;
};

uint32_t readU32(std::istream& in)
{
    uint8_t b[4] {};
    in.read(reinterpret_cast<char*>(b), 4);
    return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
}

uint16_t readU16(std::istream& in)
{
    uint8_t b[2] {};
    in.read(reinterpret_cast<char*>(b), 2);
    return static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
}

void writeU32(std::ostream& out, uint32_t value)
{
    const uint8_t b[4] { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24) };
    out.write(reinterpret_cast<const char*>(b), 4);
}

void writeU16(std::ostream& out, uint16_t value)
{
    const uint8_t b[2] { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8) };
    out.write(reinterpret_cast<const char*>(b), 2);
}

bool readWav(const std::string& path, WavData& wav)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;

    char id[4] {};
    in.read(id, 4);
    readU32(in);
    char wave[4] {};
    in.read(wave, 4);
    if (std::string(id, 4) != "RIFF" || std::string(wave, 4) != "WAVE")
        return false;

    uint16_t format = 0;
    uint16_t bits = 0;
    std::vector<int16_t> pcm;
    while (in && !in.eof()) {
        in.read(id, 4);
        if (in.gcount() != 4)
            break;
        const auto size = readU32(in);
        if (std::string(id, 4) == "fmt ") {
            format = readU16(in);
            wav.channels = readU16(in);
            wav.sampleRate = readU32(in);
            readU32(in);
            readU16(in);
            bits = readU16(in);
            in.ignore(size > 16 ? size - 16 : 0);
        } else if (std::string(id, 4) == "data") {
            if (format != 1 || bits != 16 || wav.channels == 0)
                return false;
            pcm.resize(size / sizeof(int16_t));
            in.read(reinterpret_cast<char*>(pcm.data()), static_cast<std::streamsize>(size));
        } else {
            in.ignore(size);
        }
        if (size % 2 == 1)
            in.ignore(1);
    }

    wav.samples.resize(pcm.size());
    for (std::size_t i = 0; i < pcm.size(); ++i)
        wav.samples[i] = static_cast<float>(pcm[i]) / 32768.0f;
    return !wav.samples.empty();
}

bool writeWav(const std::string& path, const WavData& wav)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;

    const auto dataBytes = static_cast<uint32_t>(wav.samples.size() * sizeof(int16_t));
    out.write("RIFF", 4);
    writeU32(out, 36 + dataBytes);
    out.write("WAVEfmt ", 8);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, wav.channels);
    writeU32(out, wav.sampleRate);
    writeU32(out, wav.sampleRate * wav.channels * 2);
    writeU16(out, wav.channels * 2);
    writeU16(out, 16);
    out.write("data", 4);
    writeU32(out, dataBytes);

    for (auto sample : wav.samples) {
        const auto clamped = std::clamp(sample, -1.0f, 1.0f);
        const auto pcm = static_cast<int16_t>(clamped * 32767.0f);
        out.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
    }
    return true;
}
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: offline_tune <input-16bit-wav> <output-wav>\n";
        return 2;
    }

    WavData wav;
    if (!readWav(argv[1], wav)) {
        std::cerr << "failed to read 16-bit PCM WAV: " << argv[1] << "\n";
        return 1;
    }

    if (wav.channels > 1)
        std::cerr << "note: processing left channel only; result is duplicated to all " << wav.channels << " channels\n";

    simplelivetune::engine::TuneEngine engine;
    engine.prepare(wav.sampleRate, 512);
    simplelivetune::engine::TuneParameters params;
    params.scale = simplelivetune::dsp::ScaleType::Chromatic;
    params.retuneTimeMs = 60.0f;
    engine.setParameters(params);

    std::vector<float> mono(wav.samples.size() / wav.channels);
    for (std::size_t frame = 0; frame < mono.size(); ++frame)
        mono[frame] = wav.samples[frame * wav.channels];

    std::vector<float> processed(mono.size());
    for (std::size_t pos = 0; pos < mono.size(); pos += 512) {
        const auto count = static_cast<int>(std::min<std::size_t>(512, mono.size() - pos));
        engine.process(mono.data() + pos, processed.data() + pos, count);
    }

    for (std::size_t frame = 0; frame < mono.size(); ++frame)
        for (uint16_t ch = 0; ch < wav.channels; ++ch)
            wav.samples[frame * wav.channels + ch] = processed[frame];

    if (!writeWav(argv[2], wav)) {
        std::cerr << "failed to write WAV: " << argv[2] << "\n";
        return 1;
    }
}
