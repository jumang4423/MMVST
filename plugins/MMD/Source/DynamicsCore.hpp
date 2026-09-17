#pragma once

#include <algorithm>
#include <cmath>

namespace mmddsp {

struct Parameters {
    float atk = 64.0f / 127.0f;
    float rel = 20.0f / 127.0f;
    float thrs = 88.0f / 127.0f;
    float mix = 1.0f;
    float rat = 112.0f / 127.0f;
    float gain = 48.0f / 127.0f;
    float rms = 16.0f / 127.0f;
    float inp = 64.0f / 127.0f;
};

class DynamicsCore {
public:
    void prepare(double newSampleRate) {
        sampleRate = std::max(8000.0, newSampleRate);
        reset();
        smoothed = desired;
        smoothInitialised = false;
    }

    void reset() {
        rmsPower = 0.0f;
        reductionDb = 0.0f;
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

        // Controls are normalized to 0..1; time and ratio use musical curves.
        const float attackSeconds = exponential(smoothed.atk, 0.0005f, 0.100f);
        const float releaseSeconds = exponential(smoothed.rel, 0.050f, 5.0f);
        const float thresholdDb = -63.5f + 63.5f * smoothed.thrs;
        const float ratio = 255.0f / (255.0f - 254.0f * smoothed.rat);
        const float makeupDb = 24.0f * smoothed.gain;
        // Raw 64 is exactly unity; endpoints are approximately -12/+11.8 dB.
        const float inputDb = ((smoothed.inp * 127.0f) - 64.0f) * (12.0f / 64.0f);
        const float inputGain = dbToGain(inputDb);
        const float dryLeft = inputLeft * inputGain;
        const float dryRight = inputRight * inputGain;

        const float peak = std::max(std::abs(dryLeft), std::abs(dryRight));
        const float power = 0.5f * (dryLeft * dryLeft + dryRight * dryRight);
        const float rmsSeconds = 0.0001f + 0.1999f * smoothed.rms * smoothed.rms;
        const float rmsCoefficient = timeCoefficient(rmsSeconds);
        rmsPower = power + rmsCoefficient * (rmsPower - power);
        const float rmsLevel = std::sqrt(std::max(0.0f, rmsPower));
        const float detector = peak + smoothed.rms * (rmsLevel - peak);
        const float levelDb = gainToDb(detector);
        const float overDb = std::max(0.0f, levelDb - thresholdDb);
        const float targetReductionDb = overDb * (1.0f - 1.0f / ratio);
        const float coefficient = targetReductionDb > reductionDb
            ? timeCoefficient(attackSeconds)
            : timeCoefficient(releaseSeconds);
        reductionDb = targetReductionDb + coefficient * (reductionDb - targetReductionDb);

        const float compressedGain = dbToGain(makeupDb - reductionDb);
        const float wetLeft = std::tanh(dryLeft * compressedGain);
        const float wetRight = std::tanh(dryRight * compressedGain);
        outputLeft = dryLeft + smoothed.mix * (wetLeft - dryLeft);
        outputRight = dryRight + smoothed.mix * (wetRight - dryRight);
    }

private:
    static float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

    static Parameters clampParameters(Parameters value) {
        value.atk = clamp01(value.atk);
        value.rel = clamp01(value.rel);
        value.thrs = clamp01(value.thrs);
        value.mix = clamp01(value.mix);
        value.rat = clamp01(value.rat);
        value.gain = clamp01(value.gain);
        value.rms = clamp01(value.rms);
        value.inp = clamp01(value.inp);
        return value;
    }

    static float exponential(float unit, float minimum, float maximum) {
        return minimum * std::pow(maximum / minimum, unit);
    }

    static float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
    static float gainToDb(float gain) {
        return 20.0f * std::log10(std::max(gain, 1.0e-9f));
    }

    float timeCoefficient(float seconds) const {
        return std::exp(-1.0f / static_cast<float>(sampleRate * seconds));
    }

    void smoothParameters() {
        const float coefficient = timeCoefficient(0.030f);
        const auto step = [coefficient](float current, float target) {
            return target + coefficient * (current - target);
        };
        smoothed.atk = step(smoothed.atk, desired.atk);
        smoothed.rel = step(smoothed.rel, desired.rel);
        smoothed.thrs = step(smoothed.thrs, desired.thrs);
        smoothed.mix = step(smoothed.mix, desired.mix);
        smoothed.rat = step(smoothed.rat, desired.rat);
        smoothed.gain = step(smoothed.gain, desired.gain);
        smoothed.rms = step(smoothed.rms, desired.rms);
        smoothed.inp = step(smoothed.inp, desired.inp);
    }

    double sampleRate = 48000.0;
    float rmsPower = 0.0f;
    float reductionDb = 0.0f;
    Parameters desired {};
    Parameters smoothed {};
    bool smoothInitialised = false;
};

} // namespace mmddsp
