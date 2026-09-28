#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

class KickSynthVoice
{
public:
    struct Params
    {
        float pitch = 0.0f;
        float decay = 0.5f;
        float punch = 0.5f;
        float click = 0.3f;
        float material = 0.0f;
        float drive = 0.2f;
        float tone = 0.5f;
        float sub = 0.5f;
        float output = 0.8f;
        int mode = 1; // 0=Soft, 1=Neutral, 2=Hard
    };

    void prepare(double sampleRateHz) noexcept;
    void setParams(const Params& newParams) noexcept;
    void trigger(int midiNote, float velocity) noexcept;
    float process() noexcept;
    bool isActive() const noexcept;
    float getFundamentalHz() const noexcept;
    static constexpr double getMaximumTailSeconds(double sampleRateHz) noexcept
    {
        return 4.02 + (16.0 / (sampleRateHz > 0.0 ? sampleRateHz : 1.0));
    }
    static constexpr int getLatencySamples() noexcept { return 8; }

private:
    enum class Mode
    {
        soft,
        neutral,
        hard
    };

    static constexpr double kTwoPi = 6.28318530717958647692;
    static constexpr size_t kDownsampleFilterSize = 33;
    static constexpr int kFilteredDrainSamples = 16;

    float renderAtOversampleStep() noexcept;
    float renderSoftStep() noexcept;
    float renderNeutralStep() noexcept;
    float renderHardStep() noexcept;
    float pushDownsampleFilter(float sample) noexcept;
    static float softClip(float x) noexcept;
    static float fastTanh(float x) noexcept;
    float nextNoise() noexcept;

    Params params {};

    double oversampledRate = 96000.0;
    float phase = 0.0f;
    float subPhase = 0.0f;
    float clickPhase = 0.0f;
    float ampEnv = 0.0f;
    float pitchEnv = 0.0f;
    float clickEnv = 0.0f;
    float toneLow = 0.0f;
    float dampingLowpass = 0.0f;
    float transientLowpass = 0.0f;
    float velocityGain = 1.0f;
    float fundamentalHz = 52.0f;
    Mode latchedMode = Mode::neutral;
    bool active = false;
    int filterDrainSamplesRemaining = 0;
    uint32_t noiseState = 0x12345678u;
    std::array<float, kDownsampleFilterSize> downsampleHistory {};
    size_t downsampleWriteIndex = 0;
};
