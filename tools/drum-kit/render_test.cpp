// Desktop tests compile the actual SDK adapter and DSP, without the binary ARM libraries.
#include "../../core/synth/SynthModelMigration.h"
#include "../../core/synth/engines/DrumKitSynthVoice.cpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

using namespace polysynth;
constexpr InstrumentVoice::InstrumentVoiceContext context{ 120.0f };

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// Metadata receives a real Patch object; the DSP never calls these storage methods.
struct TestPatch : Patch
{
    Metadata metadata;
    Sequencer seq;
    optional_paramvalue_t getParamValue(int) const override { return std::nullopt; }
    tiny_optional<float> getParamValueEditMapped(int) const override { return std::nullopt; }
    void setParamValueEditMapped(int, tiny_optional<float>, ParamChangeSource) override {}
    optional_paramvalue_t getParamFinalValue(int) const override { return std::nullopt; }
    void setParamValue(int, optional_paramvalue_t, ParamChangeSource) override {}
    void subscribeParamUpdates(std::weak_ptr<ParamChangeListener>) override {}
    void unsubscribeParamUpdates(const ParamChangeListener&) override {}
    void startBatchUpdate() override {}
    void finishBatchUpdate() override {}
    optional_paramvalue_t getMacroValue(int) const override { return std::nullopt; }
    void setMacroValue(int, optional_paramvalue_t, tiny_optional<int>) override {}
    void setMacroModifier(int, int, tiny_optional<int>) override {}
    void setMacroModifierAmount(int, int, float) override {}
    tiny_optional<float> getMacroModifierAmount(int, int) override { return std::nullopt; }
    void clearMacroModifiers() override {}
    void changeMacroModifierAmount(int, int, paramvalue_t) override {}
    void setMacroBipolar(int, bool) override {}
    void setMacroDisplayName(int, std::string) override {}
    const Sample* getSample(int) const override { return nullptr; }
    LoadStatus loadSample(int, const std::string&) override { return LoadStatus::kFileError; }
    const Metadata& getMetadata() const override { return metadata; }
    const Sequencer& getSequencer() const override { return seq; }
    void setSequence(const Sequencer::Sequence&, int) override {}
    void setSequencerConfig(const Sequencer::Config&) override {}
};

MidiEvent noteEvent(MidiEvent::Type type, int note, int velocity = 100)
{
    MidiEvent e{};
    e.type = type;
    e.data.note.note = note;
    e.data.note.velocity = velocity;
    return e;
}

std::unique_ptr<InstrumentVoice> makeVoice(const Patch::Metadata& metadata)
{
    auto voice = createDrumKitSynth()->createVoice(0);
    for (size_t i = 0; i < metadata.params.size(); ++i)
        voice->onParamChange(i, metadata.params[i].defaultValue);
    return voice;
}

std::vector<float> render(InstrumentVoice& voice, int blocks)
{
    std::vector<float> result;
    result.reserve(blocks * 128);
    std::array<float, 128> left{}, right{};
    for (int b = 0; b < blocks; ++b)
    {
        voice.renderVoiceAudio(
          context,
          std::nullopt,
          AudioBufferStereoRef{ AudioBufferRef{ left }, AudioBufferRef{ right } },
          {});
        for (float x : left)
        {
            require(std::isfinite(x) && std::fabs(x) <= 1.0f, "finite bounded audio");
            result.push_back(x);
        }
    }
    return result;
}

double energy(const std::vector<float>& a)
{
    double sum = 0;
    for (float x : a)
        sum += x * x;
    return sum;
}

void wav(const std::filesystem::path& path, const std::vector<float>& data)
{
    std::ofstream f(path, std::ios::binary);
    auto le = [&f](uint32_t x, int bytes)
    {
        for (int i = 0; i < bytes; ++i)
            f.put(static_cast<char>(x >> (8 * i)));
    };
    f.write("RIFF", 4);
    le(36 + data.size() * 2, 4);
    f.write("WAVEfmt ", 8);
    le(16, 4);
    le(1, 2);
    le(1, 2);
    le(44100, 4);
    le(88200, 4);
    le(2, 2);
    le(16, 2);
    f.write("data", 4);
    le(data.size() * 2, 4);
    for (float x : data)
        le(static_cast<uint16_t>(std::lround(x * 32767.0f)), 2);
    require(f.good(), "write WAV");
}

struct Buses
{
    std::vector<float> left, right, delay, reverb;
};

Buses renderKit(InstrumentVoice& voice, int blocks)
{
    Buses audio;
    std::array<float, 128> l{}, r{}, d{}, v{};
    for (auto* bus : { &audio.left, &audio.right, &audio.delay, &audio.reverb })
        bus->reserve(blocks * 128);
    for (int i = 0; i < blocks; ++i)
    {
        std::array<AudioBufferRef, 2> aux{ AudioBufferRef{ d }, AudioBufferRef{ v } };
        voice.renderVoiceAudio(
          context, std::nullopt, { AudioBufferRef{ l }, AudioBufferRef{ r } }, aux);
        for (auto buf : { l, r, d, v })
            for (float x : buf)
                require(std::isfinite(x) && std::fabs(x) <= 1.0f, "finite bounded KIT buses");
        audio.left.insert(audio.left.end(), l.begin(), l.end());
        audio.right.insert(audio.right.end(), r.begin(), r.end());
        audio.delay.insert(audio.delay.end(), d.begin(), d.end());
        audio.reverb.insert(audio.reverb.end(), v.begin(), v.end());
    }
    return audio;
}

void wavStereo(const std::filesystem::path& file, const Buses& buses)
{
    std::ofstream f(file, std::ios::binary);
    auto le = [&](uint32_t x, int bytes)
    {
        for (int i = 0; i < bytes; ++i)
            f.put(static_cast<char>(x >> (8 * i)));
    };
    const size_t bytes = buses.left.size() * 4;
    f.write("RIFF", 4);
    le(36 + bytes, 4);
    f.write("WAVEfmt ", 8);
    le(16, 4);
    le(1, 2);
    le(2, 2);
    le(44100, 4);
    le(176400, 4);
    le(4, 2);
    le(16, 2);
    f.write("data", 4);
    le(bytes, 4);
    for (size_t i = 0; i < buses.left.size(); ++i)
    {
        le(static_cast<uint16_t>(std::lround(buses.left[i] * 32767)), 2);
        le(static_cast<uint16_t>(std::lround(buses.right[i] * 32767)), 2);
    }
    require(f.good(), "stereo WAV written");
}

int main(int argc, char** argv)
{
    using namespace drumkit;
    using C = Control;
    auto out = std::filesystem::path(argc > 1 ? argv[1] : "build/drum-kit-808");
    std::filesystem::create_directories(out);
    auto patch = std::make_shared<TestPatch>();
    auto type = createDrumKitSynth();
    type->createMetadata(*patch, patch->metadata);
    auto& m = patch->metadata;
    m.initDefaultMacros(m.macros);
    require(m.params.size() == 42 && m.maxVoices == 1, "one SDK voice / six internal drums");
    require(m.layoutFlat.sections.size() == 6 && m.rendersAuxSends &&
              m.outputChannels == Patch::Metadata::OutputChannels::kStereo,
            "six sections, stereo and sends");
    for (size_t i = 0; i < m.layoutFlat.sections.size(); ++i)
    {
        require(m.layoutFlat.sections[i].shortName.size() <= 7, "KIT tile labels fit display");
        for (size_t j = 0; j < i; ++j)
            require(m.layoutFlat.sections[i].shortName != m.layoutFlat.sections[j].shortName,
                    "unique KIT tile labels");
    }
    constexpr std::array names{ "Pitch", "Decay", "Tone", "Drive", "Level", "Pan" };
    for (int i = 0; i < 6; ++i)
    {
        require(m.macros[i].displayName == names[i], "six KIT macro names");
        require(*m.macros[i].modifiers[0].paramId == 36 + i && m.macros[i].bipolar,
                "KIT macro routing");
    }
    for (const auto& p : m.params)
        require(p.defaultValue && *p.defaultValue >= p.minValue && *p.defaultValue <= p.maxValue,
                "valid KIT defaults");
    auto make = [&](std::shared_ptr<TestPatch> p = std::shared_ptr<TestPatch>{})
    {
        if (!p)
            p = patch;
        auto v = makeVoice(m);
        v->setPatch(p);
        return v;
    };
    auto hit = [&](InstrumentVoice& v, int note, int velocity = 100)
    { v.onMidiEvent(context, noteEvent(MidiEvent::Type::kNoteOn, note, velocity)); };
    auto snapshot = [](InstrumentVoice& v, int lane) -> const Settings&
    { return static_cast<DrumKitSynthVoice&>(v).kit.snapshot(lane); };

    require(*InstrumentTypeId::kDrumKitSynth == 10, "KIT patch ID stays stable after removing KFM");
    require(normalizeSynthModelIndex(8) == 7 && normalizeSynthModelIndex(7) == 7,
            "p02 KIT selector index migrates to sole p03 KIT");
    for (int i = 0; i < 7; ++i)
        require(normalizeSynthModelIndex(i) == i, "factory and CHIP selections remain unchanged");

    PreviewLatch latch;
    Buses previewSequence;
    std::array<std::vector<float>, kDrums> previewAudio;
    for (int section = 0; section < kDrums; ++section)
    {
        const auto event = latch.press(1, section);
        require(event && event->part == 1 && event->note == kNotes[section],
                "editor preview chooses selected drum, not fixed C4");
        require(!latch.press(2, (section + 1) % kDrums),
                "held/duplicate preview does not retrigger");
        auto voice = make();
        hit(*voice, event->note);
        auto audio = renderKit(*voice, 250);
        previewAudio[section] = audio.left;
        previewSequence.left.insert(
          previewSequence.left.end(), audio.left.begin(), audio.left.end());
        previewSequence.right.insert(
          previewSequence.right.end(), audio.right.begin(), audio.right.end());
        const auto released = latch.release();
        require(released && released->part == 1 && released->note == kNotes[section],
                "release remembers pressed section/part after navigation");
        require(!latch.release(), "duplicate release is ignored");
    }
    wavStereo(out / "08-editor-preview-bd-sd-ch-cp-oh-cb.wav", previewSequence);
    for (int note = 0; note < 128; ++note)
    {
        auto voice = make();
        hit(*voice, note);
        require(voice->isActive() && energy(renderKit(*voice, 3).left) > 0.000001,
                "all Mini keyboard notes/octaves can audition KIT");
    }

    auto crossings = [](const std::vector<float>& audio)
    {
        int count = 0;
        for (size_t i = 1; i < 4410; ++i)
            if ((audio[i] > 0) != (audio[i - 1] > 0))
                ++count;
        return count;
    };
    const int kickCrossings = crossings(previewAudio[0]);
    require(crossings(previewAudio[1]) > kickCrossings * 3,
            "default snare has noisy snap, not kick spectrum");
    require(crossings(previewAudio[2]) > kickCrossings * 8,
            "default closed hat has high-frequency energy");
    require(crossings(previewAudio[3]) > kickCrossings * 3,
            "default clap has filtered-noise attacks");
    auto windowEnergy = [](const std::vector<float>& audio, size_t from, size_t to)
    {
        double sum = 0;
        for (size_t i = from; i < std::min(to, audio.size()); ++i)
            sum += audio[i] * audio[i];
        return sum;
    };
    require(windowEnergy(previewAudio[4], 4410, 16000) >
              windowEnergy(previewAudio[2], 4410, 16000) * 50,
            "open and closed hats have distinctly different default tails");
    auto spectralPower = [](const std::vector<float>& audio, float hz)
    {
        double real = 0, imag = 0;
        for (size_t i = 0; i < 4096; ++i)
        {
            const double phase = 6.28318530718 * hz * i / 44100.0;
            real += audio[i] * std::cos(phase);
            imag += audio[i] * std::sin(phase);
        }
        return real * real + imag * imag;
    };
    require(spectralPower(previewAudio[5], 540) > spectralPower(previewAudio[5], 55) * 20 &&
              spectralPower(previewAudio[5], 800) > spectralPower(previewAudio[5], 55) * 20,
            "cowbell has two square-oscillator fundamentals, not bass drum");

    // Unsupported notes are silent; each documented note selects one distinct algorithm.
    for (int note : { 128, 255 })
    {
        auto v = make();
        hit(*v, note);
        require(!v->isActive() && energy(renderKit(*v, 2).left) == 0,
                "unsupported notes are silent");
    }
    constexpr std::array files{ "01-kick.wav", "02-snare.wav",    "03-closed-hat.wav",
                                "04-clap.wav", "05-open-hat.wav", "06-cowbell.wav" };
    for (int drum = 0; drum < kDrums; ++drum)
    {
        auto v = make();
        hit(*v, kNotes[drum]);
        auto audio = renderKit(*v, 1800);
        require(energy(audio.left) > .01, "each drum is audible");
        require(audio.left == audio.right, "default pan is centered");
        require(energy(audio.delay) == 0 && energy(audio.reverb) == 0, "default sends are silent");
        require(!v->isActive(), "each drum becomes inactive");
        wavStereo(out / files[drum], audio);
    }

    // A single SDK voice must render the sum of simultaneous independent drums.
    auto all = make();
    std::vector<float> sum(64 * 128);
    for (int drum = 0; drum < kDrums; ++drum)
    {
        auto v = make();
        hit(*v, kNotes[drum]);
        auto audio = renderKit(*v, 64);
        for (size_t i = 0; i < sum.size(); ++i)
            sum[i] += audio.left[i];
        hit(*all, kNotes[drum]); // open hat follows closed hat, so both initial hits can ring
    }
    auto simultaneous = renderKit(*all, 64);
    for (size_t i = 0; i < sum.size(); ++i)
        require(std::fabs(std::clamp(sum[i], -1.0f, 1.0f) - simultaneous.left[i]) < .000002f,
                "simultaneous notes do not steal other lanes");

    // Actual exported sequencer routing endpoint, keyed by assigned Patch.
    auto v = make();
    setDrumKitStepFx(patch.get(), 8, 60, 0, C::kDecay, 1.0f);
    activateDrumKitTrack(patch.get(), 8, 60);
    hit(*v, 60);
    require(std::fabs(snapshot(*v, 0).decay - 3.0f) < .0001f, "kick decay lock applied");
    setDrumKitStepFx(patch.get(), 9, 62, 0, C::kDecay, -1.0f);
    activateDrumKitTrack(patch.get(), 9, 62);
    hit(*v, 62);
    require(std::fabs(snapshot(*v, 1).decay - .04f) < .0001f, "snare has separate decay lock");
    require(std::fabs(snapshot(*v, 0).decay - 3.0f) < .0001f,
            "snare FX leaves kick tail unchanged");
    setDrumKitStepFx(patch.get(), 8, 60, 0, C::kDecay, -1.0f);
    require(std::fabs(snapshot(*v, 0).decay - 3.0f) < .0001f,
            "new lock does not rewrite sounding hit");
    activateDrumKitTrack(patch.get(), 8, 60);
    hit(*v, 60);
    require(std::fabs(snapshot(*v, 0).decay - .075f) < .0001f, "new lock applies to next kick");

    // Same drum from two tracks restores each track's own FX, including delayed hits.
    setDrumKitStepFx(patch.get(), 8, 60, 0, C::kTone, -1.0f);
    setDrumKitStepFx(patch.get(), 10, 60, 0, C::kTone, 1.0f);
    activateDrumKitTrack(patch.get(), 8, 60);
    hit(*v, 60);
    require(snapshot(*v, 0).tone == 0.0f, "track 8 restores own tone lock");
    activateDrumKitTrack(patch.get(), 10, 60);
    hit(*v, 60);
    require(std::fabs(snapshot(*v, 0).tone - 1.0f) < .0001f, "track 10 restores own tone lock");
    setDrumKitStepFx(patch.get(), 10, -1, 1, C::kTone, -1.0f);
    hit(*v, 60);
    require(std::fabs(snapshot(*v, 0).tone - 1.0f) < .0001f, "FX1 takes precedence over FX2");
    setDrumKitStepFx(patch.get(), 10, -1, 0, C::kNone, 0);
    hit(*v, 60);
    require(snapshot(*v, 0).tone == 0.0f,
            "OFF clears only one slot, empty-note row targets prior drum");
    resetDrumKitStepFx(patch.get());
    hit(*v, 60);
    require(snapshot(*v, 0).tone == defaults(0).tone, "reset clears step controls");

    // Controls are isolated between different SYN parts/patches.
    auto otherPatch = std::make_shared<TestPatch>();
    auto other = make(otherPatch);
    setDrumKitStepFx(patch.get(), 8, 60, 0, C::kPitch, 1);
    hit(*v, 60);
    hit(*other, 60);
    require(snapshot(*v, 0).pitch == 24 && snapshot(*other, 0).pitch == 0, "part isolation");
    resetDrumKitStepFx(patch.get());

    // Saved controls affect their own drum only; velocity is monotonic.
    auto baseline = make(), changed = make(otherPatch);
    changed->onParamChange(6 + 1, 3.0f);
    changed->onParamChange(6 + 2, 1.0f);
    hit(*baseline, 60);
    hit(*changed, 60);
    require(renderKit(*baseline, 120).left == renderKit(*changed, 120).left,
            "snare patch edits cannot change kick audio");
    double previous = 0;
    for (int velocity : { 16, 64, 127 })
    {
        auto a = make();
        hit(*a, 62, velocity);
        double e = energy(renderKit(*a, 100).left);
        require(e > previous, "velocity controls drum level");
        previous = e;
    }

    // Panning and independent delay/reverb buses.
    auto pan = make();
    pan->onParamChange(5, -1.0f);
    setDrumKitStepFx(patch.get(), 8, 60, 0, C::kDelay, 1);
    setDrumKitStepFx(patch.get(), 8, 60, 1, C::kReverb, .5f);
    hit(*pan, 60);
    auto buses = renderKit(*pan, 64);
    require(energy(buses.left) > .01 && energy(buses.right) == 0, "per-drum stereo pan");
    require(energy(buses.delay) > .01 && energy(buses.reverb) > .001, "independent aux buses");
    resetDrumKitStepFx(patch.get());
    setDrumKitStepFx(patch.get(), 8, 60, 0, C::kPanAbsolute, 1);
    hit(*pan, 60);
    buses = renderKit(*pan, 32);
    require(energy(buses.left) == 0 && energy(buses.right) > .01, "P FX overrides saved drum pan");
    resetDrumKitStepFx(patch.get());

    // Ordinary note-offs and zero-velocity note-ons do not cut one-shots or retrigger.
    auto one = make(), two = make(otherPatch);
    hit(*one, 62);
    hit(*two, 62);
    renderKit(*one, 5);
    renderKit(*two, 5);
    one->onMidiEvent(context, noteEvent(MidiEvent::Type::kNoteOff, 62));
    one->onMidiEvent(context, noteEvent(MidiEvent::Type::kNoteOn, 62, 0));
    require(renderKit(*one, 100).left == renderKit(*two, 100).left,
            "one-shot MIDI note-off behavior");

    // Closed hats choke open hats, but not kicks; explicit track mute is targeted.
    auto choke = make();
    hit(*choke, 67);
    renderKit(*choke, 10);
    hit(*choke, 64);
    renderKit(*choke, 80);
    require(!choke->isActive(), "closed hat chokes open hat");
    auto mute = make();
    hit(*mute, 67);
    stopDrumKitNote(patch.get(), -1);
    require(mute->isActive(), "empty muted track cannot stop entire kit");
    stopDrumKitNote(patch.get(), 67);
    renderKit(*mute, 80);
    require(!mute->isActive(), "targeted track mute releases drum");
    auto transport = make();
    for (int note : kNotes)
        hit(*transport, note);
    stopDrumKit(patch.get());
    renderKit(*transport, 120);
    require(!transport->isActive() && !transport->handlesAnyNote(), "sequencer Stop endpoint");
    for (int cc : { 120, 123 })
    {
        auto a = make();
        for (int note : kNotes)
            hit(*a, note);
        MidiEvent off{};
        off.type = MidiEvent::Type::kControlChange;
        off.data.controlChange.id = cc;
        a->onMidiEvent(context, off);
        renderKit(*a, 120);
        require(!a->isActive() && !a->handlesAnyNote(), "CC stop ends all drum lanes");
    }

    // Bounds/random settings and rapid six-drum retriggers via the actual SDK adapter.
    std::mt19937 random(20261103);
    for (int test = 0; test < 48; ++test)
    {
        auto a = make();
        for (int i = 0; i < 42; ++i)
        {
            const auto& p = m.params[i];
            float x = test % 3 == 0   ? p.minValue
                      : test % 3 == 1 ? p.maxValue
                                      : p.minValue + (p.maxValue - p.minValue) *
                                                       std::generate_canonical<float, 24>(random);
            a->onParamChange(i, x);
        }
        for (int j = 0; j < 24; ++j)
        {
            hit(*a, kNotes[j % 6]);
            renderKit(*a, 3);
        }
        renderKit(*a, 1900);
        require(!a->isActive(), "extreme kit settings always decay to inactive");
    }

    // Eight bars, all six drums on ONE SDK voice/part, independent per-track locks.
    auto demo = make();
    resetDrumKitStepFx(patch.get());
    Buses song;
    int lastStep = -1;
    for (int block = 0; block < 5513; ++block)
    {
        int step = static_cast<int>(block * 128 / 5512.5);
        if (step != lastStep)
        {
            lastStep = step;
            auto drumHit = [&](int drum, float decay, int velocity)
            {
                int track = 8 + drum, note = kNotes[drum];
                setDrumKitStepFx(patch.get(), track, note, 0, C::kDecay, decay);
                setDrumKitStepFx(patch.get(),
                                 track,
                                 note,
                                 1,
                                 C::kPan,
                                 drum == 2   ? .35f
                                 : drum == 5 ? -.35f
                                             : 0);
                activateDrumKitTrack(patch.get(), track, note);
                hit(*demo, note, velocity);
            };
            if (step % 4 == 0 || step % 32 == 30)
                drumHit(0, (step / 16) % 2 ? .1f : -.1f, 115);
            if (step % 8 == 4)
                drumHit(1, 0, 105);
            if (step % 2 == 0)
                drumHit(2, (step % 8 == 6) ? .4f : 0, step % 4 == 0 ? 55 : 80);
            if (step % 16 == 12)
                drumHit(3, -.1f, 90);
            if (step % 16 == 10)
                drumHit(4, 0, 75);
            if (step % 8 == 3 || step % 8 == 7)
                drumHit(5, (step % 16 == 7) ? .3f : -.3f, 70);
        }
        auto audio = renderKit(*demo, 1);
        song.left.insert(song.left.end(), audio.left.begin(), audio.left.end());
        song.right.insert(song.right.end(), audio.right.begin(), audio.right.end());
    }
    wavStereo(out / "07-kit-one-part-120bpm.wav", song);
    std::cout
      << "PASS: 6 algorithms / 1 SDK voice, simultaneous drum sum, 42 parameters, 6 macros, "
         "track FX isolation and precedence, snapshots, part isolation, stereo/aux, hats choke, "
         "targeted mute, one-shot MIDI, velocity, 48 boundary/random kits, rapid retriggers, "
         "tails.\n";
}
