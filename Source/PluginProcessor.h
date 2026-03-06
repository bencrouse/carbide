#pragma once

#include "DSP/KickSynthVoice.h"
#include "DSP/WaveformCapture.h"

#include <array>
#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

class CarbideAudioProcessor final : public juce::AudioProcessor
{
public:
    CarbideAudioProcessor();
    ~CarbideAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept;
    float getPeakMeter() const noexcept;
    float getRmsMeter() const noexcept;
    void getWaveformSnapshot(std::array<float, WaveformCapture::kSize>& target) const noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyPreset(int index);

private:
    KickSynthVoice::Params readSmoothedParams() noexcept;
    float renderVoiceSample() noexcept;
    void syncSmoothersFromParameters(bool forceCurrent) noexcept;
    int readMode() const noexcept;
    static constexpr int kParameterCount = 9;

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, kParameterCount> rawParameterValues {};
    juce::AudioParameterChoice* modeChoiceParameter = nullptr;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, kParameterCount> parameterSmoothers;
    KickSynthVoice voice;

    std::atomic<float> peakMeter { 0.0f };
    std::atomic<float> rmsMeter { 0.0f };
    WaveformCapture waveform;
    int currentProgram = 0;
    std::vector<juce::String> presetNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarbideAudioProcessor)
};
