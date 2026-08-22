// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jon Skarin

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <limits>

namespace dyrekreds
{

// Immutable after loading. Instances can therefore be shared safely between
// the message thread and audio voices through an atomic shared_ptr.
struct SampleData
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 0.0;
    juce::String name;
    juce::File sourceFile;

    bool isValid() const noexcept
    {
        return sampleRate > 0.0
            && audio.getNumChannels() > 0
            && audio.getNumSamples() > 0;
    }

    double durationSeconds() const noexcept
    {
        return isValid() ? (double) audio.getNumSamples() / sampleRate : 0.0;
    }
};

struct SampleLoadResult
{
    std::shared_ptr<const SampleData> sample;
    juce::String error;

    explicit operator bool() const noexcept
    {
        return sample != nullptr;
    }
};

class SampleLoader
{
public:
    static SampleLoadResult load(const juce::File& file)
    {
        if (! file.existsAsFile())
            return { {}, "The selected sample file does not exist." };

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(
            formats.createReaderFor(file));

        if (reader == nullptr)
            return { {}, "The file is not a supported WAV or AIFF sample." };

        if (reader->sampleRate <= 0.0
            || reader->lengthInSamples <= 0
            || reader->numChannels == 0)
        {
            return { {}, "The sample contains no readable audio." };
        }

        constexpr double maximumDurationSeconds = 5.0 * 60.0;
        const double duration =
            (double) reader->lengthInSamples / reader->sampleRate;

        if (duration > maximumDurationSeconds)
            return { {}, "The sample is longer than the five-minute limit." };

        if (reader->lengthInSamples
            > (juce::int64) std::numeric_limits<int>::max())
        {
            return { {}, "The sample is too large to load." };
        }

        auto loaded = std::make_shared<SampleData>();
        const int channels =
            juce::jlimit(1, 2, (int) reader->numChannels);
        const int samples = (int) reader->lengthInSamples;

        loaded->audio.setSize(channels, samples);

        if (! reader->read(&loaded->audio,
                           0,
                           samples,
                           0,
                           true,
                           true))
        {
            return { {}, "The sample could not be decoded." };
        }

        loaded->sampleRate = reader->sampleRate;
        loaded->name = file.getFileNameWithoutExtension();
        loaded->sourceFile = file;

        return { std::move(loaded), {} };
    }
};

} // namespace dyrekreds
