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
    juce::MemoryBlock encodedFile;

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
    static constexpr size_t maximumEncodedBytes =
        64u * 1024u * 1024u;

    static SampleLoadResult load(const juce::File& file)
    {
        if (! file.existsAsFile())
            return { {}, "The selected sample file does not exist." };

        if (file.getSize()
            > (juce::int64) maximumEncodedBytes)
        {
            return {
                {},
                "The sample is larger than the 64 MiB embedded-sample limit."
            };
        }

        juce::MemoryBlock encodedFile;

        if (! file.loadFileAsData(encodedFile))
            return { {}, "The selected sample file could not be read." };

        return decode(
            encodedFile,
            file.getFileNameWithoutExtension(),
            file);
    }

    static SampleLoadResult load(
        const juce::MemoryBlock& encodedFile,
        const juce::String& name)
    {
        return decode(encodedFile, name, {});
    }

private:
    static SampleLoadResult decode(
        const juce::MemoryBlock& encodedFile,
        const juce::String& name,
        const juce::File& sourceFile)
    {
        if (encodedFile.isEmpty())
            return { {}, "The embedded sample contains no data." };

        if (encodedFile.getSize() > maximumEncodedBytes)
        {
            return {
                {},
                "The sample is larger than the 64 MiB embedded-sample limit."
            };
        }

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        auto stream =
            std::make_unique<juce::MemoryInputStream>(
                encodedFile,
                false);

        std::unique_ptr<juce::AudioFormatReader> reader(
            formats.createReaderFor(std::move(stream)));

        if (reader == nullptr)
            return { {}, "The data is not a supported WAV, AIFF or MP3 sample." };

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

        if (! reader->read(
                &loaded->audio,
                0,
                samples,
                0,
                true,
                true))
        {
            return { {}, "The sample could not be decoded." };
        }

        loaded->sampleRate = reader->sampleRate;
        loaded->name = name.isNotEmpty() ? name : "Embedded Sample";
        loaded->sourceFile = sourceFile;
        loaded->encodedFile = encodedFile;

        return { std::move(loaded), {} };
    }
};

} // namespace dyrekreds
