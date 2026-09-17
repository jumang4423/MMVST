#include "../Source/ChorusCore.hpp"

#include <array>
#include <cmath>
#include <iostream>

int main() {
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
