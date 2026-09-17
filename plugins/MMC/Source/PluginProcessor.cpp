#include "PluginProcessor.hpp"

namespace {
constexpr auto delId = "del";
constexpr auto depId = "dep";
constexpr auto spdId = "spd";
constexpr auto mixId = "mix";
constexpr auto fbId = "fb";
constexpr auto widId = "wid";
constexpr auto lpId = "lp";
constexpr auto inpId = "inp";
constexpr float factoryMid = 64.0f / 127.0f;
}

MMCAudioProcessor::MMCAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "MMCState", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout MMCAudioProcessor::createParameterLayout() {
    using Parameter = juce::AudioParameterFloat;
    using ID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto unitRange = juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f);
    const auto make = [&layout, &unitRange](const char* id, const char* name, float defaultValue) {
        layout.add(std::make_unique<Parameter>(ID(id, 1), name, unitRange, defaultValue));
    };
    make(delId, "DEL", factoryMid);
    make(depId, "DEP", 0.3f);
    make(spdId, "SPD", 0.0f);
    make(mixId, "MIX", 1.0f);
    make(fbId, "FB", 1.0f);
    make(widId, "WID", 1.0f);
    make(lpId, "LP", 1.0f);
    make(inpId, "INP", 1.0f);
    return layout;
}

void MMCAudioProcessor::prepareToPlay(double sampleRate, int) {
    core.prepare(sampleRate);
    core.setParameters(readParameters());
}

bool MMCAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo());
}

mmcdsp::Parameters MMCAudioProcessor::readParameters() const {
    const auto value = [this](const char* id) {
        return parameters.getRawParameterValue(id)->load();
    };
    return {value(delId), value(depId), value(spdId), value(mixId),
            value(fbId), value(widId), value(lpId), value(inpId)};
}

void MMCAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int channels = buffer.getNumChannels();
    if (channels == 0) {
        buffer.clear();
        return;
    }

    core.setParameters(readParameters());
    auto* left = buffer.getWritePointer(0);
    auto* right = channels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        const float inputLeft = left[sample];
        const float inputRight = right != nullptr ? right[sample] : inputLeft;
        float outputLeft = 0.0f;
        float outputRight = 0.0f;
        core.process(inputLeft, inputRight, outputLeft, outputRight);
        left[sample] = right != nullptr ? outputLeft : 0.5f * (outputLeft + outputRight);
        if (right != nullptr)
            right[sample] = outputRight;
    }
}

juce::AudioProcessorEditor* MMCAudioProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

void MMCAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destination);
}

void MMCAudioProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml != nullptr)
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MMCAudioProcessor();
}
