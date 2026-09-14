// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jon Skarin

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <limits>
#include <cmath>
#include <vector>

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
    int detectedMidiNote = -1;
    float detectedCents = 0.0f;
    float pitchConfidence = 0.0f;
    float normalisationGain = 1.0f;

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
    struct PitchEstimate
    {
        int midiNote = -1;
        float cents = 0.0f;
        float confidence = 0.0f;
    };

    // Lightweight YIN analysis, downsampled to keep imports responsive. It is
    // intentionally conservative: ambiguous/percussive material leaves Root alone.
    static PitchEstimate detectPitch(const juce::AudioBuffer<float>& audio,
                                     double sampleRate)
    {
        if (audio.getNumSamples() < 256 || sampleRate <= 0.0)
            return {};

        const int stride = juce::jmax(1, juce::roundToInt(sampleRate / 12000.0));
        const double analysisRate = sampleRate / (double) stride;
        const int maxPoints = 8192;
        const int points = juce::jmin(maxPoints, audio.getNumSamples() / stride);
        const int minLag = juce::jmax(2, (int) std::floor(analysisRate / 2000.0));
        const int maxLag = juce::jmin(points / 2, (int) std::ceil(analysisRate / 40.0));

        if (points < 256 || maxLag <= minLag)
            return {};

        std::vector<float> signal((size_t) points);
        double mean = 0.0;
        for (int i = 0; i < points; ++i)
        {
            float value = 0.0f;
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                value += audio.getSample(channel, i * stride);
            value /= (float) audio.getNumChannels();
            signal[(size_t) i] = value;
            mean += value;
        }
        mean /= (double) points;

        double energy = 0.0;
        for (auto& value : signal)
        {
            value -= (float) mean;
            energy += (double) value * value;
        }
        if (energy / (double) points < 1.0e-8)
            return {};

        std::vector<float> yin((size_t) maxLag + 1, 0.0f);
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            double difference = 0.0;
            const int count = points - lag;
            for (int i = 0; i < count; ++i)
            {
                const double delta = signal[(size_t) i] - signal[(size_t) (i + lag)];
                difference += delta * delta;
            }
            yin[(size_t) lag] = (float) difference;
        }

        double runningSum = 0.0;
        yin[0] = 1.0f;
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            runningSum += yin[(size_t) lag];
            yin[(size_t) lag] = runningSum > 0.0
                                  ? (float) (yin[(size_t) lag] * lag / runningSum)
                                  : 1.0f;
        }

        constexpr float threshold = 0.18f;
        int bestLag = -1;
        for (int lag = minLag; lag < maxLag; ++lag)
        {
            if (yin[(size_t) lag] < threshold)
            {
                while (lag + 1 <= maxLag
                       && yin[(size_t) (lag + 1)] < yin[(size_t) lag])
                    ++lag;
                bestLag = lag;
                break;
            }
        }

        if (bestLag < 0)
            return {};

        double refinedLag = (double) bestLag;
        if (bestLag > minLag && bestLag < maxLag)
        {
            const double left = yin[(size_t) (bestLag - 1)];
            const double centre = yin[(size_t) bestLag];
            const double right = yin[(size_t) (bestLag + 1)];
            const double denominator = left - 2.0 * centre + right;
            if (std::abs(denominator) > 1.0e-9)
                refinedLag += 0.5 * (left - right) / denominator;
        }

        const double frequency = analysisRate / refinedLag;
        const double midi = 69.0 + 12.0 * std::log2(frequency / 440.0);
        const int note = juce::jlimit(0, 127, juce::roundToInt(midi));
        const float confidence = 1.0f - yin[(size_t) bestLag];

        if (! std::isfinite(midi) || confidence < 0.75f)
            return {};

        return { note, (float) ((midi - note) * 100.0), confidence };
    }

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

        const auto pitch = detectPitch(loaded->audio, loaded->sampleRate);
        loaded->detectedMidiNote = pitch.midiNote;
        loaded->detectedCents = pitch.cents;
        loaded->pitchConfidence = pitch.confidence;

        float peak = 0.0f;
        for (int channel = 0; channel < loaded->audio.getNumChannels(); ++channel)
            peak = juce::jmax(peak, loaded->audio.getMagnitude(channel, 0, samples));

        constexpr float targetPeak = 0.8912509f; // -1 dBFS
        constexpr float maximumBoost = 7.9432823f; // +18 dB
        if (peak > 1.0e-6f && peak < targetPeak)
        {
            loaded->normalisationGain = juce::jmin(maximumBoost, targetPeak / peak);
            loaded->audio.applyGain(loaded->normalisationGain);
        }

        return { std::move(loaded), {} };
    }
};

} // namespace dyrekreds
