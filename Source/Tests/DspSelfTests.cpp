#include "../DSP/KickSynthVoice.h"
#include "../Presets/FactoryPresets.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
[[nodiscard]] bool isFiniteBuffer(const std::vector<float>& buffer)
{
    for (const float sample : buffer)
    {
        if (! std::isfinite(sample))
            return false;
    }
    return true;
}

float midiNoteToHz(const float midiNote) noexcept
{
    return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}

KickSynthVoice::Params identityParams(const int mode)
{
    KickSynthVoice::Params params;
    params.pitch = 0.50f;
    params.decay = 0.25f;
    params.punch = 0.80f;
    params.click = 0.60f;
    params.material = 0.50f;
    params.drive = 0.45f;
    params.tone = 0.55f;
    params.sub = 0.45f;
    params.output = 0.70f;
    params.mode = mode;
    return params;
}

KickSynthVoice::Params paramsFromPreset(const FactoryPresets::Preset& preset)
{
    KickSynthVoice::Params params;
    params.pitch = preset.values[0];
    params.decay = preset.values[1];
    params.punch = preset.values[2];
    params.click = preset.values[3];
    params.material = preset.values[4];
    params.drive = preset.values[5];
    params.tone = preset.values[6];
    params.sub = preset.values[7];
    params.output = preset.values[8];
    params.mode = preset.mode;
    return params;
}

struct RenderFeatures
{
    float peak = 0.0f;
    float rms = 0.0f;
    float attackRms = 0.0f;
    float bodyRms = 0.0f;
    float brightness = 0.0f;
    float temporalCentroidSeconds = 0.0f;
};

std::vector<float> renderVoice(const KickSynthVoice::Params& params,
                               const int midiNote,
                               const float velocity,
                               const double sampleRate,
                               const float durationSeconds)
{
    KickSynthVoice voice;
    voice.prepare(sampleRate);
    voice.setParams(params);
    voice.trigger(midiNote, velocity);

    std::vector<float> rendered(static_cast<size_t>(sampleRate * durationSeconds));
    for (auto& sample : rendered)
        sample = voice.process();
    return rendered;
}

RenderFeatures measureFeatures(const std::vector<float>& rendered, const double sampleRate)
{
    RenderFeatures features;
    double sumSquares = 0.0;
    double differenceSquares = 0.0;
    double weightedTime = 0.0;
    double attackSquares = 0.0;
    double bodySquares = 0.0;
    size_t attackCount = 0;
    size_t bodyCount = 0;
    float previous = 0.0f;

    const auto attackEnd = static_cast<size_t>(sampleRate * 0.010);
    const auto bodyStart = static_cast<size_t>(sampleRate * 0.020);
    const auto bodyEnd = static_cast<size_t>(sampleRate * 0.120);
    for (size_t i = 0; i < rendered.size(); ++i)
    {
        const float sample = rendered[i];
        const double square = static_cast<double>(sample) * sample;
        features.peak = std::max(features.peak, std::abs(sample));
        sumSquares += square;
        weightedTime += square * (static_cast<double>(i) / sampleRate);

        const double difference = static_cast<double>(sample - previous);
        differenceSquares += difference * difference;
        previous = sample;

        if (i < attackEnd)
        {
            attackSquares += square;
            ++attackCount;
        }
        else if (i >= bodyStart && i < bodyEnd)
        {
            bodySquares += square;
            ++bodyCount;
        }
    }

    const double count = static_cast<double>(std::max<size_t>(1, rendered.size()));
    features.rms = static_cast<float>(std::sqrt(sumSquares / count));
    features.attackRms = static_cast<float>(std::sqrt(attackSquares / static_cast<double>(std::max<size_t>(1, attackCount))));
    features.bodyRms = static_cast<float>(std::sqrt(bodySquares / static_cast<double>(std::max<size_t>(1, bodyCount))));
    features.brightness = static_cast<float>(std::sqrt(differenceSquares / std::max(1.0, sumSquares)));
    features.temporalCentroidSeconds = static_cast<float>(weightedTime / std::max(1.0e-12, sumSquares));
    return features;
}

float normalizedCorrelation(const std::vector<float>& a, const std::vector<float>& b)
{
    const auto count = std::min(a.size(), b.size());
    double dot = 0.0;
    double energyA = 0.0;
    double energyB = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        dot += static_cast<double>(a[i]) * b[i];
        energyA += static_cast<double>(a[i]) * a[i];
        energyB += static_cast<double>(b[i]) * b[i];
    }

    return static_cast<float>(dot / std::sqrt(std::max(1.0e-18, energyA * energyB)));
}

float normalizedDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    const auto count = std::min(a.size(), b.size());
    double differenceEnergy = 0.0;
    double referenceEnergy = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        const double difference = static_cast<double>(a[i]) - b[i];
        differenceEnergy += difference * difference;
        referenceEnergy += static_cast<double>(a[i]) * a[i];
    }

    return static_cast<float>(std::sqrt(differenceEnergy / std::max(1.0e-18, referenceEnergy)));
}

double highFrequencyEnergy(const std::vector<float>& buffer)
{
    double energy = 0.0;
    float previous = 0.0f;
    for (const float sample : buffer)
    {
        const double difference = static_cast<double>(sample - previous);
        energy += difference * difference;
        previous = sample;
    }
    return energy;
}

double lowBandEnergy(const std::vector<float>& buffer, const double sampleRate)
{
    const float coefficient = std::exp(-2.0f * 3.14159265359f * 120.0f / static_cast<float>(sampleRate));
    float lowpass = 0.0f;
    double energy = 0.0;
    for (const float sample : buffer)
    {
        lowpass = coefficient * lowpass + (1.0f - coefficient) * sample;
        energy += static_cast<double>(lowpass) * lowpass;
    }
    return energy;
}

float estimateFrequency(const std::vector<float>& buffer,
                        const double sampleRate,
                        const float startSeconds,
                        const float endSeconds)
{
    const auto start = static_cast<size_t>(sampleRate * startSeconds);
    const auto end = std::min(buffer.size(), static_cast<size_t>(sampleRate * endSeconds));
    size_t firstCrossing = 0;
    size_t lastCrossing = 0;
    int crossingCount = 0;
    for (size_t i = std::max<size_t>(1, start); i < end; ++i)
    {
        if (buffer[i - 1] <= 0.0f && buffer[i] > 0.0f)
        {
            if (crossingCount == 0)
                firstCrossing = i;
            lastCrossing = i;
            ++crossingCount;
        }
    }

    if (crossingCount < 2 || lastCrossing == firstCrossing)
        return 0.0f;
    return static_cast<float>((crossingCount - 1) * sampleRate / static_cast<double>(lastCrossing - firstCrossing));
}

bool approximatelyEqual(const float actual, const float expected, const float relativeTolerance)
{
    return std::abs(actual - expected) <= std::max(1.0e-6f, std::abs(expected) * relativeTolerance);
}

int runNeutralRegressionChecks()
{
    constexpr std::array<KickSynthVoice::Params, 3> fixtures {
        KickSynthVoice::Params { 0.50f, 0.25f, 0.80f, 0.60f, 0.50f, 0.45f, 0.55f, 0.45f, 0.70f, 1 },
        KickSynthVoice::Params { 0.52f, 0.27f, 0.70f, 0.52f, 0.40f, 0.50f, 0.58f, 0.50f, 0.66f, 1 },
        KickSynthVoice::Params { 0.51f, 0.34f, 0.68f, 0.44f, 0.52f, 0.60f, 0.44f, 0.68f, 0.63f, 1 }
    };
    constexpr std::array<int, 3> notes { 36, 36, 48 };
    constexpr std::array<float, 3> velocities { 1.0f, 0.72f, 1.0f };
    constexpr std::array<RenderFeatures, 3> expected {
        RenderFeatures { 0.64678013f, 0.42625317f, 0.60098660f, 0.56827962f, 0.08898033f, 0.15942152f },
        RenderFeatures { 0.62185264f, 0.37290263f, 0.58308506f, 0.52396870f, 0.09059005f, 0.14356437f },
        RenderFeatures { 0.62696242f, 0.49289754f, 0.55012375f, 0.57546639f, 0.09504027f, 0.20265619f }
    };
    constexpr std::array<size_t, 12> checkpoints { 0, 1, 2, 3, 10, 25, 50, 100, 250, 500, 1000, 5000 };
    constexpr std::array<float, 12> expectedSamples {
        0.407220f, 0.662908f, 0.602404f, 0.639039f, -0.579568f, 0.614096f,
        -0.631068f, 0.606417f, 0.640268f, -0.599622f, 0.600846f, -0.579279f
    };

    for (size_t i = 0; i < fixtures.size(); ++i)
    {
        const auto rendered = renderVoice(fixtures[i], notes[i], velocities[i], 48000.0, 0.5f);
        const std::vector<float> aligned(rendered.begin() + KickSynthVoice::getLatencySamples(), rendered.end());
        const auto features = measureFeatures(aligned, 48000.0);
        if (i == 0)
        {
            for (size_t checkpoint = 0; checkpoint < checkpoints.size(); ++checkpoint)
            {
                if (std::abs(aligned[checkpoints[checkpoint]] - expectedSamples[checkpoint]) > 2.0e-5f)
                {
                    std::cerr << "FAIL: Neutral sample checkpoint " << checkpoint << " changed.\n";
                    return 1;
                }
            }
        }
        if (! approximatelyEqual(features.peak, expected[i].peak, 0.15f)
            || ! approximatelyEqual(features.rms, expected[i].rms, 0.005f)
            || ! approximatelyEqual(features.attackRms, expected[i].attackRms, 0.005f)
            || ! approximatelyEqual(features.bodyRms, expected[i].bodyRms, 0.005f)
            || ! approximatelyEqual(features.brightness, expected[i].brightness, 0.05f)
            || ! approximatelyEqual(features.temporalCentroidSeconds, expected[i].temporalCentroidSeconds, 0.01f))
        {
            std::cerr << "FAIL: Neutral regression fixture " << i << " changed: peak=" << features.peak
                      << " rms=" << features.rms << " attack=" << features.attackRms
                      << " body=" << features.bodyRms << " brightness=" << features.brightness
                      << " centroid=" << features.temporalCentroidSeconds << "\n";
            return 1;
        }
    }

    std::cout << "PASS: Neutral render regression\n";
    return 0;
}

int runMidiPitchTrackingChecks()
{
    KickSynthVoice voice;
    voice.prepare(48000.0);
    KickSynthVoice::Params params;
    params.pitch = 0.5f;
    voice.setParams(params);

    voice.trigger(36, 1.0f);
    const float note36Hz = voice.getFundamentalHz();
    voice.trigger(48, 1.0f);
    const float note48Hz = voice.getFundamentalHz();

    if (std::abs(note36Hz - midiNoteToHz(36.0f)) > 0.001f
        || std::abs(note48Hz - midiNoteToHz(48.0f)) > 0.001f
        || std::abs((note48Hz / note36Hz) - 2.0f) > 0.0001f)
    {
        std::cerr << "FAIL: MIDI note tracking mismatch. note36=" << note36Hz
                  << " note48=" << note48Hz << "\n";
        return 1;
    }

    params.pitch = 0.0f;
    voice.setParams(params);
    voice.trigger(36, 1.0f);
    const float downOctaveHz = voice.getFundamentalHz();

    params.pitch = 1.0f;
    voice.setParams(params);
    voice.trigger(36, 1.0f);
    const float upOctaveHz = voice.getFundamentalHz();

    if (std::abs(downOctaveHz - midiNoteToHz(24.0f)) > 0.001f
        || std::abs(upOctaveHz - midiNoteToHz(48.0f)) > 0.001f)
    {
        std::cerr << "FAIL: Pitch transpose mismatch. down=" << downOctaveHz
                  << " up=" << upOctaveHz << "\n";
        return 1;
    }

    params.pitch = 0.5f;
    voice.setParams(params);
    for (const int midiNote : { 0, 127 })
    {
        voice.trigger(midiNote, 1.0f);
        for (int i = 0; i < 4096; ++i)
        {
            const float sample = voice.process();
            if (! std::isfinite(sample) || std::abs(sample) > 0.98f)
            {
                std::cerr << "FAIL: Unsafe output at MIDI note " << midiNote << "\n";
                return 1;
            }
        }
    }

    std::cout << "PASS: MIDI pitch tracking\n";
    return 0;
}

int runRenderedPitchChecks()
{
    for (int mode = 0; mode < 3; ++mode)
    {
        auto params = identityParams(mode);
        params.decay = 1.0f;
        params.punch = 0.0f;
        params.click = 0.0f;
        params.material = 0.0f;
        params.drive = 0.0f;
        params.tone = 0.5f;
        params.sub = 0.0f;

        const auto note36 = renderVoice(params, 36, 1.0f, 48000.0, 0.8f);
        const auto note48 = renderVoice(params, 48, 1.0f, 48000.0, 0.8f);
        const float note36Hz = estimateFrequency(note36, 48000.0, 0.4f, 0.8f);
        const float note48Hz = estimateFrequency(note48, 48000.0, 0.4f, 0.8f);
        if (! approximatelyEqual(note36Hz, midiNoteToHz(36.0f), 0.04f)
            || ! approximatelyEqual(note48Hz, midiNoteToHz(48.0f), 0.04f)
            || ! approximatelyEqual(note48Hz / note36Hz, 2.0f, 0.03f))
        {
            std::cerr << "FAIL: Rendered pitch mismatch in mode " << mode
                      << ", note36=" << note36Hz << " note48=" << note48Hz << "\n";
            return 1;
        }
    }

    std::cout << "PASS: rendered pitch tracking in every mode\n";
    return 0;
}

int runKickRenderSmoke()
{
    KickSynthVoice voice;
    voice.prepare(48000.0);

    const auto params = identityParams(1);
    voice.setParams(params);
    voice.trigger(36, 1.0f);

    constexpr int count = 48000;
    std::vector<float> rendered;
    rendered.reserve(count);
    float peak = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const float sample = voice.process();
        rendered.push_back(sample);
        peak = std::max(peak, std::abs(sample));
    }

    if (! isFiniteBuffer(rendered))
    {
        std::cerr << "FAIL: Non-finite samples in render.\n";
        return 1;
    }

    if (peak > 0.98f)
    {
        std::cerr << "FAIL: Output ceiling exceeded peak=" << peak << "\n";
        return 1;
    }

    std::cout << "PASS: kick render smoke, peak=" << peak << "\n";
    return 0;
}

double bufferEnergy(const std::vector<float>& buffer)
{
    double energy = 0.0;
    for (const float sample : buffer)
        energy += static_cast<double>(sample * sample);
    return energy;
}

int runMacroResponseChecks()
{
    constexpr std::array<float KickSynthVoice::Params::*, 3> textureMacros {
        &KickSynthVoice::Params::material,
        &KickSynthVoice::Params::drive,
        &KickSynthVoice::Params::sub
    };
    constexpr std::array<const char*, 3> textureNames { "material", "drive", "sub" };

    for (int mode = 0; mode < 3; ++mode)
    {
        const auto base = identityParams(mode);
        auto low = base;
        auto high = base;

        low.decay = 0.1f;
        high.decay = 0.9f;
        const auto lowDecay = renderVoice(low, 36, 1.0f, 48000.0, 0.5f);
        const auto highDecay = renderVoice(high, 36, 1.0f, 48000.0, 0.5f);
        if (bufferEnergy(highDecay) <= bufferEnergy(lowDecay) * 1.2)
        {
            std::cerr << "FAIL: Decay response too small in mode " << mode << "\n";
            return 1;
        }

        low = base;
        high = base;
        low.click = 0.1f;
        high.click = 0.9f;
        const auto lowClick = renderVoice(low, 36, 1.0f, 48000.0, 0.008f);
        const auto highClick = renderVoice(high, 36, 1.0f, 48000.0, 0.008f);
        const float clickDifference = normalizedDifference(lowClick, highClick);
        const double lowClickHighEnergy = highFrequencyEnergy(lowClick);
        const double highClickHighEnergy = highFrequencyEnergy(highClick);
        if (clickDifference < 0.02f
            || (mode == 0 && highClickHighEnergy <= lowClickHighEnergy * 1.02))
        {
            std::cerr << "FAIL: Click response too small in mode " << mode << ", difference=" << clickDifference
                      << " lowHighEnergy=" << lowClickHighEnergy << " highHighEnergy=" << highClickHighEnergy << "\n";
            return 1;
        }

        low = base;
        high = base;
        low.punch = 0.1f;
        high.punch = 0.9f;
        const auto lowPunch = renderVoice(low, 36, 1.0f, 48000.0, 0.15f);
        const auto highPunch = renderVoice(high, 36, 1.0f, 48000.0, 0.15f);
        if (normalizedDifference(lowPunch, highPunch) < 0.15f)
        {
            std::cerr << "FAIL: Punch response too small in mode " << mode << "\n";
            return 1;
        }

        low = base;
        high = base;
        low.tone = 0.1f;
        high.tone = 0.9f;
        const auto lowTone = measureFeatures(renderVoice(low, 36, 1.0f, 48000.0, 0.25f), 48000.0);
        const auto highTone = measureFeatures(renderVoice(high, 36, 1.0f, 48000.0, 0.25f), 48000.0);
        if (highTone.brightness <= lowTone.brightness * 1.05f)
        {
            std::cerr << "FAIL: Tone response too small in mode " << mode << "\n";
            return 1;
        }

        for (size_t i = 0; i < textureMacros.size(); ++i)
        {
            low = base;
            high = base;
            low.*textureMacros[i] = 0.1f;
            high.*textureMacros[i] = 0.9f;
            const auto lowTexture = renderVoice(low, 36, 1.0f, 48000.0, 0.25f);
            const auto highTexture = renderVoice(high, 36, 1.0f, 48000.0, 0.25f);
            if (normalizedDifference(lowTexture, highTexture) < 0.03f)
            {
                std::cerr << "FAIL: " << textureNames[i] << " response too small in mode " << mode << "\n";
                return 1;
            }
        }

        low = base;
        high = base;
        low.sub = 0.1f;
        high.sub = 0.9f;
        const auto lowSub = renderVoice(low, 36, 1.0f, 48000.0, 0.25f);
        const auto highSub = renderVoice(high, 36, 1.0f, 48000.0, 0.25f);
        const double lowSubEnergy = lowBandEnergy(lowSub, 48000.0);
        const double highSubEnergy = lowBandEnergy(highSub, 48000.0);
        if (mode != 1 && highSubEnergy <= lowSubEnergy * 1.02)
        {
            std::cerr << "FAIL: Sub does not increase low-band energy in mode " << mode
                      << ", low=" << lowSubEnergy << " high=" << highSubEnergy << "\n";
            return 1;
        }

        low = base;
        high = base;
        low.output = 0.2f;
        high.output = 0.8f;
        const auto lowOutput = measureFeatures(renderVoice(low, 36, 1.0f, 48000.0, 0.25f), 48000.0);
        const auto highOutput = measureFeatures(renderVoice(high, 36, 1.0f, 48000.0, 0.25f), 48000.0);
        if (highOutput.rms <= lowOutput.rms * 1.15f)
        {
            std::cerr << "FAIL: Output response too small in mode " << mode << "\n";
            return 1;
        }
    }

    std::cout << "PASS: macro response checks\n";
    return 0;
}

int runRetriggerCheck()
{
    KickSynthVoice voice;
    voice.prepare(48000.0);
    KickSynthVoice::Params params;
    params.output = 0.7f;
    voice.setParams(params);

    float firstPeak = 0.0f;
    float secondPeak = 0.0f;

    voice.trigger(36, 1.0f);
    for (int i = 0; i < 24000; ++i)
    {
        const float sample = voice.process();
        firstPeak = std::max(firstPeak, std::abs(sample));
    }

    voice.trigger(36, 1.0f);
    for (int i = 0; i < 24000; ++i)
    {
        const float sample = voice.process();
        secondPeak = std::max(secondPeak, std::abs(sample));
    }

    if (std::abs(firstPeak - secondPeak) > 0.08f)
    {
        std::cerr << "FAIL: Retrigger mismatch.\n";
        return 1;
    }

    std::cout << "PASS: retrigger consistency\n";
    return 0;
}

int runRapidRetriggerCheck()
{
    for (int mode = 0; mode < 3; ++mode)
    {
        KickSynthVoice voice;
        voice.prepare(48000.0);
        auto params = identityParams(mode);
        voice.setParams(params);
        voice.trigger(36, 1.0f);

        float beforeRetrigger = 0.0f;
        for (int i = 0; i < 4000; ++i)
            beforeRetrigger = voice.process();

        params.mode = (mode + 1) % 3;
        voice.setParams(params);
        voice.trigger(36, 1.0f);
        const float afterRetrigger = voice.process();
        double initialEnergy = static_cast<double>(afterRetrigger) * afterRetrigger;
        for (int i = 1; i < KickSynthVoice::getLatencySamples(); ++i)
        {
            const float sample = voice.process();
            initialEnergy += static_cast<double>(sample) * sample;
        }

        if (std::abs(afterRetrigger - beforeRetrigger) > 0.20f || initialEnergy < 0.01)
        {
            std::cerr << "FAIL: Rapid retrigger interrupted the latency pipeline in mode " << mode << "\n";
            return 1;
        }
    }

    std::cout << "PASS: continuous rapid retrigger\n";
    return 0;
}

int runModeSeparationCheck()
{
    const auto soft = renderVoice(identityParams(0), 36, 1.0f, 48000.0, 0.5f);
    const auto neutral = renderVoice(identityParams(1), 36, 1.0f, 48000.0, 0.5f);
    const auto hard = renderVoice(identityParams(2), 36, 1.0f, 48000.0, 0.5f);
    const auto softFeatures = measureFeatures(soft, 48000.0);
    const auto neutralFeatures = measureFeatures(neutral, 48000.0);
    const auto hardFeatures = measureFeatures(hard, 48000.0);

    if (softFeatures.brightness >= neutralFeatures.brightness * 0.65f
        || hardFeatures.brightness <= neutralFeatures.brightness * 1.25f)
    {
        std::cerr << "FAIL: Mode brightness identities overlap. soft=" << softFeatures.brightness
                  << " neutral=" << neutralFeatures.brightness << " hard=" << hardFeatures.brightness << "\n";
        return 1;
    }

    const float softNeutralCorrelation = std::abs(normalizedCorrelation(soft, neutral));
    const float neutralHardCorrelation = std::abs(normalizedCorrelation(neutral, hard));
    const float softHardCorrelation = std::abs(normalizedCorrelation(soft, hard));
    if (softNeutralCorrelation >= 0.95f || neutralHardCorrelation >= 0.95f || softHardCorrelation >= 0.90f)
    {
        std::cerr << "FAIL: Mode waveforms too similar. soft-neutral=" << softNeutralCorrelation
                  << " neutral-hard=" << neutralHardCorrelation << " soft-hard=" << softHardCorrelation << "\n";
        return 1;
    }

    const float lowestRms = std::min({ softFeatures.rms, neutralFeatures.rms, hardFeatures.rms });
    const float highestRms = std::max({ softFeatures.rms, neutralFeatures.rms, hardFeatures.rms });
    if (highestRms > lowestRms * 1.55f)
    {
        std::cerr << "FAIL: Mode comparison is not reasonably level matched.\n";
        return 1;
    }

    std::cout << "PASS: mode identity separation\n";
    return 0;
}

int runModeLatchingCheck()
{
    KickSynthVoice switched;
    KickSynthVoice control;
    switched.prepare(48000.0);
    control.prepare(48000.0);

    auto params = identityParams(0);
    switched.setParams(params);
    control.setParams(params);
    switched.trigger(36, 1.0f);
    control.trigger(36, 1.0f);
    for (int i = 0; i < 512; ++i)
    {
        switched.process();
        control.process();
    }

    params.mode = 2;
    switched.setParams(params);
    params.mode = 0;
    control.setParams(params);
    for (int i = 0; i < 2048; ++i)
    {
        if (std::abs(switched.process() - control.process()) > 1.0e-7f)
        {
            std::cerr << "FAIL: Mode changed during an active hit.\n";
            return 1;
        }
    }

    params.mode = 2;
    switched.setParams(params);
    switched.trigger(36, 1.0f);
    params.mode = 0;
    control.setParams(params);
    control.trigger(36, 1.0f);
    std::vector<float> switchedRender(4096);
    std::vector<float> controlRender(4096);
    for (size_t i = 0; i < switchedRender.size(); ++i)
    {
        switchedRender[i] = switched.process();
        controlRender[i] = control.process();
    }
    if (std::abs(normalizedCorrelation(switchedRender, controlRender)) >= 0.90f)
    {
        std::cerr << "FAIL: Latched mode was not applied on the next trigger.\n";
        return 1;
    }

    std::cout << "PASS: trigger-latched mode\n";
    return 0;
}

int runSampleRateAndTailChecks()
{
    constexpr std::array<double, 3> sampleRates { 44100.0, 48000.0, 96000.0 };
    for (int mode = 0; mode < 3; ++mode)
    {
        const auto reference = measureFeatures(renderVoice(identityParams(mode), 36, 0.75f, 48000.0, 0.5f), 48000.0);
        for (const double sampleRate : sampleRates)
        {
            const auto rendered = renderVoice(identityParams(mode), 36, 0.75f, sampleRate, 0.5f);
            const auto features = measureFeatures(rendered, sampleRate);
            if (! isFiniteBuffer(rendered)
                || ! approximatelyEqual(features.peak, reference.peak, 0.20f)
                || ! approximatelyEqual(features.rms, reference.rms, 0.20f)
                || ! approximatelyEqual(features.temporalCentroidSeconds, reference.temporalCentroidSeconds, 0.20f))
            {
                std::cerr << "FAIL: Sample-rate consistency in mode " << mode << " at " << sampleRate << " Hz.\n";
                return 1;
            }
        }

        for (const double tailSampleRate : { 100.0, 11025.0, 48000.0 })
        {
            KickSynthVoice voice;
            voice.prepare(tailSampleRate);
            auto params = identityParams(mode);
            params.decay = 1.0f;
            voice.setParams(params);
            voice.trigger(36, 1.0f);
            int activeSamples = 0;
            const int maximumSamples = static_cast<int>(
                std::ceil(tailSampleRate * KickSynthVoice::getMaximumTailSeconds(tailSampleRate))) + 1;
            while (voice.isActive() && activeSamples < maximumSamples)
            {
                voice.process();
                ++activeSamples;
            }
            if (voice.isActive()
                || activeSamples > static_cast<int>(tailSampleRate * KickSynthVoice::getMaximumTailSeconds(tailSampleRate)))
            {
                std::cerr << "FAIL: Tail exceeds declared maximum in mode " << mode
                          << " at " << tailSampleRate << " Hz.\n";
                return 1;
            }
        }
    }

    std::cout << "PASS: sample-rate and tail checks\n";
    return 0;
}

int runInvalidParameterCheck()
{
    KickSynthVoice voice;
    voice.prepare(48000.0);
    KickSynthVoice::Params params;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    params.pitch = nan;
    params.decay = nan;
    params.punch = std::numeric_limits<float>::infinity();
    params.click = -10.0f;
    params.material = 10.0f;
    params.drive = nan;
    params.tone = nan;
    params.sub = nan;
    params.output = nan;
    params.mode = 99;
    voice.setParams(params);
    voice.trigger(36, 1.0f);

    const int maximumSamples = static_cast<int>(48000.0 * KickSynthVoice::getMaximumTailSeconds(48000.0));
    float peak = 0.0f;
    for (int i = 0; i < maximumSamples && voice.isActive(); ++i)
    {
        const float sample = voice.process();
        if (! std::isfinite(sample))
        {
            std::cerr << "FAIL: Invalid parameters produced non-finite output.\n";
            return 1;
        }
        peak = std::max(peak, std::abs(sample));
    }
    if (voice.isActive() || peak < 0.05f)
    {
        std::cerr << "FAIL: Invalid parameters were not sanitized into a valid finite render.\n";
        return 1;
    }

    voice.prepare(48000.0);
    voice.setParams(identityParams(2));
    voice.trigger(36, nan);
    for (int i = 0; i < 512; ++i)
    {
        if (! std::isfinite(voice.process()))
        {
            std::cerr << "FAIL: Invalid velocity contaminated the decimator.\n";
            return 1;
        }
    }
    voice.trigger(36, 1.0f);
    peak = 0.0f;
    for (int i = 0; i < 512; ++i)
        peak = std::max(peak, std::abs(voice.process()));
    if (peak < 0.05f)
    {
        std::cerr << "FAIL: Voice did not recover after invalid velocity.\n";
        return 1;
    }

    std::cout << "PASS: invalid parameter sanitization\n";
    return 0;
}

int runFactoryPresetChecks()
{
    const auto presets = FactoryPresets::getPresets();
    if (presets.size() < 25)
    {
        std::cerr << "FAIL: Expected at least 25 presets, got " << presets.size() << "\n";
        return 1;
    }

    constexpr std::array<const char*, 25> expectedNames {
        "Core / Needle Punch", "Core / Short Alloy", "Core / Dry Mono Rail", "Core / Arc Core", "Core / Pulse Stub",
        "Click / Glass Foot", "Click / Tin Snap", "Click / Laser Tap", "Click / Punch Film", "Click / Air Needle",
        "Weight / Needle Sub", "Weight / Future Brick", "Weight / Silk Hammer", "Weight / Hollow Carbon", "Weight / Bedrock",
        "Edge / Crushed Rubber", "Edge / Grain Impact", "Edge / Chrome Bite", "Edge / Iron Flick", "Edge / Hard Resin",
        "Soft / Soft Collapse", "Soft / Velvet Knock", "Soft / Pillow Tap", "Soft / Round Bloom", "Soft / Haze Pulse"
    };
    for (size_t i = 0; i < expectedNames.size(); ++i)
    {
        if (presets[i].name != expectedNames[i])
        {
            std::cerr << "FAIL: Factory preset name or program order changed at index " << i << "\n";
            return 1;
        }
    }

    std::array<double, 5> categoryBrightness {};
    std::array<double, 5> categoryAttackRatio {};
    std::array<int, 5> categoryCounts {};
    constexpr std::array<double, 3> sampleRates { 44100.0, 48000.0, 96000.0 };

    for (const auto& preset : presets)
    {
        const auto params = paramsFromPreset(preset);
        for (const double sampleRate : sampleRates)
        {
            for (const int midiNote : { 24, 36, 48 })
            {
                const auto rendered = renderVoice(params, midiNote, 1.0f, sampleRate, 0.5f);
                const auto features = measureFeatures(rendered, sampleRate);
                if (! isFiniteBuffer(rendered))
                {
                    std::cerr << "FAIL: Non-finite sample in preset " << preset.name
                              << " at MIDI note " << midiNote << " and " << sampleRate << " Hz.\n";
                    return 1;
                }

                if (features.peak > 0.98f || features.peak < 0.08f || features.rms < 0.01f)
                {
                    std::cerr << "FAIL: Unsafe preset level " << preset.name
                              << " at MIDI note " << midiNote << " and " << sampleRate
                              << " Hz, peak=" << features.peak << " rms=" << features.rms << "\n";
                    return 1;
                }

                const auto clipped = std::count_if(rendered.begin(), rendered.end(), [](const float sample) {
                    return std::abs(sample) >= 0.969f;
                });
                if (clipped > static_cast<ptrdiff_t>(rendered.size() / 50))
                {
                    std::cerr << "FAIL: Excessive ceiling occupancy in preset " << preset.name << "\n";
                    return 1;
                }
            }
        }

        const auto features = measureFeatures(renderVoice(params, 36, 1.0f, 48000.0, 0.5f), 48000.0);
        int category = -1;
        if (preset.name.startsWith("Core /"))
            category = 0;
        else if (preset.name.startsWith("Click /"))
            category = 1;
        else if (preset.name.startsWith("Weight /"))
            category = 2;
        else if (preset.name.startsWith("Edge /"))
            category = 3;
        else if (preset.name.startsWith("Soft /"))
            category = 4;

        if (category < 0)
        {
            std::cerr << "FAIL: Unknown preset category for " << preset.name << "\n";
            return 1;
        }
        categoryBrightness[static_cast<size_t>(category)] += features.brightness;
        categoryAttackRatio[static_cast<size_t>(category)] += features.attackRms / std::max(0.001f, features.bodyRms);
        ++categoryCounts[static_cast<size_t>(category)];
    }

    for (size_t i = 0; i < categoryCounts.size(); ++i)
    {
        if (categoryCounts[i] != 5)
        {
            std::cerr << "FAIL: Expected five presets in category " << i << "\n";
            return 1;
        }
        categoryBrightness[i] /= categoryCounts[i];
        categoryAttackRatio[i] /= categoryCounts[i];
    }

    if (categoryBrightness[1] <= categoryBrightness[0] * 1.5
        || categoryBrightness[2] >= categoryBrightness[0] * 0.75
        || categoryBrightness[3] <= categoryBrightness[0] * 1.35
        || categoryBrightness[4] >= categoryBrightness[0] * 0.35
        || categoryAttackRatio[1] <= categoryAttackRatio[0] * 1.15)
    {
        std::cerr << "FAIL: Factory preset category identities overlap.\n";
        return 1;
    }

    std::cout << "PASS: factory preset safety and category identity\n";
    return 0;
}
} // namespace

int main()
{
    int failures = 0;
    failures += runNeutralRegressionChecks();
    failures += runMidiPitchTrackingChecks();
    failures += runRenderedPitchChecks();
    failures += runKickRenderSmoke();
    failures += runMacroResponseChecks();
    failures += runRetriggerCheck();
    failures += runRapidRetriggerCheck();
    failures += runModeSeparationCheck();
    failures += runModeLatchingCheck();
    failures += runSampleRateAndTailChecks();
    failures += runInvalidParameterCheck();
    failures += runFactoryPresetChecks();
    return failures == 0 ? 0 : 1;
}
