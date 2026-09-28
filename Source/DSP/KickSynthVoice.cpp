#include "KickSynthVoice.h"

#include <algorithm>

void KickSynthVoice::prepare(const double sampleRateHz) noexcept
{
    oversampledRate = std::max(1.0, sampleRateHz) * 2.0;
    phase = 0.0f;
    subPhase = 0.0f;
    clickPhase = 0.0f;
    ampEnv = 0.0f;
    pitchEnv = 0.0f;
    clickEnv = 0.0f;
    toneLow = 0.0f;
    dampingLowpass = 0.0f;
    transientLowpass = 0.0f;
    downsampleHistory.fill(0.0f);
    downsampleWriteIndex = 0;
    filterDrainSamplesRemaining = 0;
    active = false;
}

void KickSynthVoice::setParams(const Params& newParams) noexcept
{
    const auto normalized = [](const float value, const float fallback) {
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    };

    params.pitch = normalized(newParams.pitch, 0.5f);
    params.decay = normalized(newParams.decay, 0.5f);
    params.punch = normalized(newParams.punch, 0.5f);
    params.click = normalized(newParams.click, 0.3f);
    params.material = normalized(newParams.material, 0.0f);
    params.drive = normalized(newParams.drive, 0.2f);
    params.tone = normalized(newParams.tone, 0.5f);
    params.sub = normalized(newParams.sub, 0.5f);
    params.output = normalized(newParams.output, 0.8f);
    params.mode = std::clamp(newParams.mode, 0, 2);
}

void KickSynthVoice::trigger(const int midiNote, const float velocity) noexcept
{
    phase = 0.0f;
    subPhase = 0.0f;
    clickPhase = 0.0f;
    ampEnv = 1.0f;
    pitchEnv = 1.0f;
    clickEnv = 1.0f;
    toneLow = 0.0f;
    dampingLowpass = 0.0f;
    transientLowpass = 0.0f;
    filterDrainSamplesRemaining = 0;
    latchedMode = static_cast<Mode>(params.mode);
    active = true;

    constexpr float a4Hz = 440.0f;
    const float transposeSemitones = (params.pitch * 24.0f) - 12.0f;
    const float noteSemitonesFromA4 = static_cast<float>(std::clamp(midiNote, 0, 127) - 69);
    fundamentalHz = a4Hz * std::pow(2.0f, (noteSemitonesFromA4 + transposeSemitones) / 12.0f);
    const float safeVelocity = std::isfinite(velocity) ? std::clamp(velocity, 0.0f, 1.0f) : 1.0f;
    velocityGain = 0.2f + (safeVelocity * safeVelocity * 0.9f);
}

float KickSynthVoice::process() noexcept
{
    if (! active)
        return 0.0f;

    if (filterDrainSamplesRemaining > 0)
    {
        const float sample = pushDownsampleFilter(0.0f);
        pushDownsampleFilter(0.0f);
        --filterDrainSamplesRemaining;
        if (filterDrainSamplesRemaining == 0)
            active = false;
        if (! std::isfinite(sample))
        {
            active = false;
            downsampleHistory.fill(0.0f);
            downsampleWriteIndex = 0;
            return 0.0f;
        }
        return std::clamp(sample, -0.97f, 0.97f);
    }

    const float y0 = renderAtOversampleStep();
    const float y1 = renderAtOversampleStep();
    const float sample = pushDownsampleFilter(y0);
    pushDownsampleFilter(y1);

    if (ampEnv < 0.00015f)
        filterDrainSamplesRemaining = kFilteredDrainSamples;

    if (! std::isfinite(sample))
    {
        active = false;
        filterDrainSamplesRemaining = 0;
        downsampleHistory.fill(0.0f);
        downsampleWriteIndex = 0;
        return 0.0f;
    }
    return std::clamp(sample, -0.97f, 0.97f);
}

bool KickSynthVoice::isActive() const noexcept
{
    return active;
}

float KickSynthVoice::getFundamentalHz() const noexcept
{
    return fundamentalHz;
}

float KickSynthVoice::renderAtOversampleStep() noexcept
{
    if (latchedMode == Mode::soft)
        return renderSoftStep();
    if (latchedMode == Mode::hard)
        return renderHardStep();
    return renderNeutralStep();
}

float KickSynthVoice::renderNeutralStep() noexcept
{
    const float decaySeconds = 0.035f + (params.decay * 0.42f);
    const float ampDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * decaySeconds));

    const float punchAmount = 24.0f + (params.punch * 68.0f);
    const float pitchEnvSeconds = 0.007f + ((1.0f - params.punch) * 0.035f);
    const float pitchDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * pitchEnvSeconds));

    const float clickSeconds = 0.0025f + ((1.0f - params.click) * 0.005f);
    const float clickDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * clickSeconds));

    const float pitchMultiplier = std::pow(2.0f, (pitchEnv * punchAmount) / 12.0f);
    const float bodyHz = std::clamp(fundamentalHz * pitchMultiplier, 25.0f, 2600.0f);
    const float bodyIncrement = static_cast<float>((kTwoPi * static_cast<double>(bodyHz)) / oversampledRate);
    phase += bodyIncrement;
    if (phase >= static_cast<float>(kTwoPi))
        phase -= static_cast<float>(kTwoPi);

    const float sine = std::sin(phase);
    const float harmonic = fastTanh(3.5f * sine + 0.5f * std::sin(phase * 2.0f));
    float body = (sine * (1.0f - params.material)) + (harmonic * params.material);

    const float subIncrement = bodyIncrement * 0.5f;
    subPhase += subIncrement;
    if (subPhase >= static_cast<float>(kTwoPi))
        subPhase -= static_cast<float>(kTwoPi);
    body += std::sin(subPhase) * params.sub * 0.45f;

    const float clickShape = params.click * params.click;
    const float clickNoiseLevel = 0.06f + (clickShape * 0.90f);
    const float clickToneFreqMul = 1.7f + (clickShape * 8.2f);
    const float clickToneLevel = 0.05f + (clickShape * 0.42f);
    const float clickNoise = nextNoise() * clickNoiseLevel;
    const float clickTone = std::sin(phase * clickToneFreqMul) * clickToneLevel;
    const float clickMix = 0.24f + (clickShape * 0.40f);
    const float clickSample = ((1.0f - clickMix) * clickNoise + clickMix * clickTone) * clickEnv;

    const float punchAccent = 1.0f + (params.punch * pitchEnv * 0.45f);
    float sample = (body * ampEnv * punchAccent) + clickSample;
    sample *= velocityGain;

    const float drive = 1.0f + (params.drive * 7.0f);
    sample = fastTanh(sample * drive);

    const float lowCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * 180.0f / static_cast<float>(oversampledRate));
    toneLow = lowCoeff * toneLow + (1.0f - lowCoeff) * sample;
    const float toneHigh = sample - toneLow;
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

    const float dampingCutoff = std::clamp(2400.0f
                                               + (params.tone * 5000.0f)
                                               + ((1.0f - params.drive) * 3500.0f)
                                               + ((1.0f - params.material) * 1500.0f),
                                           1600.0f, 12000.0f);
    const float dampingCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * dampingCutoff / static_cast<float>(oversampledRate));
    dampingLowpass = dampingCoeff * dampingLowpass + (1.0f - dampingCoeff) * sample;
    const float dampingMix = std::clamp(0.20f + (params.drive * 0.34f) + (params.material * 0.22f), 0.0f, 0.90f);
    sample = sample * (1.0f - dampingMix) + dampingLowpass * dampingMix;

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

float KickSynthVoice::renderSoftStep() noexcept
{
    const float decaySeconds = 0.045f + (params.decay * 0.34f);
    const float ampDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * decaySeconds));

    const float punchAmount = 10.0f + (params.punch * 38.0f);
    const float pitchEnvSeconds = 0.018f + ((1.0f - params.punch) * 0.055f);
    const float pitchDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * pitchEnvSeconds));

    const float clickSeconds = 0.004f + ((1.0f - params.click) * 0.010f);
    const float clickDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * clickSeconds));

    const float pitchMultiplier = std::pow(2.0f, (pitchEnv * punchAmount) / 12.0f);
    const float bodyHz = std::clamp(fundamentalHz * pitchMultiplier, 20.0f, 1400.0f);
    const float bodyIncrement = static_cast<float>((kTwoPi * static_cast<double>(bodyHz)) / oversampledRate);
    phase += bodyIncrement;
    if (phase >= static_cast<float>(kTwoPi))
        phase -= static_cast<float>(kTwoPi);

    const float materialShape = params.material * params.material;
    const float fundamental = std::sin(phase);
    const float warmSecond = std::sin((phase * 2.0f) + 0.35f) * materialShape * 0.20f;
    const float warmThird = std::sin(phase * 3.0f) * materialShape * params.material * 0.055f;
    float body = (fundamental + warmSecond + warmThird) / (1.0f + materialShape * 0.16f);

    subPhase += bodyIncrement * 0.5f;
    if (subPhase >= static_cast<float>(kTwoPi))
        subPhase -= static_cast<float>(kTwoPi);
    body += std::sin(subPhase) * params.sub * 0.50f;

    const float noiseCutoff = 650.0f + (params.tone * 2200.0f) + (params.click * 1900.0f);
    const float noiseCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * noiseCutoff / static_cast<float>(oversampledRate));
    transientLowpass = noiseCoeff * transientLowpass + (1.0f - noiseCoeff) * nextNoise();

    const float clickHz = std::clamp(bodyHz * (1.8f + params.click * 2.2f), 160.0f, 3400.0f);
    clickPhase += static_cast<float>((kTwoPi * static_cast<double>(clickHz)) / oversampledRate);
    if (clickPhase >= static_cast<float>(kTwoPi))
        clickPhase -= static_cast<float>(kTwoPi);
    const float clickAmount = params.click * params.click;
    const float transient = ((transientLowpass * 0.68f) + (std::sin(clickPhase) * 0.32f))
                            * clickAmount * clickEnv * 0.82f;

    const float bodyEnvelope = ampEnv * (0.72f + ampEnv * 0.28f);
    const float punchAccent = 1.0f + (params.punch * pitchEnv * 0.20f);
    float sample = (body * bodyEnvelope * punchAccent) + transient;
    sample *= velocityGain;

    const float drive = 1.0f + (params.drive * 3.8f) + (materialShape * 0.7f);
    const float bias = materialShape * 0.08f;
    sample = fastTanh((sample * drive) + bias) - fastTanh(bias);

    const float lowCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * 150.0f / static_cast<float>(oversampledRate));
    toneLow = lowCoeff * toneLow + (1.0f - lowCoeff) * sample;
    const float toneHigh = sample - toneLow;
    const float toneTilt = (params.tone * 2.0f) - 1.0f;
    const float lowGain = 1.0f - std::max(0.0f, toneTilt) * 0.20f + std::max(0.0f, -toneTilt) * 0.70f;
    const float highGain = 1.0f + std::max(0.0f, toneTilt) * 0.42f - std::max(0.0f, -toneTilt) * 0.78f;
    sample = toneLow * lowGain + toneHigh * highGain;

    const float dampingCutoff = std::clamp(1100.0f
                                               + (params.tone * 4300.0f)
                                               + ((1.0f - params.drive) * 1800.0f)
                                               + ((1.0f - params.material) * 900.0f),
                                           900.0f, 7800.0f);
    const float dampingCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * dampingCutoff / static_cast<float>(oversampledRate));
    dampingLowpass = dampingCoeff * dampingLowpass + (1.0f - dampingCoeff) * sample;
    const float dampingMix = std::clamp(0.42f + (params.drive * 0.22f) + (params.material * 0.12f), 0.0f, 0.82f);
    sample = sample * (1.0f - dampingMix) + dampingLowpass * dampingMix;

    sample *= 0.2f + (params.output * 1.3f);
    sample = softClip(sample * 1.20f);
    sample = std::clamp(sample, -0.97f, 0.97f);

    ampEnv *= ampDecayCoeff;
    pitchEnv *= pitchDecayCoeff;
    clickEnv *= clickDecayCoeff;
    return sample;
}

float KickSynthVoice::renderHardStep() noexcept
{
    const float decaySeconds = 0.030f + (params.decay * 0.30f);
    const float ampDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * decaySeconds));

    const float punchAmount = 42.0f + (params.punch * 82.0f);
    const float pitchEnvSeconds = 0.004f + ((1.0f - params.punch) * 0.024f);
    const float pitchDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * pitchEnvSeconds));

    const float clickSeconds = 0.0012f + ((1.0f - params.click) * 0.0040f);
    const float clickDecayCoeff = std::exp(-1.0f / (static_cast<float>(oversampledRate) * clickSeconds));

    const float pitchMultiplier = std::pow(2.0f, (pitchEnv * punchAmount) / 12.0f);
    const float bodyHz = std::clamp(fundamentalHz * pitchMultiplier, 25.0f, 5200.0f);
    const float bodyIncrement = static_cast<float>((kTwoPi * static_cast<double>(bodyHz)) / oversampledRate);
    phase += bodyIncrement;
    if (phase >= static_cast<float>(kTwoPi))
        phase -= static_cast<float>(kTwoPi);

    const float materialShape = params.material * params.material;
    const float distortedPhase = phase + std::sin(phase) * (0.18f + materialShape * 1.10f);
    const float fundamental = std::sin(distortedPhase);
    const float second = std::sin(phase * 2.0f) * (0.10f + materialShape * 0.34f);
    const float third = std::sin((phase * 3.0f) + 0.4f) * materialShape * 0.22f;
    float body = (fundamental + second + third) / (1.10f + materialShape * 0.42f);

    subPhase += bodyIncrement * 0.5f;
    if (subPhase >= static_cast<float>(kTwoPi))
        subPhase -= static_cast<float>(kTwoPi);
    body += std::sin(subPhase) * params.sub * 0.34f;

    const float noise = nextNoise();
    const float noiseCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * 1700.0f / static_cast<float>(oversampledRate));
    transientLowpass = noiseCoeff * transientLowpass + (1.0f - noiseCoeff) * noise;
    const float highpassedNoise = noise - transientLowpass;

    const float clickHz = std::clamp(1700.0f + (params.click * params.click * 9000.0f) + (bodyHz * 2.8f), 1200.0f, 15000.0f);
    clickPhase += static_cast<float>((kTwoPi * static_cast<double>(clickHz)) / oversampledRate);
    if (clickPhase >= static_cast<float>(kTwoPi))
        clickPhase -= static_cast<float>(kTwoPi);
    const float clickAmount = params.click * params.click;
    const float transient = ((highpassedNoise * (0.52f + params.material * 0.18f))
                             + (std::sin(clickPhase) * (0.48f - params.material * 0.12f)))
                            * clickAmount * clickEnv * 1.15f;

    const float punchAccent = 1.0f + (params.punch * pitchEnv * 0.72f);
    float sample = (body * ampEnv * punchAccent) + transient;
    sample *= velocityGain;

    const float drive = 1.0f + (params.drive * 10.0f) + (materialShape * 2.0f);
    const float bias = 0.06f + (materialShape * 0.16f);
    sample = fastTanh((sample * drive) + bias) - fastTanh(bias);
    sample += transient * 0.22f;

    const float lowCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * 230.0f / static_cast<float>(oversampledRate));
    toneLow = lowCoeff * toneLow + (1.0f - lowCoeff) * sample;
    const float toneHigh = sample - toneLow;
    const float toneTilt = (params.tone * 2.0f) - 1.0f;
    const float lowGain = 1.0f - std::max(0.0f, toneTilt) * 0.38f + std::max(0.0f, -toneTilt) * 0.72f;
    const float highGain = 1.0f + std::max(0.0f, toneTilt) * 0.88f - std::max(0.0f, -toneTilt) * 0.72f;
    sample = toneLow * lowGain + toneHigh * highGain;

    const float dampingCutoff = std::clamp(4200.0f
                                               + (params.tone * 8200.0f)
                                               + ((1.0f - params.drive) * 2400.0f)
                                               + ((1.0f - params.material) * 1200.0f),
                                           3200.0f, 16000.0f);
    const float dampingCoeff = std::exp(-2.0f * static_cast<float>(kTwoPi) * dampingCutoff / static_cast<float>(oversampledRate));
    dampingLowpass = dampingCoeff * dampingLowpass + (1.0f - dampingCoeff) * sample;
    const float dampingMix = std::clamp(0.08f + (params.drive * 0.20f) + (params.material * 0.12f), 0.05f, 0.45f);
    sample = sample * (1.0f - dampingMix) + dampingLowpass * dampingMix;
    sample += (sample - dampingLowpass) * (0.18f + params.tone * 0.48f);

    sample *= 0.2f + (params.output * 1.3f);
    sample = softClip(sample * 1.48f);
    sample = std::clamp(sample, -0.97f, 0.97f);

    ampEnv *= ampDecayCoeff;
    pitchEnv *= pitchDecayCoeff;
    clickEnv *= clickDecayCoeff;
    return sample;
}

float KickSynthVoice::pushDownsampleFilter(const float sample) noexcept
{
    // 33-tap low-pass at the 2x rate; the stopband begins at the base-rate Nyquist.
    static constexpr std::array<float, kDownsampleFilterSize> coefficients {
        0.0015123472f, 0.0f, -0.0024849035f, -0.0022651511f, 0.0033450848f,
        0.0078212464f, 0.0f, -0.0151321582f, -0.0126182068f, 0.0168173737f,
        0.0360964201f, 0.0f, -0.0654293674f, -0.0574814479f, 0.0901953707f,
        0.2997959859f, 0.3996548121f, 0.2997959859f, 0.0901953707f,
        -0.0574814479f, -0.0654293674f, 0.0f, 0.0360964201f, 0.0168173737f,
        -0.0126182068f, -0.0151321582f, 0.0f, 0.0078212464f, 0.0033450848f,
        -0.0022651511f, -0.0024849035f, 0.0f, 0.0015123472f
    };

    downsampleHistory[downsampleWriteIndex] = sample;
    float filtered = 0.0f;
    size_t readIndex = downsampleWriteIndex;
    for (const float coefficient : coefficients)
    {
        filtered += coefficient * downsampleHistory[readIndex];
        readIndex = readIndex == 0 ? kDownsampleFilterSize - 1 : readIndex - 1;
    }
    downsampleWriteIndex = (downsampleWriteIndex + 1) % kDownsampleFilterSize;
    return filtered;
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
