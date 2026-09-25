// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Andrea De Murtas

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <functional>
#include "BlackMetalLookAndFeel.h"
#include "SampleData.h"

namespace galdr
{

// Lock-free mono tap the audio thread pushes into and a GUI timer drains.
class VisFifo
{
public:
    static constexpr int capacity = 1 << 14;

    void push(const float* l, const float* r, int numSamples, int channels)
    {
        if (fifo.getFreeSpace() < numSamples)
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead(numSamples - fifo.getFreeSpace(), s1, n1, s2, n2);
            fifo.finishedRead(n1 + n2);
        }
        int s1, n1, s2, n2;
        fifo.prepareToWrite(numSamples, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i)
            data[(size_t) (s1 + i)] = mono(l, r, i, channels);
        for (int i = 0; i < n2; ++i)
            data[(size_t) (s2 + i)] = mono(l, r, n1 + i, channels);
        fifo.finishedWrite(n1 + n2);
    }

    int pull(float* dest, int maxSamples)
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead(juce::jmin(maxSamples, fifo.getNumReady()), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i)
            dest[i] = data[(size_t) (s1 + i)];
        for (int i = 0; i < n2; ++i)
            dest[n1 + i] = data[(size_t) (s2 + i)];
        fifo.finishedRead(n1 + n2);
        return n1 + n2;
    }

private:
    static float mono(const float* l, const float* r, int i, int channels)
    {
        return channels > 1 ? 0.5f * (l[i] + r[i]) : l[i];
    }

    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> data {};
};

class ScopeComponent : public juce::Component, private juce::Timer
{
public:
    explicit ScopeComponent(VisFifo& f) : fifoRef(f)
    {
        setInterceptsMouseClicks(false, false);
        startTimerHz(30);
    }

private:
    void timerCallback() override
    {
        float tmp[2048];
        int got;
        while ((got = fifoRef.pull(tmp, 2048)) > 0)
            for (int i = 0; i < got; ++i)
            {
                ring[(size_t) writePos] = tmp[i];
                writePos = (writePos + 1) % (int) ring.size();
            }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xe60a1218));
        g.fillRect(b);
        g.setColour(theme::outline.withAlpha(0.6f));
        g.drawRect(b, 1.0f);
        g.setColour(theme::boneDim.withAlpha(0.2f));
        g.drawLine(b.getX(), b.getCentreY(), b.getRight(), b.getCentreY(), 1.0f);

        constexpr int windowLen = 1024;
        auto at = [this](int back)
        {
            return ring[(size_t) ((writePos + (int) ring.size() - 1 - back) % (int) ring.size())];
        };

        int trig = windowLen;
        for (int i = windowLen; i < windowLen + 900; ++i)
            if (at(i + 1) < 0.0f && at(i) >= 0.0f)
            {
                trig = i;
                break;
            }

        juce::Path p;
        for (int k = 0; k < windowLen; ++k)
        {
            const float v = juce::jlimit(-1.0f, 1.0f, at(trig - k));
            const float px = b.getX() + b.getWidth() * (float) k / (float) (windowLen - 1);
            const float py = b.getCentreY() - v * b.getHeight() * 0.45f;
            if (k == 0)
                p.startNewSubPath(px, py);
            else
                p.lineTo(px, py);
        }
        g.setColour(theme::blood.withAlpha(0.35f));
        g.strokePath(p, juce::PathStrokeType(3.0f));
        g.setColour(theme::bloodBright);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }

    VisFifo& fifoRef;
    std::array<float, 4096> ring {};
    int writePos = 0;
};

class SpectrumComponent : public juce::Component, private juce::Timer
{
public:
    SpectrumComponent(VisFifo& f, std::function<double()> sampleRateFn)
        : fifoRef(f), getSampleRate(std::move(sampleRateFn))
    {
        setInterceptsMouseClicks(false, false);
        display.fill(-100.0f);
        startTimerHz(30);
    }

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    void timerCallback() override
    {
        float tmp[2048];
        int got;
        while ((got = fifoRef.pull(tmp, 2048)) > 0)
            for (int i = 0; i < got; ++i)
            {
                ring[(size_t) ringPos] = tmp[i];
                ringPos = (ringPos + 1) % fftSize;
            }

        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi
                                                   * (float) i / (float) (fftSize - 1));
            fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)] * w;
        }
        std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        for (int bin = 0; bin < fftSize / 2; ++bin)
        {
            const float db = juce::Decibels::gainToDecibels(
                fftData[(size_t) bin] / (float) (fftSize / 4), -100.0f);
            display[(size_t) bin] = juce::jmax(db, display[(size_t) bin] - 2.0f);
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xe60a1218));
        g.fillRect(b);
        g.setColour(theme::outline.withAlpha(0.6f));
        g.drawRect(b, 1.0f);

        const double sr = juce::jmax(8000.0, getSampleRate());
        const int w = juce::jmax(2, (int) b.getWidth());

        juce::Path p;
        p.startNewSubPath(b.getX(), b.getBottom());
        for (int x = 0; x < w; ++x)
        {
            const float freq = 20.0f * std::pow(1000.0f, (float) x / (float) (w - 1));
            const int bin = juce::jlimit(1, fftSize / 2 - 1,
                (int) (freq / (float) (sr * 0.5) * (float) (fftSize / 2)));
            const float db = juce::jlimit(-100.0f, 0.0f, display[(size_t) bin]);
            const float y = juce::jmap(db, -100.0f, 0.0f, b.getBottom(), b.getY() + 2.0f);
            p.lineTo(b.getX() + (float) x, y);
        }
        p.lineTo(b.getRight(), b.getBottom());
        p.closeSubPath();

        g.setColour(theme::blood.withAlpha(0.25f));
        g.fillPath(p);
        g.setColour(theme::bloodBright.withAlpha(0.9f));
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }

    VisFifo& fifoRef;
    std::function<double()> getSampleRate;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> ring {};
    std::array<float, (size_t) fftSize * 2> fftData {};
    std::array<float, fftSize / 2> display {};
    int ringPos = 0;
};

    class SampleWaveformComponent : public juce::Component,
                                public juce::SettableTooltipClient,
                                private juce::Timer
{
    public:
    using SampleProvider =
    std::function<std::shared_ptr<const dyrekreds::SampleData>()>;

    using PlayheadProvider =
    std::function<float()>;

    SampleWaveformComponent(
    SampleProvider sampleProvider,
    PlayheadProvider playheadProvider,
    juce::RangedAudioParameter& position,
    juce::RangedAudioParameter& spreadParam,
    juce::RangedAudioParameter& sampleStart,
    juce::RangedAudioParameter& sampleEnd)
    : getSample(std::move(sampleProvider)),
      getPlayhead(std::move(playheadProvider)),
      positionParameter(position),
      spreadParameter(spreadParam),
      startParameter(sampleStart),
      endParameter(sampleEnd)
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    setTooltip("Wheel: zoom | Shift+wheel: scroll | Alt/middle drag: pan | Double-click: reset zoom");
    startTimerHz(30);
}

    private:
    void timerCallback() override
{
    const auto sample = getSample();

    if (sample != displayedSample)
    {
        displayedSample = sample;
        waveform.clear();
        zoom = 1.0f;
        viewStart = 0.0f;

        if (sample != nullptr && sample->isValid())
            buildWaveform(*sample);
    }

    playhead = juce::jlimit(
    0.0f,
    1.0f,
    getPlayhead());

    rangeStart = juce::jlimit(0.0f, 1.0f, startParameter.getValue());
    rangeEnd = juce::jlimit(0.0f, 1.0f, endParameter.getValue());
    if (rangeStart > rangeEnd)
        std::swap(rangeStart, rangeEnd);

    spread = juce::jlimit(
    0.0f,
    1.0f,
    spreadParameter.getValue()) * (rangeEnd - rangeStart);

    repaint();
}

    void buildWaveform(const dyrekreds::SampleData& sample)
    {
        const int sampleCount = sample.audio.getNumSamples();
        const int channels = sample.audio.getNumChannels();
        const int points = juce::jlimit(2, 8192, sampleCount);

        waveform.reserve((size_t) points);

        for (int point = 0; point < points; ++point)
        {
            const int start =
                point * sampleCount / points;

            const int end =
                juce::jmax(
                    start + 1,
                    (point + 1) * sampleCount / points);

            float peak = 0.0f;

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto range =
                    sample.audio.findMinMax(
                        channel,
                        start,
                        juce::jmin(sampleCount, end) - start);

                peak = juce::jmax(
                    peak,
                    std::abs(range.getStart()),
                    std::abs(range.getEnd()));
            }

            waveform.push_back(peak);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds =
            getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(juce::Colour(0xe60a1218));
        g.fillRect(bounds);

        g.setColour(theme::outline.withAlpha(0.6f));
        g.drawRect(bounds, 1.0f);

        g.setColour(theme::boneDim.withAlpha(0.18f));
        g.drawLine(
            bounds.getX(),
            bounds.getCentreY(),
            bounds.getRight(),
            bounds.getCentreY(),
            1.0f);

        if (waveform.empty())
        {
            g.setFont(12.0f);
            g.setColour(theme::boneDim.withAlpha(0.55f));
            g.drawText(
                "NO SAMPLE LOADED",
                getLocalBounds(),
                juce::Justification::centred);

            return;
        }
    const float spreadStart =
    juce::jlimit(
        0.0f,
        1.0f,
        playhead - spread);

    const float spreadEnd =
    juce::jlimit(
        0.0f,
        1.0f,
        playhead + spread);

    const float rangeStartX = samplePositionToX(rangeStart, bounds);
    const float rangeEndX = samplePositionToX(rangeEnd, bounds);

    g.setColour(juce::Colours::black.withAlpha(0.48f));
    if (rangeStartX > bounds.getX())
        g.fillRect(bounds.withRight(juce::jmin(bounds.getRight(), rangeStartX)));
    if (rangeEndX < bounds.getRight())
        g.fillRect(bounds.withLeft(juce::jmax(bounds.getX(), rangeEndX)));

    const float spreadX =
    samplePositionToX(spreadStart, bounds);

    const float spreadWidth =
    (spreadEnd - spreadStart) / viewLength() * bounds.getWidth();

    g.setColour(theme::blood.withAlpha(0.14f));
    g.fillRect(
    spreadX,
    bounds.getY(),
    spreadWidth,
    bounds.getHeight());

    g.setColour(theme::bloodBright.withAlpha(0.32f));
    g.drawVerticalLine(
    juce::roundToInt(spreadX),
    bounds.getY(),
    bounds.getBottom());

    g.drawVerticalLine(
    juce::roundToInt(spreadX + spreadWidth),
    bounds.getY(),
    bounds.getBottom());

        juce::Path path;
        const float centreY = bounds.getCentreY();
        const float halfHeight = bounds.getHeight() * 0.43f;

        const auto firstPoint = juce::jlimit<size_t>(
            0, waveform.size() - 1,
            (size_t) std::floor(viewStart * (float) (waveform.size() - 1)));
        const auto lastPoint = juce::jlimit<size_t>(
            firstPoint, waveform.size() - 1,
            (size_t) std::ceil((viewStart + viewLength())
                               * (float) (waveform.size() - 1)));

        for (size_t i = firstPoint; i <= lastPoint; ++i)
        {
            const float position = (float) i / (float) (waveform.size() - 1);
            const float x = samplePositionToX(position, bounds);

            const float height = waveform[i] * halfHeight;

            path.startNewSubPath(x, centreY - height);
            path.lineTo(x, centreY + height);
        }

        g.setColour(theme::blood.withAlpha(0.35f));
        g.strokePath(path, juce::PathStrokeType(3.0f));

        g.setColour(theme::bloodBright.withAlpha(0.90f));
        g.strokePath(path, juce::PathStrokeType(1.0f));
    const float playheadX = samplePositionToX(playhead, bounds);

    g.setColour(theme::blood.withAlpha(0.30f));
    g.drawLine(
    playheadX,
    bounds.getY(),
    playheadX,
    bounds.getBottom(),
    4.0f);

    g.setColour(theme::bloodBright);
    g.drawLine(
    playheadX,
    bounds.getY(),
    playheadX,
    bounds.getBottom(),
    1.25f);

    juce::Path marker;
    marker.addTriangle(
    playheadX - 4.0f,
    bounds.getY(),
    playheadX + 4.0f,
    bounds.getY(),
    playheadX,
    bounds.getY() + 6.0f);

    g.fillPath(marker);

    g.setColour(theme::boneDim.withAlpha(0.9f));
    if (bounds.contains(rangeStartX, bounds.getCentreY()))
        g.drawVerticalLine(juce::roundToInt(rangeStartX),
                           bounds.getY(), bounds.getBottom());
    if (bounds.contains(rangeEndX, bounds.getCentreY()))
        g.drawVerticalLine(juce::roundToInt(rangeEndX),
                           bounds.getY(), bounds.getBottom());

    constexpr float handleWidth = 5.0f;
    if (bounds.contains(rangeStartX, bounds.getCentreY()))
        g.fillRect(rangeStartX - handleWidth * 0.5f, bounds.getY(),
                   handleWidth, 12.0f);
    if (bounds.contains(rangeEndX, bounds.getCentreY()))
        g.fillRect(rangeEndX - handleWidth * 0.5f, bounds.getY(),
                   handleWidth, 12.0f);

    if (zoom > 1.001f)
    {
        g.setFont(10.0f);
        g.setColour(theme::boneDim.withAlpha(0.75f));
        g.drawText(juce::String(zoom, 1) + "x", getLocalBounds().reduced(5),
                   juce::Justification::topRight);
    }
    }
    void mouseDown(const juce::MouseEvent& event) override
{
    if (displayedSample == nullptr)
        return;

    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (event.mods.isMiddleButtonDown() || event.mods.isAltDown())
    {
        dragTarget = DragTarget::pan;
        panStart = viewStart;
        panMouseDownX = event.x;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }

    const auto startX = samplePositionToX(rangeStart, bounds);
    const auto endX = samplePositionToX(rangeEnd, bounds);

    if (std::abs((float) event.x - startX) <= 8.0f)
        dragTarget = DragTarget::start;
    else if (std::abs((float) event.x - endX) <= 8.0f)
        dragTarget = DragTarget::end;
    else
        dragTarget = DragTarget::position;

    activeParameter().beginChangeGesture();
    updateFromMouse(event.x);
}

    void mouseDrag(const juce::MouseEvent& event) override
{
    if (displayedSample == nullptr)
        return;

    if (dragTarget == DragTarget::pan)
    {
        const float delta = (float) (panMouseDownX - event.x)
                            / juce::jmax(1.0f, (float) getWidth()) * viewLength();
        viewStart = juce::jlimit(0.0f, 1.0f - viewLength(), panStart + delta);
        repaint();
        return;
    }

    updateFromMouse(event.x);
}

    void mouseUp(const juce::MouseEvent&) override
{
    if (displayedSample != nullptr && dragTarget != DragTarget::none)
    {
        if (dragTarget != DragTarget::pan)
            activeParameter().endChangeGesture();
        dragTarget = DragTarget::none;
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
}

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        zoom = 1.0f;
        viewStart = 0.0f;
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override
    {
        if (displayedSample == nullptr)
            return;

        if (event.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        {
            const float amount = std::abs(wheel.deltaX) > std::abs(wheel.deltaY)
                                     ? wheel.deltaX : wheel.deltaY;
            viewStart = juce::jlimit(0.0f, 1.0f - viewLength(),
                                     viewStart - amount * viewLength() * 0.35f);
        }
        else
        {
            const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            const float anchor = xToSamplePosition((float) event.x, bounds);
            const float anchorFraction = ((float) event.x - bounds.getX())
                                         / juce::jmax(1.0f, bounds.getWidth());
            zoom = juce::jlimit(1.0f, maximumZoom,
                                zoom * std::pow(2.0f, wheel.deltaY * 2.0f));
            viewStart = juce::jlimit(0.0f, 1.0f - viewLength(),
                                     anchor - anchorFraction * viewLength());
        }

        repaint();
    }

    juce::RangedAudioParameter& activeParameter() noexcept
    {
        if (dragTarget == DragTarget::start)
            return startParameter;
        if (dragTarget == DragTarget::end)
            return endParameter;
        return positionParameter;
    }

    void updateFromMouse(int mouseX)
{
    const auto bounds =
        getLocalBounds().toFloat().reduced(1.0f);

    const float normalised = xToSamplePosition((float) mouseX, bounds);

    constexpr float minimumRange = 0.001f;
    if (dragTarget == DragTarget::start)
    {
        startParameter.setValueNotifyingHost(
            juce::jmin(normalised, rangeEnd - minimumRange));
    }
    else if (dragTarget == DragTarget::end)
    {
        endParameter.setValueNotifyingHost(
            juce::jmax(normalised, rangeStart + minimumRange));
    }
    else
    {
        const auto selectedLength = juce::jmax(minimumRange, rangeEnd - rangeStart);
        positionParameter.setValueNotifyingHost(
            juce::jlimit(0.0f, 1.0f, (normalised - rangeStart) / selectedLength));
    }
}

    float viewLength() const noexcept { return 1.0f / zoom; }

    float xToSamplePosition(float x, juce::Rectangle<float> bounds) const noexcept
    {
        const float fraction = juce::jlimit(0.0f, 1.0f,
            (x - bounds.getX()) / juce::jmax(1.0f, bounds.getWidth()));
        return viewStart + fraction * viewLength();
    }

    float samplePositionToX(float position, juce::Rectangle<float> bounds) const noexcept
    {
        return bounds.getX() + (position - viewStart) / viewLength() * bounds.getWidth();
    }
    SampleProvider getSample;
    PlayheadProvider getPlayhead;
    juce::RangedAudioParameter& positionParameter;
    juce::RangedAudioParameter& spreadParameter;
    juce::RangedAudioParameter& startParameter;
    juce::RangedAudioParameter& endParameter;
    std::shared_ptr<const dyrekreds::SampleData> displayedSample;
    std::vector<float> waveform;
    float playhead = 0.0f;
    float spread = 0.0f;
    float rangeStart = 0.0f;
    float rangeEnd = 1.0f;
    static constexpr float maximumZoom = 64.0f;
    float zoom = 1.0f;
    float viewStart = 0.0f;
    float panStart = 0.0f;
    int panMouseDownX = 0;
    enum class DragTarget { none, position, start, end, pan };
    DragTarget dragTarget = DragTarget::none;
};
} // namespace galdr
