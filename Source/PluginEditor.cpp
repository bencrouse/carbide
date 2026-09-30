#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::Colour silver() { return juce::Colour::fromRGB(225, 227, 218); }
juce::Colour ink() { return juce::Colour::fromRGB(17, 20, 18); }
juce::Colour signal() { return juce::Colour::fromRGB(185, 255, 28); }
juce::Colour muted() { return juce::Colour::fromRGB(124, 132, 122); }

juce::Font utilityFont(float size = 11.0f)
{
    return juce::Font(juce::FontOptions("Menlo", size, juce::Font::plain));
}

void utility(juce::Graphics& g, const juce::String& copy, juce::Rectangle<int> bounds,
             juce::Colour colour = ink(), float size = 11.0f,
             juce::Justification alignment = juce::Justification::left)
{
    g.setColour(colour);
    g.setFont(utilityFont(size));
    g.drawText(copy, bounds, alignment, false);
}

// The oscillator body before the sub, transient and output stages. The same
// waveshaping equations are used in KickSynthVoice for each character engine.
float bodyShape(const int mode, const float material, const float phase)
{
    const auto sine = std::sin(phase);
    const auto shape = material * material;
    if (mode == 0)
    {
        const auto second = std::sin(phase * 2.0f + 0.35f) * shape * 0.20f;
        const auto third = std::sin(phase * 3.0f) * shape * material * 0.055f;
        return (sine + second + third) / (1.0f + shape * 0.16f);
    }
    if (mode == 2)
    {
        const auto distorted = std::sin(phase + sine * (0.18f + shape * 1.10f));
        const auto second = std::sin(phase * 2.0f) * (0.10f + shape * 0.34f);
        const auto third = std::sin(phase * 3.0f + 0.4f) * shape * 0.22f;
        return (distorted + second + third) / (1.10f + shape * 0.42f);
    }

    const auto harmonicInput = 3.5f * sine + 0.5f * std::sin(phase * 2.0f);
    const auto squared = harmonicInput * harmonicInput;
    const auto harmonic = harmonicInput * (27.0f + squared) / (27.0f + 9.0f * squared);
    return sine * (1.0f - material) + harmonic * material;
}
} // namespace

void CarbideAudioProcessorEditor::InstrumentLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float position,
    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto cx = static_cast<float>(x + width / 2);
    const auto cy = static_cast<float>(y + height / 2);
    const auto radius = static_cast<float>(std::min(width, height)) * 0.36f;
    const auto angle = startAngle + position * (endAngle - startAngle);

    juce::Path ring;
    ring.addCentredArc(cx, cy, radius - 3.0f, radius - 3.0f, 0.0f,
                       startAngle, endAngle, true);
    g.setColour(silver().withAlpha(0.28f));
    g.strokePath(ring, juce::PathStrokeType(3.0f));

    juce::Path progress;
    progress.addCentredArc(cx, cy, radius - 3.0f, radius - 3.0f, 0.0f,
                           startAngle, angle, true);
    g.setColour(silver());
    g.strokePath(progress, juce::PathStrokeType(3.0f));

    const auto pointerStart = radius * 0.57f;
    g.setColour(silver());
    g.drawLine(cx + std::sin(angle) * pointerStart, cy - std::cos(angle) * pointerStart,
                cx + std::sin(angle) * (radius - 12.0f),
                cy - std::cos(angle) * (radius - 12.0f), 2.0f);

    const auto value = slider.getName() == "Pitch"
        ? juce::String(juce::roundToInt((slider.getValue() - 0.5) * 24.0)) + " st"
        : juce::String(juce::roundToInt(slider.getValue() * 100.0)) + "%";
    g.setColour(silver().withAlpha(0.82f));
    g.setFont(utilityFont(12.0f));
    g.drawText(value, juce::Rectangle<float>(cx - radius * 0.65f, cy - 13.0f,
                                             radius * 1.3f, 26.0f),
               juce::Justification::centred, false);

    if (slider.hasKeyboardFocus(true))
    {
        g.setColour(signal());
        g.drawRect(juce::Rectangle<int>(x, y, width, height).reduced(2), 1);
    }
}

void CarbideAudioProcessorEditor::InstrumentLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    if (slider.getName() == "Material")
    {
        auto plot = juce::Rectangle<float>(static_cast<float>(x + 18), static_cast<float>(y + 19),
                                            static_cast<float>(width - 36), 121.0f);
        g.setColour(ink().withAlpha(0.18f));
        g.drawHorizontalLine(static_cast<int>(plot.getCentreY()), plot.getX(), plot.getRight());

        juce::Path wave;
        constexpr int samples = 160;
        const auto material = static_cast<float>(slider.getValue());
        for (int i = 0; i < samples; ++i)
        {
            const auto t = static_cast<float>(i) / static_cast<float>(samples - 1);
            const auto phase = t * juce::MathConstants<float>::twoPi * 2.0f;
            const auto sample = bodyShape(materialMode, material, phase);
            const auto px = plot.getX() + plot.getWidth() * t;
            const auto py = plot.getCentreY() - sample * plot.getHeight() * 0.36f;
            if (i == 0)
                wave.startNewSubPath(px, py);
            else
                wave.lineTo(px, py);
        }
        g.setColour(ink());
        g.strokePath(wave, juce::PathStrokeType(2.5f));

        const auto left = static_cast<float>(x + 18);
        const auto right = static_cast<float>(x + width - 18);
        const auto cy = static_cast<float>(y + height - 28);
        const auto position = std::clamp(sliderPos, left, right);
        g.setColour(ink().withAlpha(0.25f));
        g.drawLine(left, cy, right, cy, 3.0f);
        g.setColour(ink());
        g.drawLine(left, cy, position, cy, 4.0f);
        g.setColour(signal());
        g.fillRect(juce::Rectangle<float>(position - 5.0f, cy - 12.0f, 10.0f, 24.0f));

        if (slider.hasKeyboardFocus(true))
        {
            g.setColour(ink());
            g.drawRect(juce::Rectangle<int>(x, y, width, height).reduced(2), 1);
        }
        return;
    }

    const auto left = static_cast<float>(x + 12);
    const auto right = static_cast<float>(x + width - 12);
    const auto mid = (left + right) * 0.5f;
    const auto cy = static_cast<float>(y + height / 2);
    const auto position = std::clamp(sliderPos, left, right);

    g.setColour(silver().withAlpha(0.3f));
    g.drawLine(left, cy, right, cy, 3.0f);
    g.setColour(signal());
    g.drawLine(slider.getName() == "Tone" ? mid : left, cy, position, cy, 3.0f);
    g.fillRect(juce::Rectangle<float>(position - 4.0f, cy - 13.0f, 8.0f, 26.0f));

    if (slider.getName() == "Tone")
    {
        g.setColour(silver().withAlpha(0.5f));
        g.drawVerticalLine(static_cast<int>(mid), cy - 11.0f, cy + 11.0f);
    }

    if (slider.hasKeyboardFocus(true))
    {
        g.setColour(signal());
        g.drawRect(juce::Rectangle<int>(x, y, width, height).reduced(2), 1);
    }
}

void CarbideAudioProcessorEditor::InstrumentLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour&, bool hover, bool down)
{
    auto bounds = button.getLocalBounds().toFloat();
    const bool selected = button.getToggleState();
    if (selected)
    {
        g.setColour(ink());
        g.fillRect(bounds);
        g.setColour(signal());
        g.fillRect(bounds.removeFromLeft(5.0f));
    }
    else if (hover || down)
    {
        g.setColour(ink().withAlpha(0.09f));
        g.fillRect(bounds);
    }

    if (button.hasKeyboardFocus(true))
    {
        g.setColour(selected ? signal() : ink());
        g.drawRect(button.getLocalBounds().reduced(2), 1);
    }
}

void CarbideAudioProcessorEditor::InstrumentLookAndFeel::drawButtonText(
    juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setColour(button.getToggleState() ? silver() : ink());
    g.setFont(juce::Font(juce::FontOptions("Helvetica Neue",
                                            button.getToggleState() ? 18.0f : 16.0f,
                                            juce::Font::bold)));
    g.drawText(button.getButtonText().toUpperCase(), button.getLocalBounds().reduced(13, 0),
               juce::Justification::centred, false);
}

void CarbideAudioProcessorEditor::InstrumentLookAndFeel::drawComboBox(
    juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    g.fillAll(ink());
    g.setColour(silver().withAlpha(0.65f));
    g.drawRect(0, 0, width, height);
    g.setColour(signal());
    juce::Path arrow;
    arrow.startNewSubPath(static_cast<float>(width - 24), static_cast<float>(height / 2 - 3));
    arrow.lineTo(static_cast<float>(width - 13), static_cast<float>(height / 2 - 3));
    arrow.lineTo(static_cast<float>(width) - 18.5f, static_cast<float>(height / 2 + 3));
    arrow.closeSubPath();
    g.fillPath(arrow);
}

juce::Font CarbideAudioProcessorEditor::InstrumentLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return utilityFont(12.0f);
}

CarbideAudioProcessorEditor::CarbideAudioProcessorEditor(CarbideAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setOpaque(true);
    setLookAndFeel(&instrumentLookAndFeel);
    setSize(900, 570);

    modeBox.addItem("Soft", 1);
    modeBox.addItem("Neutral", 2);
    modeBox.addItem("Hard", 3);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getState(), "mode", modeBox);

    const char* modeNames[] = { "Soft", "Neutral", "Hard" };
    for (int i = 0; i < 3; ++i)
    {
        auto& button = modeButtons[static_cast<size_t>(i)];
        button.setButtonText(modeNames[i]);
        button.setClickingTogglesState(false);
        button.onClick = [this, i] { modeBox.setSelectedId(i + 1, juce::sendNotificationSync); };
        addAndMakeVisible(button);
    }
    modeBox.onChange = [this] { refreshModeButtons(); };
    refreshModeButtons();

    presetBox.setTextWhenNothingSelected("SELECT CUT");
    presetBox.setColour(juce::ComboBox::textColourId, silver());
    presetBox.setColour(juce::ComboBox::arrowColourId, signal());
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

CarbideAudioProcessorEditor::~CarbideAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void CarbideAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(ink());
    g.setColour(silver());
    g.fillRect(12, 88, 876, 352);

    // One uninterrupted faceplate, with the control banks cut out of it.
    g.setColour(ink());
    g.fillRect(12, 170, 260, 270);
    g.fillRect(626, 170, 262, 270);
    g.drawLine(278.0f, 170.0f, 278.0f, 440.0f, 1.0f);
    g.drawLine(620.0f, 170.0f, 620.0f, 440.0f, 1.0f);

    g.setColour(signal());
    g.fillRect(12, 12, 9, 70);
    g.setColour(silver());
    g.setFont(juce::Font(juce::FontOptions("Helvetica Neue", 32.0f, juce::Font::bold)));
    g.drawText("carbide", 32, 19, 205, 44, juce::Justification::left, false);
    utility(g, "PRESETS", { 618, 13, 246, 19 }, muted(), 10.0f);

    g.setColour(ink());
    g.setFont(juce::Font(juce::FontOptions("Helvetica Neue", 17.0f, juce::Font::bold)));
    g.drawText("ENGINE", 29, 108, 226, 36, juce::Justification::left, false);

    utility(g, "BODY", { 29, 180, 228, 17 }, muted(), 10.0f);
    utility(g, "EDGE", { 642, 180, 220, 17 }, muted(), 10.0f);
    utility(g, "PURE", { 310, 392, 98, 17 }, ink().withAlpha(0.72f), 10.0f);
    utility(g, "HARMONIC", { 487, 392, 106, 17 }, ink().withAlpha(0.72f), 10.0f,
            juce::Justification::right);

    g.setColour(silver().withAlpha(0.17f));
    g.drawLine(30.0f, 320.0f, 254.0f, 320.0f);
    g.drawLine(644.0f, 320.0f, 869.0f, 320.0f);
    utility(g, "ROUND", { 43, 402, 80, 18 }, muted(), 10.0f);
    utility(g, "SNAP", { 180, 402, 72, 18 }, muted(), 10.0f, juce::Justification::right);
    utility(g, "WEIGHT", { 655, 402, 87, 18 }, muted(), 10.0f);
    utility(g, "EDGE", { 794, 402, 72, 18 }, muted(), 10.0f, juce::Justification::right);
}

void CarbideAudioProcessorEditor::resized()
{
    presetBox.setBounds(618, 39, 248, 32);
    for (int i = 0; i < 3; ++i)
        modeButtons[static_cast<size_t>(i)].setBounds(285 + i * 195, 104, 186, 44);

    const std::array<juce::Rectangle<int>, 9> knobBounds {
        juce::Rectangle<int>(28, 213, 73, 78), juce::Rectangle<int>(107, 213, 73, 78),
        juce::Rectangle<int>(31, 352, 224, 43), juce::Rectangle<int>(638, 213, 73, 78),
        juce::Rectangle<int>(300, 202, 298, 190),
        juce::Rectangle<int>(718, 213, 73, 78), juce::Rectangle<int>(643, 352, 226, 43),
        juce::Rectangle<int>(186, 213, 73, 78), juce::Rectangle<int>(798, 213, 73, 78)
    };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        knobs[i].setBounds(knobBounds[i]);
        const auto& bounds = knobBounds[i];
        if (i == 2 || i == 6)
            labels[i].setBounds(bounds.getX() + 9, 326, 97, 21);
        else
            labels[i].setBounds(bounds.getX() - 4, i == 4 ? 411 : 292,
                                bounds.getWidth() + 8, 21);
    }

    waveform.setBounds(12, 446, 654, 98);
    meter.setBounds(672, 446, 216, 98);
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

void CarbideAudioProcessorEditor::refreshModeButtons()
{
    for (size_t i = 0; i < modeButtons.size(); ++i)
        modeButtons[i].setToggleState(modeBox.getSelectedId() == static_cast<int>(i) + 1,
                                      juce::dontSendNotification);
    instrumentLookAndFeel.setMaterialMode(juce::jlimit(0, 2, modeBox.getSelectedId() - 1));
    knobs[4].repaint();
}

void CarbideAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label,
                                            const juce::String& textLabel, const juce::String& paramId)
{
    slider.setName(textLabel);
    const bool gesture = textLabel == "Punch" || textLabel == "Tone";
    const bool material = textLabel == "Material";
    slider.setSliderStyle(gesture || material ? juce::Slider::LinearHorizontal
                                  : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    if (! gesture && ! material)
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible(slider);

    label.setText(textLabel.toUpperCase(), juce::dontSendNotification);
    label.setFont(utilityFont(material ? 14.0f : 11.0f));
    label.setJustificationType(gesture ? juce::Justification::centredLeft
                                       : juce::Justification::centred);
    label.setColour(juce::Label::textColourId,
                    material ? ink() :
                    gesture ? signal() : silver());
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
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(ink());
    g.setColour(silver().withAlpha(0.32f));
    g.drawRect(bounds, 1.0f);

    auto plot = bounds.reduced(20.0f, 15.0f);
    plot.removeFromTop(13.0f);
    for (int i = 0; i <= 4; ++i)
    {
        const auto x = plot.getX() + plot.getWidth() * static_cast<float>(i) / 4.0f;
        g.setColour(silver().withAlpha(i == 0 ? 0.3f : 0.11f));
        g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
    }
    for (int i = 0; i <= 2; ++i)
    {
        const auto y = plot.getY() + plot.getHeight() * static_cast<float>(i) / 2.0f;
        g.setColour(silver().withAlpha(i == 1 ? 0.34f : 0.11f));
        g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
    }

    juce::Path path;
    for (size_t i = 0; i < data.size(); ++i)
    {
        const auto x = plot.getX() + static_cast<float>(i) / static_cast<float>(data.size() - 1) * plot.getWidth();
        const auto y = plot.getCentreY() - data[i] * plot.getHeight() * 0.45f;
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }
    g.setColour(signal());
    g.strokePath(path, juce::PathStrokeType(2.0f));
    utility(g, "WAVEFORM", { 21, 5, 220, 15 }, muted(), 9.0f);
}

void CarbideAudioProcessorEditor::MeterComponent::setLevels(float newPeak, float newRms)
{
    peak = std::clamp(newPeak, 0.0f, 1.0f);
    rms = std::clamp(newRms, 0.0f, 1.0f);
    repaint();
}

void CarbideAudioProcessorEditor::MeterComponent::paint(juce::Graphics& g)
{
    g.fillAll(ink());
    g.setColour(silver().withAlpha(0.32f));
    g.drawRect(getLocalBounds(), 1);
    utility(g, "LEVEL / PEAK", { 15, 10, 186, 18 }, muted(), 10.0f);

    const auto bar = juce::Rectangle<float>(15.0f, 46.0f, 186.0f, 15.0f);
    g.setColour(silver().withAlpha(0.15f));
    g.fillRect(bar);
    g.setColour(silver().withAlpha(0.48f));
    g.fillRect(bar.withWidth(bar.getWidth() * rms));
    g.setColour(signal());
    g.fillRect(bar.withWidth(bar.getWidth() * peak));

}
