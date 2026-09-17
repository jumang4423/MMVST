#include "PluginProcessor.hpp"

namespace {
constexpr auto atkId = "atk"; constexpr auto relId = "rel";
constexpr auto thrsId = "thrs"; constexpr auto mixId = "mix";
constexpr auto ratId = "rat"; constexpr auto gainId = "gain";
constexpr auto rmsId = "rms"; constexpr auto inpId = "inp";
constexpr float factoryMid = 64.0f / 127.0f;
}

MMDAudioProcessor::MMDAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "MMDState", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout MMDAudioProcessor::createParameterLayout() {
    using Parameter = juce::AudioParameterFloat;
    using ID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto unitRange = juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f / 127.0f);
    const auto make = [&layout, &unitRange](const char* id, const char* name, float value) {
        layout.add(std::make_unique<Parameter>(ID(id, 1), name, unitRange, value));
    };
    make(atkId, "ATK", 64.0f / 127.0f); make(relId, "REL", 20.0f / 127.0f);
    make(thrsId, "THRS", 88.0f / 127.0f); make(mixId, "MIX", 1.0f);
    make(ratId, "RAT", 112.0f / 127.0f); make(gainId, "GAIN", 48.0f / 127.0f);
    make(rmsId, "RMS", 16.0f / 127.0f); make(inpId, "INP", factoryMid);
    return layout;
}

void MMDAudioProcessor::prepareToPlay(double sampleRate, int) {
    core.prepare(sampleRate);
    core.setParameters(readParameters());
}

bool MMDAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo());
}

mmddsp::Parameters MMDAudioProcessor::readParameters() const {
    const auto value = [this](const char* id) {
        return parameters.getRawParameterValue(id)->load();
    };
    return {value(atkId), value(relId), value(thrsId), value(mixId),
            value(ratId), value(gainId), value(rmsId), value(inpId)};
}

void MMDAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int channels = buffer.getNumChannels();
    if (channels == 0) return;
    core.setParameters(readParameters());
    auto* left = buffer.getWritePointer(0);
    auto* right = channels > 1 ? buffer.getWritePointer(1) : nullptr;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float outLeft = 0.0f, outRight = 0.0f;
        core.process(left[i], right != nullptr ? right[i] : left[i], outLeft, outRight);
        left[i] = right != nullptr ? outLeft : 0.5f * (outLeft + outRight);
        if (right != nullptr) right[i] = outRight;
    }
}

juce::AudioProcessorEditor* MMDAudioProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

void MMDAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, destination);
}

void MMDAudioProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml != nullptr)
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MMDAudioProcessor(); }
