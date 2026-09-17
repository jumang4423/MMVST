#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mmcdsp {

struct Parameters {
    float del = 64.0f / 127.0f;
    float dep = 0.3f;
    float spd = 0.0f;
    float mix = 1.0f;
    float fb = 1.0f;
    float wid = 1.0f;
    float lp = 1.0f;
    float inp = 1.0f;
};

// Independent reconstruction of the six-tap fixed-point chorus. Controls are
// quantised to the machine's 0..127 domain before use. Its DSP ran at 44.1 kHz;
// delay lengths and phase increments are time-corrected for modern host rates.
class ChorusCore {
public:
    void prepare(double newSampleRate) {
        sampleRate = std::max(8000.0, newSampleRate);
        rateScale = sampleRate / 44100.0;
        reset();
    }

    void reset() {
        for (auto& channel : delay) channel.fill(0.0f);
        writeIndex = 0;
        feedbackState = {};
        phase = 0.0;
        smoothed = rawTarget;
        smoothInitialised = false;
    }

    void setParameters(const Parameters& value) {
        rawTarget = {toRaw(value.del), toRaw(value.dep), toRaw(value.spd), toRaw(value.mix),
                     toRaw(value.fb), toRaw(value.wid), toRaw(value.lp), toRaw(value.inp)};
        if (!smoothInitialised) {
            smoothed = rawTarget;
            smoothInitialised = true;
        }
    }

    void process(float inputLeft, float inputRight, float& outputLeft, float& outputRight) {
        std::array<float, 8> p {};
        for (size_t i = 0; i < p.size(); ++i) {
            smoothed[i] += 0.02f * (rawTarget[i] - smoothed[i]);
            p[i] = smoothed[i] / 128.0f;
        }

        const float inputGain = 4.0f * p[inp] * p[inp];
        const std::array<float, 2> dry {q23(inputLeft * inputGain), q23(inputRight * inputGain)};

        // P:$1477ac..$1477c7: inverted feedback with the original fixed bias.
        const float feedbackGain = -(p[fb] + 0x00fd71 / 8388608.0f);
        const float inputPathGain = 0.5f * (1.0f - feedbackGain);
        for (size_t channel = 0; channel < 2; ++channel)
            delay[channel][writeIndex] = q23(dry[channel] * inputPathGain
                                              + feedbackState[channel] * feedbackGain);

        const float centre = (16.0f + 992.0f * p[del]) * static_cast<float>(rateScale);
        const float excursion = (992.0f * p[dep]) * static_cast<float>(rateScale);
        const float width = p[wid];
        const std::array<double, 3> offsets {0.0, twoPi / 3.0, 4.0 * pi / 3.0};
        std::array<float, 2> wet {};
        for (size_t tap = 0; tap < 3; ++tap) {
            const double leftPhase = phase + offsets[tap];
            const double rightPhase = phase + offsets[tap] + pi * static_cast<double>(width);
            wet[0] += readLinear(0, centre + excursion * static_cast<float>(std::sin(leftPhase))) * oneThird;
            wet[1] += readLinear(1, centre + excursion * static_cast<float>(std::sin(rightPhase))) * oneThird;
        }
        wet[0] = q23(wet[0]);
        wet[1] = q23(wet[1]);

        const auto lpIndex = static_cast<size_t>(std::clamp(rawTarget[lp], 0.0f, 127.0f));
        const float coefficient = lpCoefficients()[lpIndex];
        for (size_t channel = 0; channel < 2; ++channel)
            feedbackState[channel] = q23(feedbackState[channel]
                                           + coefficient * (wet[channel] - feedbackState[channel]));

        const float mixAmount = p[mix];
        const float dryAmount = (8388607.0f - smoothed[mix] * 65536.0f) / 8388608.0f;
        outputLeft = q23(dry[0] * dryAmount + wet[0] * mixAmount);
        outputRight = q23(dry[1] * dryAmount + wet[1] * mixAmount);

        writeIndex = (writeIndex + 1) & ringMask;
        phase += twoPi * (p[spd] * p[spd] * (0x956 / 8388608.0)) / rateScale;
        if (phase >= twoPi) phase -= twoPi;
    }

private:
    enum Param : size_t { del, dep, spd, mix, fb, wid, lp, inp };
    static constexpr size_t ringSize = 8192;
    static constexpr size_t ringMask = ringSize - 1;
    static constexpr double pi = 3.1415926535897932384626433832795;
    static constexpr double twoPi = 2.0 * pi;
    static constexpr float oneThird = 0x2aaaab / 8388608.0f;

    static float toRaw(float unit) {
        return static_cast<float>(std::lround(std::clamp(unit, 0.0f, 1.0f) * 127.0f));
    }

    static float q23(float value) {
        value = std::clamp(value, -1.0f, 8388607.0f / 8388608.0f);
        return static_cast<float>(std::floor(value * 8388608.0f)) / 8388608.0f;
    }

    float readLinear(size_t channel, float delaySamples) const {
        delaySamples = std::clamp(delaySamples, 1.0f, static_cast<float>(ringSize - 2));
        float position = static_cast<float>(writeIndex) - delaySamples;
        while (position < 0.0f) position += static_cast<float>(ringSize);
        const auto index0 = static_cast<size_t>(position) & ringMask;
        const auto index1 = (index0 + 1) & ringMask;
        const float fraction = position - std::floor(position);
        return q23(delay[channel][index0]
                   + fraction * (delay[channel][index1] - delay[channel][index0]));
    }

    // Analytic fit through recovered table endpoints and centre; no ROM bytes.
    static const std::array<float, 128>& lpCoefficients() {
        static const std::array<float, 128> table = [] {
            std::array<float, 128> values {};
            constexpr float minimum = 0.00221133f;
            constexpr float midpoint = 0.09420741f;
            const float ratio = midpoint / minimum;
            for (size_t i = 0; i < values.size(); ++i) {
                const float x = static_cast<float>(i) / 64.0f;
                values[i] = std::min(1.0f, minimum * std::pow(ratio, x));
            }
            return values;
        }();
        return table;
    }

    double sampleRate = 44100.0;
    double rateScale = 1.0;
    std::array<std::array<float, ringSize>, 2> delay {};
    size_t writeIndex = 0;
    std::array<float, 2> feedbackState {};
    std::array<float, 8> rawTarget {64, 38, 0, 127, 127, 127, 127, 127};
    std::array<float, 8> smoothed {};
    double phase = 0.0;
    bool smoothInitialised = false;
};

} // namespace mmcdsp
