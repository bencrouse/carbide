#include "PluginEditor.h"

#include <algorithm>

namespace
{
juce::Colour bgTop() { return juce::Colour::fromRGB(13, 14, 18); }
juce::Colour bgBottom() { return juce::Colour::fromRGB(6, 7, 9); }
juce::Colour panel() { return juce::Colour::fromRGB(19, 22, 27); }
juce::Colour accent() { return juce::Colour::fromRGB(90, 236, 200); }
juce::Colour text() { return juce::Colour::fromRGB(210, 214, 222); }
} // namespace

CarbideAudioProcessorEditor::CarbideAudioProcessorEditor(CarbideAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setOpaque(true);
    setSize(760, 420);

    modeBox.addItem("Soft", 1);
    modeBox.addItem("Neutral", 2);
    modeBox.addItem("Hard", 3);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getState(), "mode", modeBox);
    addAndMakeVisible(modeBox);

    presetBox.setTextWhenNothingSelected("Preset");
    presetBox.onChange = [this]() {
        const int index = presetBox.getSelectedItemIndex();
        if (index >= 0)
            audioProcessor.applyPreset(index);
    };
    addAndMakeVisible(presetBox);
    refreshPresetCombo();

    for (size_t i = 0; i < knobs.size(); ++i)
        setupKnob(knobs[i], labels[i], kParamLabels[i], kParamIds[i]);

    addAndMakeVisible(waveform);
    addAndMakeVisible(meter);
    startTimerHz(30);
}

void CarbideAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(bgTop(), 0.0f, 0.0f, bgBottom(), 0.0f, static_cast<float>(getHeight()), false);
    gradient.addColour(0.5, juce::Colour::fromRGB(10, 12, 15));
    g.setGradientFill(gradient);
    g.fillAll();

    const auto panelBounds = getLocalBounds().reduced(16).withTrimmedTop(50);
    g.setColour(panel().withAlpha(0.94f));
    g.fillRoundedRectangle(panelBounds.toFloat(), 12.0f);

    g.setColour(text());
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("carbide", 18, 12, 180, 28, juce::Justification::left);

    g.setColour(text().withAlpha(0.70f));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("Kick Synth v1", 20, 34, 140, 16, juce::Justification::left);
}

void CarbideAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto topBar = area.removeFromTop(40);
    presetBox.setBounds(topBar.removeFromRight(210).reduced(4));
    modeBox.setBounds(topBar.removeFromRight(130).reduced(4));

    auto panelBounds = area.withTrimmedTop(10);
    auto visualStrip = panelBounds.removeFromBottom(84).reduced(14, 10);
    meter.setBounds(visualStrip.removeFromRight(190).reduced(4));
    waveform.setBounds(visualStrip.reduced(4));

    auto knobArea = panelBounds.reduced(14, 12);
    const int columns = 5;
    const int rows = 2;
    const int knobW = knobArea.getWidth() / columns;
    const int knobH = knobArea.getHeight() / rows;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int col = static_cast<int>(i) % columns;
        const int row = static_cast<int>(i) / columns;
        auto cell = juce::Rectangle<int>(knobArea.getX() + col * knobW, knobArea.getY() + row * knobH, knobW, knobH).reduced(8);
        knobs[i].setBounds(cell.removeFromTop(cell.getHeight() - 22));
        labels[i].setBounds(cell);
    }
}

void CarbideAudioProcessorEditor::timerCallback()
{
    audioProcessor.getWaveformSnapshot(waveformData);
    waveform.setData(waveformData);
    meter.setLevels(audioProcessor.getPeakMeter(), audioProcessor.getRmsMeter());
}

void CarbideAudioProcessorEditor::refreshPresetCombo()
{
    presetBox.clear(juce::dontSendNotification);
    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i)
        presetBox.addItem(audioProcessor.getProgramName(i), i + 1);
    presetBox.setSelectedItemIndex(audioProcessor.getCurrentProgram(), juce::dontSendNotification);
}

void CarbideAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& textLabel, const juce::String& paramId)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(36, 41, 49));
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent());
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(190, 254, 240));
    addAndMakeVisible(slider);

    label.setText(textLabel, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, text().withAlpha(0.88f));
    addAndMakeVisible(label);

    const auto index = static_cast<size_t>(&slider - knobs.data());
    attachments[index] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getState(), paramId, slider);
}

void CarbideAudioProcessorEditor::WaveformComponent::setData(const std::array<float, WaveformCapture::kSize>& newData)
{
    data = newData;
    repaint();
}

void CarbideAudioProcessorEditor::WaveformComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(3.0f);
    g.setColour(juce::Colour::fromRGB(24, 28, 36));
    g.fillRoundedRectangle(bounds, 6.0f);

    juce::Path path;
    const float midY = bounds.getCentreY();
    const float amp = bounds.getHeight() * 0.38f;
    for (size_t i = 0; i < data.size(); ++i)
    {
        const float x = bounds.getX() + (static_cast<float>(i) / static_cast<float>(data.size() - 1)) * bounds.getWidth();
        const float y = midY - data[i] * amp;
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.setColour(juce::Colour::fromRGB(90, 236, 200));
    g.strokePath(path, juce::PathStrokeType(1.8f));
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawHorizontalLine(static_cast<int>(midY), bounds.getX(), bounds.getRight());
}

void CarbideAudioProcessorEditor::MeterComponent::setLevels(const float newPeak, const float newRms)
{
    peak = std::clamp(newPeak, 0.0f, 1.0f);
    rms = std::clamp(newRms, 0.0f, 1.0f);
    repaint();
}

void CarbideAudioProcessorEditor::MeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(3.0f);
    g.setColour(juce::Colour::fromRGB(24, 28, 36));
    g.fillRoundedRectangle(bounds, 6.0f);

    auto bar = bounds.reduced(12.0f, 18.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRect(bar);

    const float rmsW = bar.getWidth() * rms;
    const float peakW = bar.getWidth() * peak;
    g.setColour(juce::Colour::fromRGB(66, 136, 122));
    g.fillRect(bar.withWidth(rmsW));
    g.setColour(juce::Colour::fromRGB(114, 255, 212));
    g.fillRect(bar.removeFromLeft(peakW));

    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Output", bounds.removeFromTop(14), juce::Justification::centredTop, false);
}
