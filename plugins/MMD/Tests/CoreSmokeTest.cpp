#include "../Source/DynamicsCore.hpp"
#include <array>
#include <cmath>
#include <iostream>

int main() {
    constexpr std::array<double, 4> rates {44100.0, 48000.0, 96000.0, 192000.0};
    constexpr std::array<mmddsp::Parameters, 3> settings {{
        {},
        {0,0,0,0,0,0,0,0},
        {1,1,1,1,1,1,1,1}
    }};
    for (const double rate : rates) for (const auto& params : settings) {
        mmddsp::DynamicsCore core;
        core.prepare(rate); core.setParameters(params);
        double energy = 0.0;
        for (int i = 0; i < static_cast<int>(rate * 2.0); ++i) {
            const float input = i == 0 ? 0.5f : 0.2f * std::sin(2.0 * 3.141592653589793 * 997.0 * i / rate);
            float left = 0, right = 0;
            core.process(input, -input, left, right);
            if (!std::isfinite(left) || !std::isfinite(right)) return 1;
            energy += left * left + right * right;
        }
        if (!std::isfinite(energy)) return 2;
    }
    std::cout << "MMD core smoke test passed\n";
}
