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

// Six-tap chorus core. Controls are quantised to 128 steps. Delay lengths and
// phase increments are time-corrected for modern host rates.
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
        phase = 5.92750579;
        blockPosition = 0;
        startupBlocks = 0;
        delayState = rawTarget[del] / 128.0f;
        depthState = 0.0f;
        widthState = 0.0f;
        smoothInitialised = false;
    }

    void setParameters(const Parameters& value) {
        rawTarget = {toRaw(value.del), toRaw(value.dep), toRaw(value.spd), toRaw(value.mix),
                     toRaw(value.fb), toRaw(value.wid), toRaw(value.lp), toRaw(value.inp)};
        if (!smoothInitialised) {
            delayState = rawTarget[del] / 128.0f;
            smoothInitialised = true;
        }
    }

    void process(float inputLeft, float inputRight, float& outputLeft, float& outputRight) {
        std::array<float, 8> p {};
        for (size_t i = 0; i < p.size(); ++i) p[i] = rawTarget[i] / 128.0f;
        if (blockPosition == 0) {
            delayState += 0.02f * (p[del] - delayState);
            depthState += 0.02f * (p[dep] - depthState);
            widthState += 0.02f * (p[wid] - widthState);
        }

        const float inputGain = 4.0f * p[inp] * p[inp];
        const std::array<float, 2> dry {q23(inputLeft * inputGain), q23(inputRight * inputGain)};

        // Inverted feedback with a small fixed bias.
        const float feedbackGain = -(p[fb] + 0x00fd71 / 8388608.0f);
        const float inputPathGain = 0.5f * (1.0f - feedbackGain);
        for (size_t channel = 0; channel < 2; ++channel)
            delay[channel][writeIndex] = q23(dry[channel] * inputPathGain
                                              + feedbackState[channel] * feedbackGain);

        const float centre = (14.91992352f + 992.0f * delayState) * static_cast<float>(rateScale);
        const float excursion = (502.34559759f * depthState) * static_cast<float>(rateScale);
        const float width = widthState;
        const std::array<double, 3> offsets {0.0, twoPi / 3.0, 4.0 * pi / 3.0};
        const double phasePerSample = (p[spd] * p[spd] * (0x956 / 8388608.0)) / rateScale;
        const double nextPhase = phase + phasePerSample * 16.0;
        const float blockFraction = static_cast<float>(blockPosition) / 16.0f;
        std::array<float, 2> wet {};
        for (size_t tap = 0; tap < 3; ++tap) {
            const double leftPhase = phase + offsets[tap];
            const double rightPhase = leftPhase + pi * static_cast<double>(width);
            const float leftMod = static_cast<float>(std::sin(leftPhase))
                + blockFraction * static_cast<float>(std::sin(nextPhase + offsets[tap]) - std::sin(leftPhase));
            const float rightMod = static_cast<float>(std::sin(rightPhase))
                + blockFraction * static_cast<float>(std::sin(nextPhase + offsets[tap]
                    + pi * static_cast<double>(width)) - std::sin(rightPhase));
            wet[0] += readLinear(0, centre + excursion * leftMod) * oneThird;
            wet[1] += readLinear(1, centre + excursion * rightMod) * oneThird;
        }
        wet[0] = q23(wet[0]);
        wet[1] = q23(wet[1]);
        if (startupBlocks < 128) wet = {};

        const auto lpIndex = static_cast<size_t>(std::clamp(rawTarget[lp], 0.0f, 127.0f));
        const float coefficient = lpCoefficients()[lpIndex];
        for (size_t channel = 0; channel < 2; ++channel)
            feedbackState[channel] = q23(feedbackState[channel]
                                           + coefficient * (wet[channel] - feedbackState[channel]));

        const float mixAmount = p[mix];
        const float dryAmount = (8388607.0f - rawTarget[mix] * 65536.0f) / 8388608.0f;
        outputLeft = q23(dry[0] * dryAmount + wet[0] * mixAmount);
        outputRight = q23(dry[1] * dryAmount + wet[1] * mixAmount);

        writeIndex = (writeIndex + 1) & ringMask;
        if (++blockPosition == 16) {
            blockPosition = 0;
            phase = std::fmod(nextPhase, twoPi);
            if (startupBlocks < 128) ++startupBlocks;
        }
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

    static const std::array<float, 128>& lpCoefficients() {
        static const std::array<float, 128> table = [] {
            std::array<float, 128> values {};
            for (size_t i = 0; i < values.size(); ++i) {
                const float x = static_cast<float>(i) / 127.0f;
                const float logPole = ((((((-11.8579733f * x + 26.22141973f) * x
                    - 22.32888259f) * x + 8.98576210f) * x - 1.69233625f) * x
                    + 7.66680329f) * x - 6.11510228f);
                values[i] = 1.0f - std::exp(-std::exp(logPole));
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
    double phase = 5.92750579;
    unsigned blockPosition = 0;
    unsigned startupBlocks = 0;
    float delayState = 0.5f;
    float depthState = 0.0f;
    float widthState = 0.0f;
    bool smoothInitialised = false;
};

} // namespace mmcdsp
