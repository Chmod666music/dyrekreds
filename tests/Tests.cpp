// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Andrea De Murtas
//
// Headless synth tests: note lifecycle, oscillator pitch, Scala microtuning,
// mono note priority, arpeggiator and delay tempo sync, MIDI-learn and state
// round-trips, factory presets and numerical safety under extreme settings.
// Exits non-zero on failure; run by CI on every platform.

#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>
#include "../src/PluginProcessor.h"
#include "../src/Presets.h"

namespace
{

int failures = 0;

void check(bool condition, const juce::String& what)
{
    std::cout << (condition ? "  ok: " : "FAIL: ") << what << std::endl;
    if (! condition)
        ++failures;
}

void setParam(GaldrAudioProcessor& p, const char* id, float value)
{
    auto* rp = p.apvts.getParameter(id);
    jassert(rp != nullptr);
    rp->setValueNotifyingHost(rp->convertTo0to1(value));
}

// Every effect after the voices turned off, so the output is the raw voice
// mix through the (transparent at these levels) tanh output stage.
void neutralise(GaldrAudioProcessor& p)
{
    setParam(p, pid::distMix, 0.0f);
    setParam(p, pid::crushMix, 0.0f);
    setParam(p, pid::rmMix, 0.0f);
    setParam(p, pid::chorusMix, 0.0f);
    setParam(p, pid::tremDepth, 0.0f);
    setParam(p, pid::delayMix, 0.0f);
    setParam(p, pid::revMix, 0.0f);
    setParam(p, pid::bzLvl, 0.0f);
    setParam(p, pid::lfo1Depth, 0.0f);
    setParam(p, pid::lfo2Depth, 0.0f);
    setParam(p, pid::driftAmt, 0.0f);
}

struct Event
{
    int sample;
    juce::MidiMessage msg;
};

// Renders the synth with the given MIDI events (sample positions relative to
// the start of the render) and returns the first output channel.
std::vector<float> render(GaldrAudioProcessor& p, double sr, int blockSize, double seconds,
                          const std::vector<Event>& events)
{
    const int total = ((int) std::ceil(seconds * sr / blockSize)) * blockSize;
    std::vector<float> out;
    out.reserve((size_t) total);

    juce::AudioBuffer<float> buffer(juce::jmax(1, p.getTotalNumOutputChannels()), blockSize);

    for (int pos = 0; pos < total; pos += blockSize)
    {
        juce::MidiBuffer midi;
        for (const auto& e : events)
            if (e.sample >= pos && e.sample < pos + blockSize)
                midi.addEvent(e.msg, e.sample - pos);
        p.processBlock(buffer, midi);
        for (int i = 0; i < blockSize; ++i)
            out.push_back(buffer.getSample(0, i));
    }
    return out;
}

float peakIn(const std::vector<float>& v, double sr, double t0, double t1)
{
    const int a = juce::jlimit(0, (int) v.size(), (int) (t0 * sr));
    const int b = juce::jlimit(0, (int) v.size(), (int) (t1 * sr));
    float m = 0.0f;
    for (int i = a; i < b; ++i)
        m = juce::jmax(m, std::abs(v[(size_t) i]));
    return m;
}

// Fundamental estimate from interpolated positive-going zero crossings; good
// to a small fraction of a cycle on the sine-wave patches the tests use.
float measureFreq(const std::vector<float>& v, double sr, double t0, double t1)
{
    const int a = juce::jlimit(1, (int) v.size(), (int) (t0 * sr));
    const int b = juce::jlimit(1, (int) v.size(), (int) (t1 * sr));
    double first = -1.0, last = -1.0;
    int count = 0;
    for (int i = a; i < b; ++i)
        if (v[(size_t) (i - 1)] <= 0.0f && v[(size_t) i] > 0.0f)
        {
            const double frac = v[(size_t) (i - 1)]
                                / ((double) v[(size_t) (i - 1)] - v[(size_t) i]);
            const double t = ((double) (i - 1) + frac) / sr;
            if (first < 0.0)
                first = t;
            last = t;
            ++count;
        }
    return count > 1 ? (float) ((count - 1) / (last - first)) : 0.0f;
}

bool allFinite(const std::vector<float>& v, float& worst)
{
    worst = 0.0f;
    bool ok = true;
    for (float x : v)
    {
        if (! std::isfinite(x))
            return false;
        worst = juce::jmax(worst, std::abs(x));
        // the tanh output stage bounds every sample below unity
        ok = ok && worst <= 1.01f;
    }
    return ok;
}

void testStateRoundTrip()
{
    std::cout << "state round-trip" << std::endl;
    GaldrAudioProcessor a;
    juce::Random rng(0x5EED);
    for (auto* param : a.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            // snap through the parameter's own range so stepped parameters
            // (bools, choices) hold a legal value, as any host set would
            const float denorm = rp->convertFrom0to1(rng.nextFloat());
            rp->setValueNotifyingHost(rp->convertTo0to1(denorm));
        }

    juce::MemoryBlock state;
    a.getStateInformation(state);

    GaldrAudioProcessor b;
    b.setStateInformation(state.getData(), (int) state.getSize());

    bool same = true;
    for (int i = 0; i < a.getParameters().size(); ++i)
    {
        auto* pa = dynamic_cast<juce::RangedAudioParameter*>(a.getParameters()[i]);
        auto* pb = dynamic_cast<juce::RangedAudioParameter*>(b.getParameters()[i]);
        if (pa != nullptr && pb != nullptr
            && std::abs(pa->getValue() - pb->getValue()) > 1.0e-5f)
        {
            same = false;
            std::cout << "  mismatch: " << pa->paramID << std::endl;
        }
    }
    check(same, "all parameter values survive save/load");

    auto tree = b.capturePresetState();
    tree.removeProperty("stateVersion", nullptr);
    b.applyStateTree(tree);
    check(true, "version-0 state applies without crashing");
}

void testMidiLearn()
{
    std::cout << "midi learn and cc map persistence" << std::endl;
    const double sr = 48000.0;
    GaldrAudioProcessor p;
    p.prepareToPlay(sr, 256);

    p.armMidiLearn(pid::cutoff);
    check(p.isMidiLearnArmed(), "learn armed");

    render(p, sr, 256, 0.01, { { 0, juce::MidiMessage::controllerEvent(1, 74, 64) } });
    check(! p.isMidiLearnArmed(), "first cc consumes the armed learn");
    check(p.midiCCFor(pid::cutoff) == 74, "cutoff bound to cc 74");

    render(p, sr, 256, 0.01, { { 0, juce::MidiMessage::controllerEvent(1, 74, 127) } });
    check(std::abs(p.apvts.getParameter(pid::cutoff)->getValue() - 1.0f) < 1.0e-4f,
          "mapped cc drives the parameter");

    // host sessions carry the map, preset files must not
    auto full = p.captureFullState();
    check(full.getChildWithName("MIDIMAP").isValid(), "full state stores the map");
    check(! p.capturePresetState().getChildWithName("MIDIMAP").isValid(),
          "preset state omits the map");

    GaldrAudioProcessor b;
    b.applyStateTree(full);
    check(b.midiCCFor(pid::cutoff) == 74, "map survives a session round-trip");

    b.applyStateTree(b.capturePresetState());
    check(b.midiCCFor(pid::cutoff) == 74, "loading a preset keeps the controller setup");
}

void testTuningParser()
{
    std::cout << "scala parser" << std::endl;
    galdr::Tuning t;

    check(std::abs(t.freqs[69] - 440.0f) < 0.01f, "12-TET anchors A4 at 440 Hz");
    check(std::abs(t.freqs[72] / t.freqs[60] - 2.0f) < 1.0e-4f, "octave doubles");

    // 12-TET written out as cents must reproduce the default table
    juce::String twelve("! comment\ntwelve\n12\n");
    for (int i = 1; i <= 12; ++i)
        twelve << juce::String(i * 100) << ".0\n";
    galdr::Tuning c;
    check(c.loadSclText(twelve, "twelve"), "cents scale parses");
    bool same = true;
    for (int n = 0; n < 128; ++n)
        same = same && std::abs(c.freqs[n] / t.freqs[n] - 1.0f) < 1.0e-4f;
    check(same, "12 x 100 cents matches 12-TET across the range");

    galdr::Tuning r;
    check(r.loadSclText("fifth\n2\n3/2\n2/1\n", "fifth"), "ratio scale parses");
    check(std::abs(r.freqs[61] / r.freqs[60] - 1.5f) < 1.0e-4f, "3/2 lands a just fifth");
    check(std::abs(r.freqs[62] / r.freqs[60] - 2.0f) < 1.0e-4f, "2/1 closes the octave");
    check(std::abs(r.freqs[60] - 261.6256f) < 0.01f, "middle C stays anchored");

    galdr::Tuning bad;
    check(! bad.loadSclText("", "empty"), "empty file rejected");
    check(! bad.loadSclText("desc\n0\n", "zero"), "zero notes rejected");
    check(! bad.loadSclText("desc\n2\n0/4\n2/1\n", "ratio"), "non-positive ratio rejected");
    check(std::abs(bad.freqs[69] - 440.0f) < 0.01f, "failed load leaves the table untouched");
}

void testNoteLifecycle()
{
    std::cout << "note lifecycle" << std::endl;
    const double sr = 48000.0;
    GaldrAudioProcessor p;
    neutralise(p);
    setParam(p, pid::osc1Wave, 4.0f); // sine
    setParam(p, pid::release, 0.05f);
    p.prepareToPlay(sr, 512);

    auto out = render(p, sr, 512, 2.0, {
        { (int) (0.5 * sr), juce::MidiMessage::noteOn(1, 69, 0.8f) },
        { (int) (1.0 * sr), juce::MidiMessage::noteOff(1, 69) },
    });

    check(peakIn(out, sr, 0.1, 0.45) < 1.0e-4f, "silent before the note");
    check(peakIn(out, sr, 0.6, 0.95) > 0.02f, "sounding while held");
    check(peakIn(out, sr, 1.4, 1.9) < 1.0e-3f, "silent again after the release");
}

void testPitchAccuracy()
{
    std::cout << "oscillator pitch" << std::endl;
    for (const double sr : { 44100.0, 48000.0, 96000.0 })
        for (const int note : { 57, 69, 81 })
        {
            GaldrAudioProcessor p;
            neutralise(p);
            setParam(p, pid::osc1Wave, 4.0f); // sine
            p.prepareToPlay(sr, 512);

            auto out = render(p, sr, 512, 1.0,
                              { { 0, juce::MidiMessage::noteOn(1, note, 0.8f) } });

            const float expected = 440.0f * std::exp2((float) (note - 69) / 12.0f);
            const float got = measureFreq(out, sr, 0.3, 0.9);
            check(std::abs(got / expected - 1.0f) < 0.005f,
                  "note " + juce::String(note) + " at " + juce::String((int) sr)
                      + " Hz plays " + juce::String(expected, 1) + " Hz (got "
                      + juce::String(got, 2) + ")");
        }
}

void testTuningIntegration()
{
    std::cout << "microtuning end to end" << std::endl;
    const double sr = 48000.0;

    juce::String edo24("24-EDO\n24\n");
    for (int i = 1; i <= 24; ++i)
        edo24 << juce::String(i * 50) << ".0\n";
    auto file = juce::File::createTempFile("scl");
    file.replaceWithText(edo24);

    GaldrAudioProcessor p;
    neutralise(p);
    setParam(p, pid::osc1Wave, 4.0f);
    p.prepareToPlay(sr, 512);
    check(p.loadTuning(file), "loads a 24-EDO .scl file");

    // note 69 is 9 quarter-tone steps above the middle C anchor
    const float expected = 261.6255653f * std::exp2(450.0f / 1200.0f);
    auto out = render(p, sr, 512, 1.0, { { 0, juce::MidiMessage::noteOn(1, 69, 0.8f) } });
    check(std::abs(measureFreq(out, sr, 0.3, 0.9) / expected - 1.0f) < 0.005f,
          "quarter-tone table retunes the voices");

    // the state embeds the .scl text: restoring must not need the file
    auto state = p.captureFullState();
    file.deleteFile();

    GaldrAudioProcessor b;
    neutralise(b);
    setParam(b, pid::osc1Wave, 4.0f);
    b.applyStateTree(state);
    b.prepareToPlay(sr, 512);
    check(b.getTuningName() == p.getTuningName(), "tuning name survives the round-trip");
    auto out2 = render(b, sr, 512, 1.0, { { 0, juce::MidiMessage::noteOn(1, 69, 0.8f) } });
    check(std::abs(measureFreq(out2, sr, 0.3, 0.9) / expected - 1.0f) < 0.005f,
          "embedded tuning restores without the original file");

    b.resetTuning();
    auto out3 = render(b, sr, 512, 1.0, { { 0, juce::MidiMessage::noteOn(1, 69, 0.8f) } });
    check(std::abs(measureFreq(out3, sr, 0.3, 0.9) / 440.0f - 1.0f) < 0.005f,
          "reset returns to 12-TET");
}

void testMonoPriority()
{
    std::cout << "mono note priority" << std::endl;
    const double sr = 48000.0;
    GaldrAudioProcessor p;
    neutralise(p);
    setParam(p, pid::osc1Wave, 4.0f);
    setParam(p, pid::voiceMode, 1.0f); // mono
    p.prepareToPlay(sr, 512);

    auto out = render(p, sr, 512, 2.1, {
        { (int) (0.05 * sr), juce::MidiMessage::noteOn(1, 57, 0.8f) },
        { (int) (0.70 * sr), juce::MidiMessage::noteOn(1, 64, 0.8f) },
        { (int) (1.40 * sr), juce::MidiMessage::noteOff(1, 64) },
    });

    check(std::abs(measureFreq(out, sr, 0.3, 0.6) / 220.0f - 1.0f) < 0.005f,
          "first note sounds alone");
    check(std::abs(measureFreq(out, sr, 0.9, 1.3) / 329.63f - 1.0f) < 0.005f,
          "newer note takes the voice");
    check(std::abs(measureFreq(out, sr, 1.7, 2.05) / 220.0f - 1.0f) < 0.005f,
          "releasing it falls back to the held note");
}

void testArpTiming(int blockSize)
{
    std::cout << "arpeggiator timing, block " << blockSize << std::endl;
    const double sr = 48000.0;
    GaldrAudioProcessor p;
    neutralise(p);
    setParam(p, pid::osc1Wave, 4.0f);
    setParam(p, pid::attack, 0.001f);
    setParam(p, pid::release, 0.001f);
    setParam(p, pid::arpMode, 1.0f); // up
    setParam(p, pid::arpRate, 3.0f); // 1/8 at the default 120 bpm = 0.25 s
    setParam(p, pid::arpGate, 0.5f);
    p.prepareToPlay(sr, blockSize);

    auto out = render(p, sr, blockSize, 2.0,
                      { { 100, juce::MidiMessage::noteOn(1, 60, 0.8f) } });

    // onset = a 1 ms window above -34 dB after three windows of silence
    const int win = (int) (0.001 * sr);
    std::vector<float> env;
    for (int i = 0; i + win <= (int) out.size(); i += win)
    {
        float m = 0.0f;
        for (int k = 0; k < win; ++k)
            m = juce::jmax(m, std::abs(out[(size_t) (i + k)]));
        env.push_back(m);
    }
    std::vector<double> onsets;
    for (int k = 3; k < (int) env.size(); ++k)
        if (env[(size_t) k] > 0.02f && env[(size_t) (k - 1)] < 0.005f
            && env[(size_t) (k - 2)] < 0.005f && env[(size_t) (k - 3)] < 0.005f)
            onsets.push_back(k * 0.001);

    check((int) onsets.size() >= 6, "at least six steps fire in two seconds (got "
                                        + juce::String((int) onsets.size()) + ")");
    bool spaced = onsets.size() >= 2;
    for (size_t i = 1; i < onsets.size(); ++i)
        spaced = spaced && std::abs(onsets[i] - onsets[i - 1] - 0.25) < 0.01;
    check(spaced, "steps land a quarter second apart");
}

void testDelaySync()
{
    std::cout << "delay tempo sync" << std::endl;
    const double sr = 48000.0;
    GaldrAudioProcessor p;
    neutralise(p);
    setParam(p, pid::osc1Wave, 4.0f);
    setParam(p, pid::attack, 0.001f);
    setParam(p, pid::decay, 0.03f);
    setParam(p, pid::sustain, 0.0f);
    setParam(p, pid::release, 0.001f);
    setParam(p, pid::delayMix, 1.0f);
    setParam(p, pid::delayFb, 0.0f);
    setParam(p, pid::delaySync, 4.0f); // 1/4 at the default 120 bpm = 0.5 s
    p.prepareToPlay(sr, 512);

    auto out = render(p, sr, 512, 1.4, {
        { (int) (0.3 * sr), juce::MidiMessage::noteOn(1, 69, 0.9f) },
        { (int) (0.4 * sr), juce::MidiMessage::noteOff(1, 69) },
    });

    // the direct burst is the global peak; the echo is the peak well after it
    int direct = 0;
    float best = 0.0f;
    for (int i = 0; i < (int) out.size(); ++i)
        if (std::abs(out[(size_t) i]) > best)
        {
            best = std::abs(out[(size_t) i]);
            direct = i;
        }
    int echo = direct;
    best = 0.0f;
    for (int i = direct + (int) (0.3 * sr); i < (int) out.size(); ++i)
        if (std::abs(out[(size_t) i]) > best)
        {
            best = std::abs(out[(size_t) i]);
            echo = i;
        }
    check(std::abs((echo - direct) - (int) (0.5 * sr)) < (int) (0.01 * sr),
          "quarter-note echo lands half a second behind the note");
}

void testFactoryPresets()
{
    std::cout << "factory presets render" << std::endl;
    const double sr = 48000.0;
    GaldrAudioProcessor p;
    bool finite = true;
    juce::String culprit;

    for (int i = 0; i < (int) presets::all().size(); ++i)
    {
        presets::apply(p.apvts, i);
        p.prepareToPlay(sr, 512);
        auto out = render(p, sr, 512, 0.9, {
            { (int) (0.05 * sr), juce::MidiMessage::noteOn(1, 48, 0.8f) },
            { (int) (0.05 * sr), juce::MidiMessage::noteOn(1, 55, 0.8f) },
            { (int) (0.05 * sr), juce::MidiMessage::noteOn(1, 60, 0.8f) },
            { (int) (0.45 * sr), juce::MidiMessage::noteOff(1, 48) },
            { (int) (0.45 * sr), juce::MidiMessage::noteOff(1, 55) },
            { (int) (0.45 * sr), juce::MidiMessage::noteOff(1, 60) },
        });
        float worst = 0.0f;
        if (! allFinite(out, worst))
        {
            finite = false;
            culprit = presets::all()[(size_t) i].name;
        }
    }
    check(finite, juce::String((int) presets::all().size())
                      + " presets all stay finite and bounded"
                      + (finite ? "" : " (first offender: " + culprit + ")"));
}

void testMonoBus()
{
    std::cout << "mono output bus" << std::endl;
    const double sr = 48000.0;
    GaldrAudioProcessor p;
    auto layout = p.getBusesLayout();
    layout.outputBuses.getReference(0) = juce::AudioChannelSet::mono();
    check(p.setBusesLayout(layout), "mono layout accepted");
    setParam(p, pid::revMix, 0.5f);
    setParam(p, pid::delayMix, 0.3f);
    setParam(p, pid::chorusMix, 0.5f);
    p.prepareToPlay(sr, 512);

    auto out = render(p, sr, 512, 0.6, {
        { 0, juce::MidiMessage::noteOn(1, 60, 0.8f) },
        { (int) (0.3 * sr), juce::MidiMessage::noteOff(1, 60) },
    });
    float worst = 0.0f;
    check(allFinite(out, worst) && peakIn(out, sr, 0.05, 0.3) > 0.01f,
          "renders sound through the whole chain on one channel");
}

void testFiniteness()
{
    std::cout << "finiteness under extreme settings" << std::endl;
    for (const auto [sr, blockSize] : { std::pair { 44100.0, 64 }, { 96000.0, 2048 } })
    {
        GaldrAudioProcessor p;
        setParam(p, pid::osc1Wave, 5.0f); // wavetable
        setParam(p, pid::osc1Uni, 7.0f);
        setParam(p, pid::osc1Det, 100.0f);
        setParam(p, pid::osc1Spread, 1.0f);
        setParam(p, pid::osc1Lvl, 1.0f);
        setParam(p, pid::osc2Wave, 2.0f);
        setParam(p, pid::osc2Uni, 7.0f);
        setParam(p, pid::osc2Semi, 12.0f);
        setParam(p, pid::osc2Lvl, 1.0f);
        setParam(p, pid::subLvl, 1.0f);
        setParam(p, pid::noiseLvl, 1.0f);
        setParam(p, pid::fmAmt, 1.0f);
        setParam(p, pid::oscSync, 1.0f);
        setParam(p, pid::driftAmt, 1.0f);
        setParam(p, pid::filterType, 4.0f); // formant
        setParam(p, pid::cutoff, 20000.0f);
        setParam(p, pid::resonance, 1.0f);
        setParam(p, pid::filterDrive, 1.0f);
        setParam(p, pid::fEnvAmt, -1.0f);
        setParam(p, pid::keytrack, 1.0f);
        setParam(p, pid::glide, 1.0f);
        setParam(p, pid::lfo1Shape, 4.0f); // S&H
        setParam(p, pid::lfo1Rate, 20.0f);
        setParam(p, pid::lfo1Depth, 1.0f);
        setParam(p, pid::lfo2Rate, 20.0f);
        setParam(p, pid::lfo2Depth, 1.0f);
        for (int s = 0; s < GaldrVoice::numModSlots; ++s)
        {
            setParam(p, pid::modSrc[s], (float) (1 + s % 8));
            setParam(p, pid::modDst[s], (float) (1 + s * 2 % 15));
            setParam(p, pid::modAmt[s], s % 2 == 0 ? 1.0f : -1.0f);
        }
        setParam(p, pid::distType, 3.0f); // grim
        setParam(p, pid::distDrive, 1.0f);
        setParam(p, pid::crushBits, 2.0f);
        setParam(p, pid::crushRate, 50.0f);
        setParam(p, pid::chorusDepth, 1.0f);
        setParam(p, pid::chorusMix, 1.0f);
        setParam(p, pid::tremDepth, 1.0f);
        setParam(p, pid::tremRate, 30.0f);
        setParam(p, pid::delayTime, 0.02f);
        setParam(p, pid::delayFb, 0.95f);
        setParam(p, pid::delayMix, 1.0f);
        setParam(p, pid::revSize, 1.0f);
        setParam(p, pid::revShimmer, 1.0f);
        setParam(p, pid::revMix, 1.0f);
        setParam(p, pid::bzDensity, 200.0f);
        setParam(p, pid::bzSize, 0.5f);
        setParam(p, pid::bzLvl, 1.0f);
        setParam(p, pid::bzGate, 1.0f); // free-running
        setParam(p, pid::rmFreq, 5000.0f);
        setParam(p, pid::rmMix, 1.0f);
        setParam(p, pid::gain, 1.0f);
        p.prepareToPlay(sr, blockSize);

        // a storm of notes, bends, pressure and mapped controllers
        juce::Random rng(1234);
        std::vector<Event> events;
        for (int i = 0; i < 120; ++i)
        {
            const int at = rng.nextInt((int) (1.5 * sr));
            const int note = 24 + rng.nextInt(72);
            switch (rng.nextInt(5))
            {
                case 0: events.push_back({ at, juce::MidiMessage::noteOn(1, note, 0.5f + 0.5f * rng.nextFloat()) }); break;
                case 1: events.push_back({ at, juce::MidiMessage::noteOff(1, note) }); break;
                case 2: events.push_back({ at, juce::MidiMessage::pitchWheel(1, rng.nextInt(16384)) }); break;
                case 3: events.push_back({ at, juce::MidiMessage::controllerEvent(1, 1, rng.nextInt(128)) }); break;
                default: events.push_back({ at, juce::MidiMessage::channelPressureChange(1, rng.nextInt(128)) }); break;
            }
        }

        auto out = render(p, sr, blockSize, 1.6, events);
        float worst = 0.0f;
        const bool finite = allFinite(out, worst);
        // a silent render would pass the finite check while stressing nothing
        check(finite && worst > 1.0e-3f,
              "output stays live, finite and bounded at " + juce::String((int) sr)
                  + " Hz, block " + juce::String(blockSize)
                  + " (worst " + juce::String(worst, 4) + ")");
    }
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::cout << "Galdr headless tests" << std::endl;

    testStateRoundTrip();
    testMidiLearn();
    testTuningParser();
    testNoteLifecycle();
    testPitchAccuracy();
    testTuningIntegration();
    testMonoPriority();
    testArpTiming(128);
    testArpTiming(1024);
    testDelaySync();
    testFactoryPresets();
    testMonoBus();
    testFiniteness();

    if (failures == 0)
    {
        std::cout << "ALL TESTS PASSED" << std::endl;
        return 0;
    }
    std::cout << failures << " FAILURE(S)" << std::endl;
    return 1;
}
