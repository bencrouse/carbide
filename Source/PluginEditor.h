#pragma once

#include "PluginProcessor.h"

#include <array>
#include <juce_gui_extra/juce_gui_extra.h>

class CarbideAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit CarbideAudioProcessorEditor(CarbideAudioProcessor&);
    ~CarbideAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class WaveformComponent final : public juce::Component
    {
    public:
        void setData(const std::array<float, WaveformCapture::kSize>& newData);
        void paint(juce::Graphics&) override;

    private:
        std::array<float, WaveformCapture::kSize> data {};
    };

    class MeterComponent final : public juce::Component
    {
    public:
        void setLevels(float newPeak, float newRms);
        void paint(juce::Graphics&) override;

    private:
        float peak = 0.0f;
        float rms = 0.0f;
    };

    void timerCallback() override;
    void refreshPresetCombo();
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& paramId);

    CarbideAudioProcessor& audioProcessor;
    juce::ComboBox modeBox;
    juce::ComboBox presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    std::array<juce::Slider, 9> knobs;
    std::array<juce::Label, 9> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 9> attachments;
    static constexpr std::array<const char*, 9> kParamIds {
        "pitch", "decay", "punch", "click", "material", "drive", "tone", "sub", "output"
    };
    static constexpr std::array<const char*, 9> kParamLabels {
        "Pitch", "Decay", "Punch", "Click", "Material", "Drive", "Tone", "Sub", "Output"
    };

    WaveformComponent waveform;
    MeterComponent meter;
    std::array<float, WaveformCapture::kSize> waveformData {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarbideAudioProcessorEditor)
};
