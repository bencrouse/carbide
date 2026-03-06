#pragma once

#include <array>
#include <juce_core/juce_core.h>
#include <vector>

namespace FactoryPresets
{
struct Preset
{
    juce::String name;
    std::array<float, 9> values {};
    int mode = 1; // 0=Soft, 1=Neutral, 2=Hard
};

std::vector<Preset> getPresets();
} // namespace FactoryPresets
