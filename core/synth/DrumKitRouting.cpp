#include "config.h"
#ifdef FEATURE_SYNTH
#include "Synth.h"
#include "engines/DrumKitSynthVoice.h"
#include "interruptsControl.h"
#include "mtSequencer.h"

bool Synth::isDrumKit(int part) const
{
    return part >= 0 && part < kSynthParts &&
           parts[part].instrumentTypeId == polysynth::InstrumentTypeId::kDrumKitSynth;
}

bool Synth::seqDrumKitFx(int part, int track, int note, int fxId, int value, int slot)
{
    if (!isDrumKit(part))
        return false;
    using C = polysynth::drumkit::Control;
    using F = Sequencer::strFxConsts;
    C control = C::kNone;
    switch (fxId)
    {
        case F::FX_TYPE_OFF: break;
        case F::FX_TYPE_SEND_CC_A: control = C::kPitch; break;
        case F::FX_TYPE_SEND_CC_B: control = C::kDecay; break;
        case F::FX_TYPE_SEND_CC_C: control = C::kTone; break;
        case F::FX_TYPE_SEND_CC_D: control = C::kDrive; break;
        case F::FX_TYPE_SEND_CC_E: control = C::kLevel; break;
        case F::FX_TYPE_SEND_CC_F: control = C::kPan; break;
        case F::FX_TYPE_PANNING: control = C::kPanAbsolute; break;
        case F::FX_TYPE_DELAY_SEND: control = C::kDelay; break;
        case F::FX_TYPE_REVERB_SEND: control = C::kReverb; break;
        default: return false;
    }
    float normalized = 0.0f;
    if (control >= C::kPitch && control <= C::kPan)
    {
        const float v = std::clamp(value, 0, 127);
        normalized = v <= 63 ? (v - 63.0f) / 63.0f : (v - 63.0f) / 64.0f;
    }
    else if (control == C::kPanAbsolute)
        normalized = std::clamp(value / 50.0f - 1.0f, -1.0f, 1.0f);
    else if (control == C::kDelay || control == C::kReverb)
        normalized = std::clamp(value / 100.0f, 0.0f, 1.0f);
    DISABLE_AUDIO_INTERRUPTS();
    polysynth::setDrumKitStepFx(parts[part].patch.get(), track, note, slot, control, normalized);
    ENABLE_AUDIO_INTERRUPTS();
    return true;
}

void Synth::activateDrumKitTrack(int part, int track, int note)
{
    if (!isDrumKit(part))
        return;
    DISABLE_AUDIO_INTERRUPTS();
    polysynth::activateDrumKitTrack(parts[part].patch.get(), track, note);
    ENABLE_AUDIO_INTERRUPTS();
}

void Synth::resetDrumKitFx(int part)
{
    if (!isDrumKit(part))
        return;
    DISABLE_AUDIO_INTERRUPTS();
    polysynth::resetDrumKitStepFx(parts[part].patch.get());
    ENABLE_AUDIO_INTERRUPTS();
}

void Synth::stopDrumKitNote(int part, int note)
{
    if (!isDrumKit(part) || note < 0)
        return;
    DISABLE_AUDIO_INTERRUPTS();
    polysynth::stopDrumKitNote(parts[part].patch.get(), note);
    ENABLE_AUDIO_INTERRUPTS();
}

void Synth::stopDrumKits()
{
    DISABLE_AUDIO_INTERRUPTS();
    for (int part = 0; part < kSynthParts; ++part)
        if (isDrumKit(part))
            polysynth::stopDrumKit(parts[part].patch.get());
    ENABLE_AUDIO_INTERRUPTS();
}
#endif
