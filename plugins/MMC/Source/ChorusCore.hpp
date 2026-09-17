#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

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

class ChorusCore {
public:
    void prepare(double newSampleRate) {
        sampleRate = std::max(8000.0, newSampleRate);
        const auto size = static_cast<size_t>(std::ceil(sampleRate * maxDelaySeconds)) + 8;
        for (auto& channel : delay)
            channel.assign(size, 0.0f);
        writeIndex = 0;
        phases = {0.0, twoPi / 3.0, 2.0 * twoPi / 3.0};
        feedbackLowpass = {0.0f, 0.0f};
        smoothed = {};
        smoothInitialised = false;
    }

    void reset() {
        for (auto& channel : delay)
            std::fill(channel.begin(), channel.end(), 0.0f);
        writeIndex = 0;
        feedbackLowpass = {0.0f, 0.0f};
        phases = {0.0, twoPi / 3.0, 2.0 * twoPi / 3.0};
    }

    void setParameters(const Parameters& target) {
        desired = clampParameters(target);
        if (!smoothInitialised) {
            smoothed = desired;
            smoothInitialised = true;
        }
    }

    void process(float inputLeft, float inputRight, float& outputLeft, float& outputRight) {
        smoothParameters();

        const float delaySeconds = exponential(smoothed.del, 0.0006f, 0.025f);
        const float depthSeconds = smoothed.dep * 0.012f;
        const float speedHz = exponential(smoothed.spd, 0.025f, 8.0f);
        const float feedbackGain = smoothed.fb * 0.99f;
        const float cutoffHz = std::min(exponential(smoothed.lp, 180.0f, 20000.0f),
                                        static_cast<float>(sampleRate * 0.45));
        const float inputGain = std::pow(2.0f, ((smoothed.inp * 127.0f) - 64.0f) / 32.0f);
        const float dryLeft = inputLeft * inputGain;
        const float dryRight = inputRight * inputGain;

        std::array<float, tapCount> leftTimes {};
        std::array<float, tapCount> rightTimes {};
        constexpr std::array<double, tapCount> rateRatios {0.93, 1.0, 1.071};

        for (size_t tap = 0; tap < tapCount; ++tap) {
            const double leftPhase = phases[tap];
            const double rightPhase = phases[tapCount - 1 - tap]
                + pi * (0.35 + 0.65 * smoothed.wid);
            leftTimes[tap] = std::clamp(delaySeconds + depthSeconds * static_cast<float>(std::sin(leftPhase)),
                                        0.0001f, maxDelaySeconds - 0.001f);
            rightTimes[tap] = std::clamp(delaySeconds + depthSeconds * static_cast<float>(std::sin(rightPhase)),
                                         0.0001f, maxDelaySeconds - 0.001f);
        }

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (size_t tap = 0; tap < tapCount; ++tap) {
            wetLeft += readDelay(0, leftTimes[tap] * static_cast<float>(sampleRate));
            wetRight += readDelay(1, rightTimes[tap] * static_cast<float>(sampleRate));
        }
        wetLeft /= static_cast<float>(tapCount);
        wetRight /= static_cast<float>(tapCount);

        const float lpCoefficient = std::exp(-2.0f * static_cast<float>(pi) * cutoffHz
                                             / static_cast<float>(sampleRate));
        feedbackLowpass[0] = (1.0f - lpCoefficient) * wetLeft
            + lpCoefficient * feedbackLowpass[0];
        feedbackLowpass[1] = (1.0f - lpCoefficient) * wetRight
            + lpCoefficient * feedbackLowpass[1];

        delay[0][writeIndex] = std::tanh(dryLeft + feedbackLowpass[0] * feedbackGain);
        delay[1][writeIndex] = std::tanh(dryRight + feedbackLowpass[1] * feedbackGain);
        writeIndex = (writeIndex + 1) % delay[0].size();

        const float mid = 0.5f * (wetLeft + wetRight);
        const float side = 0.5f * (wetLeft - wetRight) * smoothed.wid;
        const float wideLeft = mid + side;
        const float wideRight = mid - side;
        const float dryGain = std::cos(smoothed.mix * static_cast<float>(pi * 0.5));
        const float wetGain = std::sin(smoothed.mix * static_cast<float>(pi * 0.5));
        outputLeft = dryLeft * dryGain + wideLeft * wetGain;
        outputRight = dryRight * dryGain + wideRight * wetGain;

        for (size_t tap = 0; tap < tapCount; ++tap) {
            phases[tap] += twoPi * speedHz * rateRatios[tap] / sampleRate;
            if (phases[tap] >= twoPi)
                phases[tap] -= twoPi;
        }
    }

private:
    static constexpr size_t tapCount = 3;
    static constexpr float maxDelaySeconds = 0.04f;
    static constexpr double pi = 3.1415926535897932384626433832795;
    static constexpr double twoPi = 2.0 * pi;

    static float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

    static Parameters clampParameters(Parameters value) {
        value.del = clamp01(value.del);
        value.dep = clamp01(value.dep);
        value.spd = clamp01(value.spd);
        value.mix = clamp01(value.mix);
        value.fb = clamp01(value.fb);
        value.wid = clamp01(value.wid);
        value.lp = clamp01(value.lp);
        value.inp = clamp01(value.inp);
        return value;
    }

    static float exponential(float unit, float minimum, float maximum) {
        return minimum * std::pow(maximum / minimum, unit);
    }

    void smoothParameters() {
        const float coefficient = std::exp(-1.0f / static_cast<float>(sampleRate * 0.04));
        const auto step = [coefficient](float current, float target) {
            return target + coefficient * (current - target);
        };
        smoothed.del = step(smoothed.del, desired.del);
        smoothed.dep = step(smoothed.dep, desired.dep);
        smoothed.spd = step(smoothed.spd, desired.spd);
        smoothed.mix = step(smoothed.mix, desired.mix);
        smoothed.fb = step(smoothed.fb, desired.fb);
        smoothed.wid = step(smoothed.wid, desired.wid);
        smoothed.lp = step(smoothed.lp, desired.lp);
        smoothed.inp = step(smoothed.inp, desired.inp);
    }

    float readDelay(size_t channel, float delaySamples) const {
        const auto size = static_cast<int>(delay[channel].size());
        float readPosition = static_cast<float>(writeIndex) - delaySamples;
        while (readPosition < 0.0f)
            readPosition += static_cast<float>(size);
        const int index1 = static_cast<int>(readPosition);
        const float fraction = readPosition - static_cast<float>(index1);
        const int index0 = (index1 - 1 + size) % size;
        const int index2 = (index1 + 1) % size;
        const int index3 = (index1 + 2) % size;
        const float y0 = delay[channel][static_cast<size_t>(index0)];
        const float y1 = delay[channel][static_cast<size_t>(index1)];
        const float y2 = delay[channel][static_cast<size_t>(index2)];
        const float y3 = delay[channel][static_cast<size_t>(index3)];
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
    }

    double sampleRate = 48000.0;
    std::array<std::vector<float>, 2> delay;
    size_t writeIndex = 0;
    std::array<double, tapCount> phases {};
    std::array<float, 2> feedbackLowpass {};
    Parameters desired {};
    Parameters smoothed {};
    bool smoothInitialised = false;
};

} // namespace mmcdsp
