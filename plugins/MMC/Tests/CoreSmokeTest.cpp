#include "../Source/ChorusCore.hpp"

#include <array>
#include <cmath>
#include <iostream>

int main() {
    // Fixed regression vector at raw 64,0,0,127,0,0,127,64.
    {
        mmcdsp::ChorusCore core;
        core.prepare(44100.0);
        core.setParameters({64.0f / 127.0f, 0.0f, 0.0f, 1.0f,
                              0.0f, 0.0f, 1.0f, 64.0f / 127.0f});
        float left = 0.0f;
        float right = 0.0f;
        core.process(0.25f, -0.25f, left, right);
        const auto q = [](float x) { return static_cast<int>(std::lround(x * 8388608.0f)); };
        if (q(left) != 16383 || q(right) != -16384) {
            std::cerr << "regression mismatch: " << q(left) << ',' << q(right) << '\n';
            return 3;
        }
    }

    // Settled 8192-sample impulse regression at the three principal arrivals.
    {
        mmcdsp::ChorusCore core;
        core.prepare(44100.0);
        core.setParameters({64.0f / 127.0f, 64.0f / 127.0f, 64.0f / 127.0f,
                              1.0f, 0.0f, 0.0f, 1.0f, 64.0f / 127.0f});
        constexpr std::array<int, 7> indices {2048, 2377, 2378, 2516, 2517, 2786, 2787};
        constexpr std::array<int, 7> expected {16383, 61984, 287466, 126959,
                                               216414, 143392, 211883};
        size_t next = 0;
        for (int sample = 0; sample < 8192; ++sample) {
            float left = 0.0f;
            float right = 0.0f;
            const float impulse = sample == 2048 ? 0.25f : 0.0f;
            core.process(impulse, -impulse, left, right);
            if (next < indices.size() && sample == indices[next]) {
                const int actual = static_cast<int>(std::lround(left * 8388608.0f));
                if (std::abs(actual - expected[next]) > 13000) {
                    std::cerr << "settled regression mismatch at " << sample
                              << ": " << actual << " expected " << expected[next] << '\n';
                    return 4;
                }
                ++next;
            }
        }
        if (next != indices.size()) return 5;
    }

    constexpr std::array<double, 4> sampleRates {44100.0, 48000.0, 96000.0, 192000.0};
    constexpr std::array<mmcdsp::Parameters, 3> settings {{
        {},
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
    }};

    for (const auto sampleRate : sampleRates) {
        for (const auto& parameters : settings) {
            mmcdsp::ChorusCore core;
            core.prepare(sampleRate);
            core.setParameters(parameters);
            double energy = 0.0;
            for (int sample = 0; sample < static_cast<int>(sampleRate * 2.0); ++sample) {
                const float impulse = sample == 0 ? 0.5f : 0.0f;
                float left = 0.0f;
                float right = 0.0f;
                core.process(impulse, -impulse, left, right);
                if (!std::isfinite(left) || !std::isfinite(right)) {
                    std::cerr << "non-finite output at " << sampleRate << " Hz\n";
                    return 1;
                }
                energy += static_cast<double>(left * left + right * right);
            }
            if (!std::isfinite(energy))
                return 2;
        }
    }

    std::cout << "MMC core smoke test passed\n";
    return 0;
}
