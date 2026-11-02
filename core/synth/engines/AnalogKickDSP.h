#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace polysynth::analogkick
{
// Independent of the firmware SDK so desktop renders exercise the actual DSP.
struct Parameters
{
    float tune = 0.0f;
    float decay = 0.45f; // seconds to -60 dB
    float sweep = 24.0f; // initial pitch rise, semitones
    float sweepTime = 0.045f;
    float fm = 0.15f;
    float ratio = 2.0f;
    float fmDecay = 0.12f;
    float click = 0.15f;
    float clickTime = 0.006f;
    float drive = 0.1f;
    float volume = 0.8f;
    bool gated = false;
};

class Voice
{
  public:
    static constexpr float kSampleRate = 44100.0f;

    void trigger(int note, float velocity)
    {
        midiNote = std::clamp(note, 0, 127);
        velocityGain = std::clamp(velocity, 0.0f, 1.0f);
        retriggerTail = lastRawOutput;
        retriggerSamples = playing ? 88 : 0; // 2 ms crossfade only on an active voice
        carrierPhase = modPhase = 0.0f;
        ampEnv = pitchEnv = fmEnv = clickEnv = 1.0f;
        attack = 0.0f;
        stopping = false;
        playing = velocityGain > 0.0f;
    }

    void stop() { stopping = true; }
    bool isActive() const { return playing; }

    void render(std::span<float> output, const Parameters& p, float pitchBend = 0.0f)
    {
        if (!playing)
        {
            std::fill(output.begin(), output.end(), 0.0f);
            return;
        }

        const float baseHz =
          std::clamp(52.0f * std::exp2((midiNote - 60.0f + std::clamp(p.tune, -24.0f, 24.0f) +
                                        std::clamp(pitchBend, -2.0f, 2.0f)) /
                                       12.0f),
                     12.0f,
                     12000.0f);
        const float sweepRatio = std::exp2(std::clamp(p.sweep, 0.0f, 48.0f) / 12.0f) - 1.0f;
        const float ampCoeff = decayCoefficient(stopping ? 0.008f : p.decay, 0.008f, 3.0f);
        const float pitchCoeff = decayCoefficient(p.sweepTime, 0.003f, 0.5f);
        const float fmCoeff = decayCoefficient(p.fmDecay, 0.005f, 2.0f);
        const float clickCoeff = decayCoefficient(p.clickTime, 0.001f, 0.04f);
        const float ratio = std::clamp(p.ratio, 0.25f, 12.0f);
        const float fmTarget = std::clamp(p.fm, 0.0f, 1.0f);
        const float clickTarget = std::clamp(p.click, 0.0f, 1.0f);
        const float driveTarget = std::clamp(p.drive, 0.0f, 1.0f);
        const float gainTarget = std::clamp(p.volume, 0.0f, 1.0f);

        if (newVoice)
        {
            smoothedHz = baseHz;
            smoothedFm = fmTarget;
            smoothedClick = clickTarget;
            smoothedDrive = driveTarget;
            smoothedGain = gainTarget;
            newVoice = false;
        }

        for (auto& sample : output)
        {
            // 1 ms smoothing of the main live controls; envelope rates are block-rate.
            smoothedHz += 0.0224f * (baseHz - smoothedHz);
            smoothedFm += 0.0224f * (fmTarget - smoothedFm);
            smoothedClick += 0.0224f * (clickTarget - smoothedClick);
            smoothedDrive += 0.0224f * (driveTarget - smoothedDrive);
            smoothedGain += 0.0224f * (gainTarget - smoothedGain);

            const float hz = std::min(16000.0f, smoothedHz * (1.0f + sweepRatio * pitchEnv));
            // Reduce modulation index in the upper register. This limits, but does
            // not eliminate, aliasing; no claim of analog circuit emulation is made.
            const float indexLimit = std::max(0.0f, (18000.0f / hz - 1.0f) / ratio * 0.5f);
            const float index = std::min(8.0f * smoothedFm * fmEnv, indexLimit);
            const float body =
              std::sin(carrierPhase * kTwoPi + index * std::sin(modPhase * kTwoPi));
            carrierPhase += hz / kSampleRate;
            modPhase += std::min(18000.0f, hz * ratio) / kSampleRate;
            carrierPhase -= std::floor(carrierPhase);
            modPhase -= std::floor(modPhase);

            noise ^= noise << 13;
            noise ^= noise >> 17;
            noise ^= noise << 5;
            const float white = static_cast<float>(noise >> 8) / 8388607.5f - 1.0f;
            noiseLow += 0.12f * (white - noiseLow);
            const float transient = (white - noiseLow) * smoothedClick * clickEnv * 0.55f;
            attack = std::min(1.0f, attack + 1.0f / 22.0f); // 0.5 ms
            float signal = (body * 0.8f + transient) * ampEnv * attack * velocityGain;

            const float driven = signal * (1.0f + 12.0f * smoothedDrive);
            const float clipped = driven / (1.0f + std::fabs(driven));
            signal = (signal * (1.0f - smoothedDrive) + clipped * smoothedDrive) * smoothedGain;
            if (retriggerSamples > 0)
            {
                const float mix = static_cast<float>(retriggerSamples--) / 88.0f;
                signal = retriggerTail * mix + signal * (1.0f - mix);
            }
            lastRawOutput = signal;
            // 10 Hz DC blocker after the nonlinear stage, also flushed in the tail.
            const float filtered = signal - dcInput + 0.998576f * dcOutput;
            dcInput = signal;
            dcOutput = filtered;
            sample = std::clamp(filtered, -1.0f, 1.0f);

            ampEnv *= ampCoeff;
            pitchEnv *= pitchCoeff;
            fmEnv *= fmCoeff;
            clickEnv *= clickCoeff;
            if (ampEnv < 0.00001f && std::fabs(dcOutput) < 0.00001f)
            {
                playing = false;
                ampEnv = lastRawOutput = dcInput = dcOutput = 0.0f;
            }
            if (!playing)
            {
                sample = 0.0f;
            }
        }
    }

  private:
    static constexpr float kTwoPi = 6.28318530718f;
    static float decayCoefficient(float seconds, float minimum, float maximum)
    {
        return std::exp(-6.907755279f / (std::clamp(seconds, minimum, maximum) * kSampleRate));
    }

    int midiNote = 60;
    bool playing = false;
    bool stopping = false;
    bool newVoice = true;
    float velocityGain = 1.0f;
    float carrierPhase = 0.0f, modPhase = 0.0f;
    float ampEnv = 0.0f, pitchEnv = 0.0f, fmEnv = 0.0f, clickEnv = 0.0f;
    float attack = 0.0f;
    float smoothedHz = 52.0f, smoothedFm = 0.15f, smoothedClick = 0.15f;
    float smoothedDrive = 0.1f, smoothedGain = 0.8f;
    uint32_t noise = 0x4b464d31u;
    float noiseLow = 0.0f;
    float lastRawOutput = 0.0f, retriggerTail = 0.0f;
    int retriggerSamples = 0;
    float dcInput = 0.0f, dcOutput = 0.0f;
};
} // namespace polysynth::analogkick
