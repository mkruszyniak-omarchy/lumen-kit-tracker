#pragma once

#include "AnalogKickDSP.h"
#include "DrumKitNotes.h"
#include <array>
#include <optional>

namespace polysynth::drumkit
{
enum class Control : int
{
    kNone = -1,
    kPitch,
    kDecay,
    kTone,
    kDrive,
    kLevel,
    kPan,
    kPanAbsolute,
    kDelay,
    kReverb,
    kCount
};
using Modifiers = std::array<std::optional<float>, static_cast<int>(Control::kCount)>;

// Track-local FX slots, independent of the firmware's part-wide synth FX slots.
// Only upcoming hits read this state. Already sounding drums retain their snapshot.
class StepRouter
{
  public:
    static constexpr int kTracks = 16;

    void activate(int track, int note)
    {
        if (track < 0 || track >= kTracks)
            return;
        auto& state = tracks[track];
        if (note >= 0)
        {
            const int drum = drumForNote(note);
            if (state.drum != drum)
            {
                if (state.drum >= 0 && owners[state.drum] == track)
                {
                    pending[state.drum] = {};
                    owners[state.drum] = -1;
                }
                state = Track{};
                state.drum = drum;
            }
        }
        if (state.drum < 0)
            return;
        auto& mods = pending[state.drum];
        mods = {};
        // FX1 (slot 0) takes precedence over FX2, as in the original sequencer.
        for (int i : { 1, 0 })
        {
            const auto& fx = state.slots[i];
            if (fx.control >= Control::kPitch && fx.control < Control::kCount)
                mods[static_cast<int>(fx.control)] = fx.value;
        }
        owners[state.drum] = track;
    }

    void set(int track, int note, int slot, Control control, float value)
    {
        if (track < 0 || track >= kTracks || slot < 0 || slot > 1)
            return;
        activate(track, note);
        tracks[track].slots[slot] = { control, value };
        activate(track, -1);
    }

    const Modifiers& get(int drum) const { return pending[drum]; }
    void reset() { *this = StepRouter{}; }

  private:
    struct Fx
    {
        Control control = Control::kNone;
        float value = 0.0f;
    };
    struct Track
    {
        int drum = -1;
        std::array<Fx, 2> slots{};
    };
    std::array<Track, kTracks> tracks{};
    std::array<Modifiers, kDrums> pending{};
    std::array<int, kDrums> owners{ -1, -1, -1, -1, -1, -1 };
};

struct Settings
{
    float pitch = 0.0f;
    float decay = 0.3f;
    float tone = 0.5f;
    float drive = 0.0f;
    float level = 0.7f;
    float pan = 0.0f;
    float delay = 0.0f;
    float reverb = 0.0f;
};

inline Settings defaults(int drum)
{
    Settings s;
    // Our starting values for the 808-inspired DSP, not a claim of factory knob values.
    constexpr std::array<float, kDrums> decay{ .60f, .32f, .10f, .32f, .80f, .22f };
    constexpr std::array<float, kDrums> tone{ .50f, .72f, .70f, .50f, .65f, .50f };
    constexpr std::array<float, kDrums> level{ .90f, .75f, .55f, .70f, .50f, .55f };
    s.decay = decay[drum];
    s.tone = tone[drum];
    s.level = level[drum];
    return s;
}

inline Settings applyModifiers(Settings s,
                               const std::array<float, 6>& performance,
                               const Modifiers& mods)
{
    std::array<float, 6> x{};
    for (int i = 0; i < 6; ++i)
        x[i] = std::clamp(performance[i] + mods[i].value_or(0.0f), -1.0f, 1.0f);
    s.pitch = std::clamp(s.pitch + 24.0f * x[0], -24.0f, 24.0f);
    s.decay = std::clamp(s.decay * std::exp2(3.0f * x[1]), .015f, 3.0f);
    s.tone = std::clamp(s.tone + .5f * x[2], 0.0f, 1.0f);
    s.drive = std::clamp(s.drive + .5f * x[3], 0.0f, 1.0f);
    s.level = std::clamp(s.level * (1.0f + x[4]), 0.0f, 1.0f);
    s.pan = std::clamp(s.pan + x[5], -1.0f, 1.0f);
    if (mods[static_cast<int>(Control::kPanAbsolute)])
        s.pan = std::clamp(*mods[static_cast<int>(Control::kPanAbsolute)], -1.0f, 1.0f);
    if (mods[static_cast<int>(Control::kDelay)])
        s.delay = std::clamp(*mods[static_cast<int>(Control::kDelay)], 0.0f, 1.0f);
    if (mods[static_cast<int>(Control::kReverb)])
        s.reverb = std::clamp(*mods[static_cast<int>(Control::kReverb)], 0.0f, 1.0f);
    return s;
}

class BandPass
{
  public:
    void configure(float hz, float q)
    {
        const float w = 6.2831853f * std::clamp(hz, 80.0f, 18000.0f) / 44100.0f;
        const float alpha = std::sin(w) / (2.0f * q);
        const float reciprocal = 1.0f / (1.0f + alpha);
        b0 = alpha * reciprocal;
        b2 = -b0;
        a1 = -2.0f * std::cos(w) * reciprocal;
        a2 = (1.0f - alpha) * reciprocal;
    }
    void reset() { z1 = z2 = 0; }
    float tick(float x)
    {
        const float y = b0 * x + z1;
        z1 = -a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

  private:
    float b0 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

class Drum
{
  public:
    void trigger(int model, Settings settings, float velocity)
    {
        sound = model;
        hit = settings;
        gain = std::clamp(velocity, 0.0f, 1.0f);
        fadeTail = last;
        fadeSamples = active ? 44 : 0;
        age = 0;
        envelope = pulseEnv = bodyEnv = releaseGain = 1.0f;
        tailEnv = 0.0f;
        filter.reset();
        phases = {};
        attack = 0;
        releasing = false;
        active = gain > 0;
        if (sound == 0)
        {
            kickParams = analogkick::Parameters{};
            kickParams.tune = hit.pitch + 1.0f; // ~55 Hz at Pitch 0
            kickParams.decay = hit.decay;
            kickParams.volume = 1.0f;
            kickParams.drive = hit.drive;
            kickParams.click = .07f + hit.tone * .08f;
            kickParams.sweep = 10.0f + 4.0f * hit.tone;
            kickParams.sweepTime = .009f;
            kickParams.fm = 0.0f;
            kick.trigger(60, gain);
        }
    }

    void stop()
    {
        releasing = true;
        kick.stop();
    }
    bool isActive() const { return active; }
    const Settings& snapshot() const { return hit; }

    void render(std::span<float> out)
    {
        if (!active)
        {
            std::fill(out.begin(), out.end(), 0.0f);
            return;
        }
        if (sound == 0)
        {
            kick.render(out, kickParams);
            active = kick.isActive();
            return;
        }
        const float sr = analogkick::Voice::kSampleRate;
        const float tune = std::exp2(std::clamp(hit.pitch, -24.0f, 24.0f) / 12.0f);
        const float envCoeff = std::exp(-6.9077553f / (sr * hit.decay));
        const float releaseCoeff = std::exp(-6.9077553f / (sr * .008f));
        const float bodyCoeff =
          std::exp(-6.9077553f / (sr * (sound == 1 ? std::min(.13f, hit.decay * .6f) : .012f)));
        const float center = sound == 1   ? 1800.0f + 4200.0f * hit.tone
                             : sound == 3 ? (1400.0f + 1800.0f * hit.tone) * tune
                             : sound == 5 ? 1000.0f * tune
                                          : 6000.0f + 5000.0f * hit.tone;
        filter.configure(center, sound == 5 ? .75f : sound == 1 ? .7f : 1.0f);
        // Chosen 808-style tuning, not a circuit-identical emulation.
        constexpr std::array<float, 6> metalHz{ 205.0f, 304.0f, 369.0f, 522.0f, 540.0f, 800.0f };
        const float driveGain = 1.0f + 10.0f * hit.drive;
        for (float& sample : out)
        {
            noise ^= noise << 13;
            noise ^= noise >> 17;
            noise ^= noise << 5;
            const float white = static_cast<float>(noise >> 8) / 8388607.5f - 1.0f;
            float source = white;
            if (sound == 2 || sound == 4)
            {
                float metal = 0.0f;
                for (int osc = 0; osc < 6; ++osc)
                {
                    phases[osc] += metalHz[osc] * tune / sr;
                    phases[osc] -= std::floor(phases[osc]);
                    metal += phases[osc] < .5f ? 1.0f : -1.0f;
                }
                source = .18f * white + .82f * metal / 6.0f;
            }
            else if (sound == 5)
            {
                phases[0] += 540.0f * tune / sr;
                phases[0] -= std::floor(phases[0]);
                phases[1] += 800.0f * tune / sr;
                phases[1] -= std::floor(phases[1]);
                const float lowSquare = phases[0] < .5f ? 1.0f : -1.0f;
                const float highSquare = phases[1] < .5f ? 1.0f : -1.0f;
                source = lowSquare * (1.0f - hit.tone) + highSquare * hit.tone;
            }
            const float filteredNoise = filter.tick(source);
            float signal = filteredNoise;
            if (sound == 1)
            {
                phases[0] += 180.0f * tune / sr;
                phases[0] -= std::floor(phases[0]);
                phases[1] += 330.0f * tune / sr;
                phases[1] -= std::floor(phases[1]);
                const float body =
                  .65f * std::sin(6.2831853f * phases[0]) + .35f * std::sin(6.2831853f * phases[1]);
                signal = .55f * body * bodyEnv + filteredNoise * envelope * (.5f + 1.8f * hit.tone);
            }
            else if (sound == 3)
            {
                // Three filtered-noise attacks 10 ms apart, with a separate diffuse tail.
                if (age == 441 || age == 882)
                    pulseEnv = 1.0f;
                if (age == 882)
                    tailEnv = .45f;
                const float burst = age < 1323 ? pulseEnv : 0.0f;
                signal *= (burst + tailEnv) * 1.7f;
                pulseEnv *= .986f;
                tailEnv *= envCoeff;
            }
            else if (sound == 5)
                signal *= envelope * (.35f + .65f * bodyEnv);
            else
                signal *= envelope * 2.5f;
            attack = std::min(1.0f, attack + 1.0f / 15.0f);
            if (releasing)
                releaseGain *= releaseCoeff;
            signal *= attack * gain * releaseGain;
            const float driven = signal * driveGain;
            signal = signal * (1.0f - hit.drive) + driven / (1.0f + std::fabs(driven)) * hit.drive;
            if (fadeSamples > 0)
            {
                const float mix = static_cast<float>(fadeSamples--) / 44.0f;
                signal = fadeTail * mix + signal * (1.0f - mix);
            }
            last = signal;
            const float dc = signal - dcInput + .998576f * dcOutput;
            dcInput = signal;
            dcOutput = dc;
            sample = dc;
            envelope *= envCoeff;
            bodyEnv *= bodyCoeff;
            ++age;
            const float remaining = sound == 3 ? std::max(envelope, tailEnv) : envelope;
            if (remaining * releaseGain < .00001f && std::fabs(dcOutput) < .00001f)
            {
                active = false;
                envelope = dcInput = dcOutput = last = 0;
            }
        }
    }

  private:
    Settings hit;
    analogkick::Voice kick;
    analogkick::Parameters kickParams;
    int sound = 0;
    bool active = false, releasing = false;
    std::array<float, 6> phases{};
    uint32_t noise = 0x4b495431u, age = 0;
    float gain = 0, envelope = 0, pulseEnv = 0, attack = 0;
    BandPass filter;
    float bodyEnv = 0, tailEnv = 0, releaseGain = 1;
    float dcInput = 0, dcOutput = 0;
    float last = 0, fadeTail = 0;
    int fadeSamples = 0;
};

class Kit
{
  public:
    Kit()
    {
        for (int i = 0; i < kDrums; ++i)
            settings[i] = defaults(i);
    }
    std::array<Settings, kDrums> settings{};
    std::array<float, 6> performance{};
    StepRouter router;

    void trigger(int note, float velocity)
    {
        const int drum = drumForNote(note);
        if (drum < 0 || velocity <= 0)
            return;
        // Closed hat chokes the open hat; the other lanes remain untouched.
        if (drum == 2)
            drums[4].stop();
        drums[drum].trigger(
          drum, applyModifiers(settings[drum], performance, router.get(drum)), velocity);
    }
    void stop(int note = -1)
    {
        const int drum = drumForNote(note);
        for (int i = 0; i < kDrums; ++i)
            if (note < 0 || drum == i)
                drums[i].stop();
    }
    bool isActive() const
    {
        for (const auto& drum : drums)
            if (drum.isActive())
                return true;
        return false;
    }
    const Settings& snapshot(int drum) const { return drums[drum].snapshot(); }

    void render(std::span<float> left,
                std::span<float> right,
                std::span<float> delay,
                std::span<float> reverb)
    {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        std::fill(delay.begin(), delay.end(), 0.0f);
        std::fill(reverb.begin(), reverb.end(), 0.0f);
        // Firmware callbacks are 128 samples; no dynamic allocation on the audio thread.
        std::array<float, 128> temp{};
        for (int drum = 0; drum < kDrums; ++drum)
        {
            if (!drums[drum].isActive())
                continue;
            drums[drum].render(std::span<float>(temp).first(left.size()));
            const auto& hit = drums[drum].snapshot();
            const float gain = hit.level * .4f;
            const float gainL = gain * std::min(1.0f, 1.0f - hit.pan);
            const float gainR = gain * std::min(1.0f, 1.0f + hit.pan);
            for (size_t i = 0; i < left.size(); ++i)
            {
                left[i] += temp[i] * gainL;
                right[i] += temp[i] * gainR;
                if (i < delay.size())
                    delay[i] += temp[i] * gain * hit.delay;
                if (i < reverb.size())
                    reverb[i] += temp[i] * gain * hit.reverb;
            }
        }
        for (auto bus : { left, right, delay, reverb })
            for (float& x : bus)
                x = std::clamp(x, -1.0f, 1.0f);
    }

  private:
    std::array<Drum, kDrums> drums;
};
} // namespace polysynth::drumkit
