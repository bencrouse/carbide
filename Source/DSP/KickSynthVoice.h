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

private:
    static constexpr double kTwoPi = 6.28318530717958647692;

    float renderAtOversampleStep() noexcept;
    static float softClip(float x) noexcept;
    static float fastTanh(float x) noexcept;
    float nextNoise() noexcept;

    Params params {};

    double baseSampleRate = 48000.0;
    double oversampledRate = 96000.0;
    float phase = 0.0f;
    float subPhase = 0.0f;
    float ampEnv = 0.0f;
    float pitchEnv = 0.0f;
    float clickEnv = 0.0f;
    float toneLow = 0.0f;
    float toneHigh = 0.0f;
    float dampingLowpass = 0.0f;
    float velocityGain = 1.0f;
    float fundamentalHz = 52.0f;
    bool active = false;
    uint32_t noiseState = 0x12345678u;
};
