#pragma once

#include "DrumKitDSP.h"
#include "PolySynth/sdk/InstrumentType.h"

namespace polysynth
{
std::unique_ptr<InstrumentType> createDrumKitSynth();
// Called with audio interrupts disabled; matches instances by their assigned Patch.
void setDrumKitStepFx(Patch* patch,
                      int track,
                      int note,
                      int slot,
                      drumkit::Control control,
                      float value);
void resetDrumKitStepFx(Patch* patch);
void activateDrumKitTrack(Patch* patch, int track, int note);
void stopDrumKitNote(Patch* patch, int note);
void stopDrumKit(Patch* patch);
}
