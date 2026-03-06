#include "../DSP/KickSynthVoice.h"
#include "../Presets/FactoryPresets.h"

#include <cmath>
#include <iostream>
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

int runKickRenderSmoke()
{
    KickSynthVoice voice;
    voice.prepare(48000.0);

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

float renderAttackPeakWith(const float decay, const float punch)
{
    KickSynthVoice voice;
    voice.prepare(48000.0);
    KickSynthVoice::Params params;
    params.decay = decay;
    params.punch = punch;
    params.output = 0.7f;
    voice.setParams(params);
    voice.trigger(36, 1.0f);

    float peak = 0.0f;
    for (int i = 0; i < 400; ++i)
        peak = std::max(peak, std::abs(voice.process()));
    return peak;
}

float energyWithDecay(const float decay)
{
    KickSynthVoice voice;
    voice.prepare(48000.0);
    KickSynthVoice::Params params;
    params.decay = decay;
    params.output = 0.7f;
    voice.setParams(params);
    voice.trigger(36, 1.0f);

    double energy = 0.0;
    for (int i = 0; i < 24000; ++i)
    {
        const float sample = voice.process();
        energy += static_cast<double>(sample * sample);
    }
    return static_cast<float>(energy);
}

float transientBrightnessForMode(const int mode)
{
    KickSynthVoice voice;
    voice.prepare(48000.0);
    KickSynthVoice::Params params;
    params.pitch = 0.5f;
    params.decay = 0.2f;
    params.punch = 0.85f;
    params.click = 0.75f;
    params.material = 0.45f;
    params.drive = 0.4f;
    params.tone = 0.55f;
    params.sub = 0.3f;
    params.output = 0.65f;
    params.mode = mode;
    voice.setParams(params);
    voice.trigger(36, 1.0f);

    float prev = 0.0f;
    double diffSum = 0.0;
    constexpr int window = 4000;
    for (int i = 0; i < window; ++i)
    {
        const float x = voice.process();
        diffSum += std::abs(x - prev);
        prev = x;
    }
    return static_cast<float>(diffSum / static_cast<double>(window));
}

std::vector<float> renderWindowForMode(const int mode)
{
    KickSynthVoice voice;
    voice.prepare(48000.0);
    KickSynthVoice::Params params;
    params.pitch = 0.5f;
    params.decay = 0.2f;
    params.punch = 0.85f;
    params.click = 0.75f;
    params.material = 0.45f;
    params.drive = 0.4f;
    params.tone = 0.55f;
    params.sub = 0.3f;
    params.output = 0.65f;
    params.mode = mode;
    voice.setParams(params);
    voice.trigger(36, 1.0f);

    std::vector<float> out;
    constexpr int window = 4096;
    out.reserve(window);
    for (int i = 0; i < window; ++i)
        out.push_back(voice.process());
    return out;
}

int runMonotonicityChecks()
{
    const float shortDecayEnergy = energyWithDecay(0.1f);
    const float longDecayEnergy = energyWithDecay(0.8f);
    if (longDecayEnergy <= shortDecayEnergy)
    {
        std::cerr << "FAIL: Decay monotonicity failed.\n";
        return 1;
    }

    const float lowPunchPeak = renderAttackPeakWith(0.25f, 0.15f);
    const float highPunchPeak = renderAttackPeakWith(0.25f, 0.9f);
    if (highPunchPeak <= lowPunchPeak)
    {
        std::cerr << "FAIL: Punch monotonicity failed.\n";
        return 1;
    }

    std::cout << "PASS: monotonicity checks\n";
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

int runModeSeparationCheck()
{
    const float softBrightness = transientBrightnessForMode(0);
    const float neutralBrightness = transientBrightnessForMode(1);
    const float hardBrightness = transientBrightnessForMode(2);

    if (! (softBrightness < neutralBrightness && neutralBrightness < hardBrightness))
    {
        std::cerr << "FAIL: Mode separation failed. soft=" << softBrightness
                  << " neutral=" << neutralBrightness << " hard=" << hardBrightness << "\n";
        return 1;
    }

    if ((hardBrightness - softBrightness) < 0.01f)
    {
        const auto soft = renderWindowForMode(0);
        const auto hard = renderWindowForMode(2);
        double mse = 0.0;
        for (size_t i = 0; i < soft.size(); ++i)
        {
            const double d = static_cast<double>(hard[i] - soft[i]);
            mse += d * d;
        }
        const float rmse = std::sqrt(static_cast<float>(mse / static_cast<double>(soft.size())));
        if (rmse < 0.02f)
        {
            std::cerr << "FAIL: Mode contrast too small. brightnessDelta=" << (hardBrightness - softBrightness)
                      << " rmse=" << rmse << "\n";
            return 1;
        }
    }

    std::cout << "PASS: mode separation\n";
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

    for (const auto& preset : presets)
    {
        KickSynthVoice voice;
        voice.prepare(48000.0);

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
        params.mode = std::clamp(preset.mode, 0, 2);

        voice.setParams(params);
        voice.trigger(36, 1.0f);

        float peak = 0.0f;
        double sumSquares = 0.0;
        for (int i = 0; i < 24000; ++i)
        {
            const float sample = voice.process();
            if (! std::isfinite(sample))
            {
                std::cerr << "FAIL: Non-finite sample in preset " << preset.name << "\n";
                return 1;
            }
            peak = std::max(peak, std::abs(sample));
            sumSquares += static_cast<double>(sample * sample);
        }

        const float rms = std::sqrt(static_cast<float>(sumSquares / 24000.0));
        if (peak > 0.98f)
        {
            std::cerr << "FAIL: Preset exceeds ceiling " << preset.name << " peak=" << peak << "\n";
            return 1;
        }

        if (peak < 0.08f || rms < 0.01f)
        {
            std::cerr << "FAIL: Preset too weak " << preset.name << " peak=" << peak << " rms=" << rms << "\n";
            return 1;
        }
    }

    std::cout << "PASS: factory preset sanity\n";
    return 0;
}
} // namespace

int main()
{
    int failures = 0;
    failures += runKickRenderSmoke();
    failures += runMonotonicityChecks();
    failures += runRetriggerCheck();
    failures += runModeSeparationCheck();
    failures += runFactoryPresetChecks();
    return failures == 0 ? 0 : 1;
}
