#include "KickSynthVoice.h"

#include <algorithm>

void KickSynthVoice::prepare(const double sampleRateHz) noexcept
{
    baseSampleRate = std::max(22050.0, sampleRateHz);
    oversampledRate = baseSampleRate * 2.0;
    phase = 0.0f;
    subPhase = 0.0f;
    ampEnv = 0.0f;
    pitchEnv = 0.0f;
    clickEnv = 0.0f;
    toneLow = 0.0f;
    toneHigh = 0.0f;
    dampingLowpass = 0.0f;
}

void KickSynthVoice::setParams(const Params& newParams) noexcept
{
    params = newParams;
}

void KickSynthVoice::trigger(const int midiNote, const float velocity) noexcept
{
    (void) midiNote;

    phase = 0.0f;
    subPhase = 0.0f;
    ampEnv = 1.0f;
    pitchEnv = 1.0f;
    clickEnv = 1.0f;
    toneLow = 0.0f;
    toneHigh = 0.0f;
    dampingLowpass = 0.0f;
    active = true;

    // Drum synth behavior: keep a fixed musical root and use the Pitch macro for tuning.
    constexpr float rootHz = 52.0f; // approx C1
    const float semitone = (params.pitch * 24.0f) - 12.0f;
    fundamentalHz = std::clamp(rootHz * std::pow(2.0f, semitone / 12.0f), 30.0f, 180.0f);
    velocityGain = std::clamp(0.2f + (velocity * velocity * 0.9f), 0.2f, 1.1f);
}

float KickSynthVoice::process() noexcept
{
    if (! active)
        return 0.0f;

    const float y0 = renderAtOversampleStep();
    const float y1 = renderAtOversampleStep();
    const float sample = 0.5f * (y0 + y1);

    if (ampEnv < 0.00015f)
        active = false;

    return std::isfinite(sample) ? sample : 0.0f;
}

bool KickSynthVoice::isActive() const noexcept
{
    return active;
}

float KickSynthVoice::renderAtOversampleStep() noexcept
{
    const int mode = std::clamp(params.mode, 0, 2);
    const float softAmt = mode == 0 ? 1.0f : 0.0f;
    const float hardAmt = mode == 2 ? 1.0f : 0.0f;

    const float decaySeconds = 0.035f + (params.decay * 0.42f);
    const float ampDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * decaySeconds));

    const float punchAmount = (24.0f + (params.punch * 68.0f)) * (1.0f - (softAmt * 0.18f) + (hardAmt * 0.24f));
    const float pitchEnvSeconds = 0.007f + ((1.0f - params.punch) * 0.035f);
    const float pitchDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * pitchEnvSeconds));

    const float clickSeconds = (0.0025f + ((1.0f - params.click) * 0.005f)) + (softAmt * 0.0015f) - (hardAmt * 0.0006f);
    const float clickDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * clickSeconds));

    const float pitchMultiplier = std::pow(2.0f, (pitchEnv * punchAmount) / 12.0f);
    const float bodyHz = std::clamp(fundamentalHz * pitchMultiplier, 25.0f, 2600.0f);
    const float bodyIncrement = static_cast<float>((kTwoPi * static_cast<double>(bodyHz)) / oversampledRate);
    phase += bodyIncrement;
    if (phase >= static_cast<float>(kTwoPi))
        phase -= static_cast<float>(kTwoPi);

    const float sine = std::sin(phase);
    const float harmonic = fastTanh(3.5f * sine + 0.5f * std::sin(phase * 2.0f));
    const float materialMix = std::clamp(params.material + (hardAmt * 0.18f) - (softAmt * 0.16f), 0.0f, 1.0f);
    float body = (sine * (1.0f - materialMix)) + (harmonic * materialMix);

    const float subIncrement = bodyIncrement * 0.5f;
    subPhase += subIncrement;
    if (subPhase >= static_cast<float>(kTwoPi))
        subPhase -= static_cast<float>(kTwoPi);
    body += std::sin(subPhase) * params.sub * 0.45f;

    const float clickShape = params.click * params.click;
    const float clickNoiseLevel = (0.06f + softAmt * 0.01f) + (clickShape * (0.90f + hardAmt * 0.26f));
    const float clickToneFreqMul = (1.7f + hardAmt * 1.6f) + (clickShape * (8.2f + hardAmt * 3.0f - softAmt * 1.4f));
    const float clickToneLevel = (0.05f + hardAmt * 0.10f) + (clickShape * (0.42f + hardAmt * 0.30f - softAmt * 0.18f));
    const float clickNoise = nextNoise() * clickNoiseLevel;
    const float clickTone = std::sin(phase * clickToneFreqMul) * clickToneLevel;
    const float clickMix = (0.24f + hardAmt * 0.10f) + (clickShape * (0.40f + hardAmt * 0.16f - softAmt * 0.12f));
    const float clickModeGain = 1.0f + (hardAmt * 0.95f) - (softAmt * 0.82f);
    const float clickSample = ((1.0f - clickMix) * clickNoise + clickMix * clickTone) * clickEnv * clickModeGain;

    const float punchAccent = 1.0f + (params.punch * pitchEnv * 0.45f);
    float sample = (body * ampEnv * punchAccent) + clickSample;
    sample *= velocityGain;

    const float drive = 1.0f + (params.drive * (7.0f + hardAmt * 4.0f - softAmt * 2.1f));
    sample = fastTanh(sample * drive);

    const float lowCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * 180.0f / static_cast<float>(oversampledRate));
    toneLow = lowCoeff * toneLow + (1.0f - lowCoeff) * sample;
    toneHigh = sample - toneLow;
    const float toneTilt = (params.tone * 2.0f) - 1.0f;
    float lowGain = 1.0f;
    float highGain = 1.0f;
    if (toneTilt < 0.0f)
    {
        lowGain = 1.0f + (-toneTilt * 0.9f);
        highGain = 1.0f + (toneTilt * 0.85f);
    }
    else
    {
        lowGain = 1.0f - (toneTilt * 0.35f);
        highGain = 1.0f + (toneTilt * 0.65f);
    }
    sample = toneLow * lowGain + toneHigh * highGain;

    // Damp high-frequency buildup from stacked nonlinearity so presets diverge more naturally.
    const float dampingCutoff = std::clamp((2400.0f - softAmt * 1100.0f + hardAmt * 800.0f)
                                               + (params.tone * (5000.0f + hardAmt * 1200.0f))
                                               + ((1.0f - params.drive) * (3500.0f - hardAmt * 600.0f))
                                               + ((1.0f - params.material) * 1500.0f),
                                           1600.0f, 12000.0f);
    const float dampingCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * dampingCutoff / static_cast<float>(oversampledRate));
    dampingLowpass = dampingCoeff * dampingLowpass + (1.0f - dampingCoeff) * sample;
    const float dampingMix = std::clamp((0.20f + softAmt * 0.24f - hardAmt * 0.12f)
                                            + (params.drive * (0.34f - hardAmt * 0.10f + softAmt * 0.06f))
                                            + (params.material * (0.22f - hardAmt * 0.07f)),
                                        0.0f, 0.90f);
    sample = sample * (1.0f - dampingMix) + dampingLowpass * dampingMix;

    const float brightEdge = sample - dampingLowpass;
    if (mode == 0)
        sample = dampingLowpass;
    else if (mode == 2)
        sample = sample + (brightEdge * 1.15f);

    sample *= (0.2f + (params.output * 1.3f));
    sample = softClip(sample * 1.35f);
    sample = std::clamp(sample, -0.97f, 0.97f);

    ampEnv *= ampDecayCoeff;
    pitchEnv *= pitchDecayCoeff;
    clickEnv *= clickDecayCoeff;

    if (! std::isfinite(ampEnv))
        ampEnv = 0.0f;
    if (! std::isfinite(pitchEnv))
        pitchEnv = 0.0f;
    if (! std::isfinite(clickEnv))
        clickEnv = 0.0f;

    return sample;
}

float KickSynthVoice::softClip(const float x) noexcept
{
    return x / (1.0f + std::abs(x));
}

float KickSynthVoice::fastTanh(const float x) noexcept
{
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

float KickSynthVoice::nextNoise() noexcept
{
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;
    constexpr float invMax = 1.0f / static_cast<float>(std::numeric_limits<uint32_t>::max());
    return (static_cast<float>(noiseState) * invMax * 2.0f) - 1.0f;
}
