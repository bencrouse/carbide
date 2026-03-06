#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr std::array<const char*, 9> kParameterIds {
    "pitch",
    "decay",
    "punch",
    "click",
    "material",
    "drive",
    "tone",
    "sub",
    "output"
};

constexpr std::array<float, 9> kSmoothingSeconds {
    0.020f, // pitch
    0.030f, // decay
    0.020f, // punch
    0.010f, // click
    0.020f, // material
    0.015f, // drive
    0.030f, // tone
    0.030f, // sub
    0.010f  // output
};
} // namespace

CarbideAudioProcessor::CarbideAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (size_t i = 0; i < kParameterIds.size(); ++i)
        rawParameterValues[i] = apvts.getRawParameterValue(kParameterIds[i]);
    modeChoiceParameter = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("mode"));

    const auto presets = FactoryPresets::getPresets();
    presetNames.reserve(presets.size());
    for (const auto& preset : presets)
        presetNames.push_back(preset.name);

    applyPreset(0);
}

void CarbideAudioProcessor::prepareToPlay(const double sampleRate, const int /*samplesPerBlock*/)
{
    voice.prepare(sampleRate);
    for (size_t i = 0; i < parameterSmoothers.size(); ++i)
        parameterSmoothers[i].reset(sampleRate, kSmoothingSeconds[i]);

    syncSmoothersFromParameters(true);
}

void CarbideAudioProcessor::releaseResources()
{
}

bool CarbideAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CarbideAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    syncSmoothersFromParameters(false);

    float peak = 0.0f;
    double rmsAccum = 0.0;

    int sampleIndex = 0;
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const auto messagePos = std::clamp(metadata.samplePosition, 0, buffer.getNumSamples());

        while (sampleIndex < messagePos)
        {
            const float sample = renderVoiceSample();
            for (int ch = 0; ch < totalNumOutputChannels; ++ch)
                buffer.setSample(ch, sampleIndex, sample);
            waveform.pushSample(sample);
            peak = std::max(peak, std::abs(sample));
            rmsAccum += static_cast<double>(sample * sample);
            ++sampleIndex;
        }

        if (message.isNoteOn())
        {
            const auto velocity = juce::jlimit(0.0f, 1.0f, message.getFloatVelocity());
            voice.trigger(message.getNoteNumber(), velocity);
        }
    }

    while (sampleIndex < buffer.getNumSamples())
    {
        const float sample = renderVoiceSample();
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            buffer.setSample(ch, sampleIndex, sample);
        waveform.pushSample(sample);
        peak = std::max(peak, std::abs(sample));
        rmsAccum += static_cast<double>(sample * sample);
        ++sampleIndex;
    }

    midiMessages.clear();

    const float rms = std::sqrt(static_cast<float>(rmsAccum / std::max(1, buffer.getNumSamples())));
    const float oldPeak = peakMeter.load(std::memory_order_relaxed);
    const float oldRms = rmsMeter.load(std::memory_order_relaxed);
    peakMeter.store(std::max(peak, oldPeak * 0.82f), std::memory_order_relaxed);
    rmsMeter.store((oldRms * 0.88f) + (rms * 0.12f), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* CarbideAudioProcessor::createEditor()
{
    return new CarbideAudioProcessorEditor(*this);
}

bool CarbideAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String CarbideAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CarbideAudioProcessor::acceptsMidi() const
{
    return true;
}

bool CarbideAudioProcessor::producesMidi() const
{
    return false;
}

bool CarbideAudioProcessor::isMidiEffect() const
{
    return false;
}

double CarbideAudioProcessor::getTailLengthSeconds() const
{
    return 0.5;
}

int CarbideAudioProcessor::getNumPrograms()
{
    return std::max(1, static_cast<int>(presetNames.size()));
}

int CarbideAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void CarbideAudioProcessor::setCurrentProgram(const int index)
{
    applyPreset(index);
}

const juce::String CarbideAudioProcessor::getProgramName(const int index)
{
    if (index < 0 || index >= static_cast<int>(presetNames.size()))
        return {};
    return presetNames[static_cast<size_t>(index)];
}

void CarbideAudioProcessor::changeProgramName(const int index, const juce::String& newName)
{
    if (index < 0 || index >= static_cast<int>(presetNames.size()))
        return;
    presetNames[static_cast<size_t>(index)] = newName;
}

void CarbideAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("currentProgram", currentProgram, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CarbideAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName(apvts.state.getType()))
        return;

    apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    const int restoredProgram = static_cast<int>(apvts.state.getProperty("currentProgram", 0));
    currentProgram = std::clamp(restoredProgram, 0, getNumPrograms() - 1);
    syncSmoothersFromParameters(true);
}

juce::AudioProcessorValueTreeState& CarbideAudioProcessor::getState() noexcept
{
    return apvts;
}

float CarbideAudioProcessor::getPeakMeter() const noexcept
{
    return juce::jlimit(0.0f, 1.0f, peakMeter.load(std::memory_order_relaxed));
}

float CarbideAudioProcessor::getRmsMeter() const noexcept
{
    return juce::jlimit(0.0f, 1.0f, rmsMeter.load(std::memory_order_relaxed));
}

void CarbideAudioProcessor::getWaveformSnapshot(std::array<float, WaveformCapture::kSize>& target) const noexcept
{
    waveform.snapshot(target);
}

juce::AudioProcessorValueTreeState::ParameterLayout CarbideAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(kParameterCount);

    auto make = [&](const char* id, const char* name, const float def) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(id, 1), name, juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def));
    };

    make("pitch", "Pitch", 0.50f);
    make("decay", "Decay", 0.22f);
    make("punch", "Punch", 0.75f);
    make("click", "Click", 0.65f);
    make("material", "Material", 0.35f);
    make("drive", "Drive", 0.30f);
    make("tone", "Tone", 0.55f);
    make("sub", "Sub", 0.45f);
    make("output", "Output", 0.70f);
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1), "Mode", juce::StringArray { "Soft", "Neutral", "Hard" }, 1));

    return { params.begin(), params.end() };
}

void CarbideAudioProcessor::applyPreset(const int index)
{
    const auto presets = FactoryPresets::getPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;

    currentProgram = index;
    const auto& preset = presets[static_cast<size_t>(index)];
    for (size_t i = 0; i < kParameterIds.size(); ++i)
    {
        if (auto* parameter = apvts.getParameter(kParameterIds[i]))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(preset.values[i]);
            parameter->endChangeGesture();
        }
    }

    if (auto* modeParam = apvts.getParameter("mode"))
    {
        modeParam->beginChangeGesture();
        modeParam->setValueNotifyingHost(static_cast<float>(std::clamp(preset.mode, 0, 2)) / 2.0f);
        modeParam->endChangeGesture();
    }
}

KickSynthVoice::Params CarbideAudioProcessor::readSmoothedParams() noexcept
{
    KickSynthVoice::Params p;
    p.pitch = parameterSmoothers[0].getNextValue();
    p.decay = parameterSmoothers[1].getNextValue();
    p.punch = parameterSmoothers[2].getNextValue();
    p.click = parameterSmoothers[3].getNextValue();
    p.material = parameterSmoothers[4].getNextValue();
    p.drive = parameterSmoothers[5].getNextValue();
    p.tone = parameterSmoothers[6].getNextValue();
    p.sub = parameterSmoothers[7].getNextValue();
    p.output = parameterSmoothers[8].getNextValue();
    p.mode = readMode();
    return p;
}

float CarbideAudioProcessor::renderVoiceSample() noexcept
{
    voice.setParams(readSmoothedParams());
    return voice.process();
}

void CarbideAudioProcessor::syncSmoothersFromParameters(const bool forceCurrent) noexcept
{
    for (size_t i = 0; i < parameterSmoothers.size(); ++i)
    {
        const float value = rawParameterValues[i]->load();
        if (forceCurrent)
            parameterSmoothers[i].setCurrentAndTargetValue(value);
        else
            parameterSmoothers[i].setTargetValue(value);
    }
}

int CarbideAudioProcessor::readMode() const noexcept
{
    if (modeChoiceParameter == nullptr)
        return 1;

    return std::clamp(modeChoiceParameter->getIndex(), 0, 2);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CarbideAudioProcessor();
}
