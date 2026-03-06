#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>

class WaveformCapture
{
public:
    static constexpr size_t kSize = 256;

    void pushSample(const float sample) noexcept
    {
        const auto index = writeIndex.fetch_add(1, std::memory_order_relaxed) % kSize;
        buffer[index].store(sample, std::memory_order_relaxed);
    }

    void snapshot(std::array<float, kSize>& destination) const noexcept
    {
        const auto idx = writeIndex.load(std::memory_order_relaxed);
        for (size_t i = 0; i < kSize; ++i)
        {
            const auto src = (idx + i) % kSize;
            destination[i] = std::clamp(buffer[src].load(std::memory_order_relaxed), -1.0f, 1.0f);
        }
    }

private:
    std::array<std::atomic<float>, kSize> buffer {};
    std::atomic<size_t> writeIndex { 0 };
};

