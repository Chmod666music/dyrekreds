// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jon Skarin

#pragma once

#include "SampleData.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dyrekreds
{

inline std::vector<float> buildSampleWaveform(const SampleData& sample)
{
    const int sampleCount = sample.audio.getNumSamples();
    const int channels = sample.audio.getNumChannels();
    if (sampleCount <= 0 || channels <= 0)
        return {};

    const int points = juce::jlimit(2, 8192, sampleCount);
    std::vector<float> waveform;
    waveform.reserve((size_t) points);

    for (int point = 0; point < points; ++point)
    {
        // The multiplication can exceed 32 bits even for a short sample.
        // An overflow here made findMinMax read before the audio buffer.
        const int start = (int) ((int64_t) point * sampleCount / points);
        const int next = (int) ((int64_t) (point + 1) * sampleCount / points);
        const int count = std::min(sampleCount - start,
                                   std::max(1, next - start));

        float peak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto range = sample.audio.findMinMax(channel, start, count);
            peak = juce::jmax(peak, std::abs(range.getStart()),
                              std::abs(range.getEnd()));
        }
        waveform.push_back(peak);
    }
    return waveform;
}

} // namespace dyrekreds
