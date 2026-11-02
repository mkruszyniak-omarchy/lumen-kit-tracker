#include "config.h"
#ifdef FEATURE_SYNTH
#include "DrumKitSynthVoice.h"
#include "PolySynth/sdk/SynthVoiceSDK.h"

namespace polysynth
{
namespace
{
// 6 drums x 6 saved controls, then 6 global performance macro destinations.
enum class KitParam : int
{
    kFirst = 0,
    kLast = 41
};
class DrumKitSynthVoice;
std::array<DrumKitSynthVoice*, 24> activeKits{};

class DrumKitSynthVoice : public sdk::CustomSynthVoice<DrumKitSynthVoice, KitParam>
{
  public:
    DrumKitSynthVoice()
    {
        for (auto& slot : activeKits)
            if (!slot)
            {
                slot = this;
                return;
            }
        assert(false && "Too many active KIT engine instances");
    }
    ~DrumKitSynthVoice() final
    {
        for (auto& slot : activeKits)
            if (slot == this)
                slot = nullptr;
    }
    drumkit::Kit kit;
    bool isActive() final { return kit.isActive(); }
    void stopAll()
    {
        kit.stop();
        kit.router.reset();
        heldNotes.clear();
    }

    void onMidiEvent(const InstrumentVoiceContext&, const MidiEvent& event) final
    {
        if (event.type == MidiEvent::Type::kStop ||
            (event.type == MidiEvent::Type::kControlChange &&
             (event.data.controlChange.id == 120 || event.data.controlChange.id == 123)))
        {
            stopAll();
            return;
        }
        if (event.type == MidiEvent::Type::kNoteOn && event.data.note.velocity > 0)
        {
            if (drumkit::drumForNote(event.data.note.note) < 0)
                return;
            // One SDK voice contains the entire kit; subsequent notes never steal
            // the other drum lanes. One-shot drums ignore allocator note-offs.
            lastNote = event.data.note.note;
            currentVelocity = event.data.note.velocity / 127.0f;
            heldNotes.clear();
            heldNotes.push_back({ *lastNote, currentVelocity });
            kit.trigger(*lastNote, currentVelocity);
        }
        else if (event.type == MidiEvent::Type::kNoteOff ||
                 (event.type == MidiEvent::Type::kNoteOn && event.data.note.velocity == 0))
        {
            if (!heldNotes.empty() && heldNotes.back().note == event.data.note.note)
                heldNotes.clear();
        }
    }
    void onNoteOn(const InstrumentVoiceContext&, note_t, float, bool) final {}
    void onNoteOff(const InstrumentVoiceContext&, note_t) final {}

    void renderVoiceAudio(const InstrumentVoiceContext&,
                          std::optional<AudioBufferStereoRef>,
                          AudioBufferStereoRef output,
                          std::span<AudioBufferRef> aux) final
    {
        kit.render(output[0],
                   output[1],
                   aux.size() > 0 ? std::span<float>(aux[0]) : std::span<float>{},
                   aux.size() > 1 ? std::span<float>(aux[1]) : std::span<float>{});
    }

    static void createMetadata(Patch&, Patch::Metadata& metadata)
    {
        using namespace sdk;
        metadata.params.resize(42);
        paramHandlers.resize(42);
        metadata.layoutFlat.sections.clear();
        constexpr std::array<const char*, 6> labels{ "Pitch", "Decay", "Tone",
                                                     "Drive", "Level", "Pan" };
        constexpr std::array<const char*, 6> sections{ "808 Kick C4",     "808 Snare D4",
                                                       "808 Hihat CH E4", "808 Clap Perc F4",
                                                       "808 Hihat OH G4", "808 Cowbell Perc A4" };
        constexpr std::array<const char*, 6> shortNames{ "C4 BD", "D4 SD", "E4 CH",
                                                         "F4 CP", "G4 OH", "A4 CB" };
        for (int drum = 0; drum < drumkit::kDrums; ++drum)
        {
            const auto d = drumkit::defaults(drum);
            const std::array<float, 6> values{ d.pitch, d.decay, d.tone, d.drive, d.level, d.pan };
            EditorSection section{ .name = sections[drum], .shortName = shortNames[drum] };
            for (int control = 0; control < 6; ++control)
            {
                const int id = drum * 6 + control;
                const char* label = labels[control];
                if (control == 2)
                    label = drum == 1                  ? "Snappy"
                            : drum == 5                ? "Balance"
                            : (drum == 2 || drum == 4) ? "Metal"
                                                       : "Tone";
                Param p{ .displayName = label,
                         .defaultValue = values[control],
                         .minValue = 0,
                         .maxValue = 1 };
                if (control == 0)
                {
                    p.minValue = -24;
                    p.maxValue = 24;
                    p.valueFormatter = createSemitoneFormatter();
                }
                else if (control == 1)
                {
                    p.minValue = .015f;
                    p.maxValue = 3;
                    p.editCurve = Param::Curve::kExponential;
                    p.valueFormatter = createTimeFormatter();
                }
                else
                {
                    if (control == 5)
                        p.minValue = -1;
                    p.valueFormatter = createPercentFormatter();
                }
                // ParamHelper requires stateless handlers. Distinct instantiations
                // carry their drum/control indices as compile-time constants.
                metadata.params[id] = std::move(p);
                section.elements.push_back({ .type = EditorElement::Type::kParam, .id = id });
            }
            metadata.layoutFlat.sections.push_back(std::move(section));
        }
        registerDrumHandlers(std::make_index_sequence<36>{});
        for (int i = 0; i < 6; ++i)
        {
            metadata.params[36 + i] = Param{
                .displayName = labels[i], .defaultValue = 0.0f, .minValue = -1, .maxValue = 1
            };
        }
        registerPerformanceHandlers(std::make_index_sequence<6>{});
        metadata.layout23 = metadata.layoutFlat;
        metadata.initDefaultMacros = [](auto& macros)
        {
            constexpr std::array names{ "Pitch", "Decay", "Tone", "Drive", "Level", "Pan" };
            for (int i = 0; i < 6; ++i)
                macros[i] = Patch::Metadata::Macro{ .displayName = names[i],
                                                    .modifiers = { { 36 + i, 1.0f } },
                                                    .bipolar = true };
            macros[*Patch::SystemMacroId::kVelocity].modifiers.clear();
        };
        metadata.instrumentTypeId = *InstrumentTypeId::kDrumKitSynth;
        metadata.outputChannels = Patch::Metadata::OutputChannels::kStereo;
        metadata.rendersAuxSends = true;
        metadata.maxVoices = 1; // Six independent internal lanes, one SDK voice.
    }

  private:
    template<size_t I>
    static void setDrumParam(DrumKitSynthVoice& v, Patch::optional_paramvalue_t x)
    {
        auto& s = v.kit.settings[I / 6];
        if constexpr (I % 6 == 0)
            s.pitch = std::clamp(*x, -24.0f, 24.0f);
        if constexpr (I % 6 == 1)
            s.decay = std::clamp(*x, .015f, 3.0f);
        if constexpr (I % 6 == 2)
            s.tone = std::clamp(*x, 0.0f, 1.0f);
        if constexpr (I % 6 == 3)
            s.drive = std::clamp(*x, 0.0f, 1.0f);
        if constexpr (I % 6 == 4)
            s.level = std::clamp(*x, 0.0f, 1.0f);
        if constexpr (I % 6 == 5)
            s.pan = std::clamp(*x, -1.0f, 1.0f);
    }
    template<size_t... I>
    static void registerDrumHandlers(std::index_sequence<I...>)
    {
        ((paramHandlers[I] = &setDrumParam<I>), ...);
    }
    template<size_t I>
    static void setPerformance(DrumKitSynthVoice& v, Patch::optional_paramvalue_t x)
    {
        v.kit.performance[I] = std::clamp(*x, -1.0f, 1.0f);
    }
    template<size_t... I>
    static void registerPerformanceHandlers(std::index_sequence<I...>)
    {
        ((paramHandlers[36 + I] = &setPerformance<I>), ...);
    }
};
} // namespace

void setDrumKitStepFx(Patch* patch,
                      int track,
                      int note,
                      int slot,
                      drumkit::Control control,
                      float value)
{
    for (auto* voice : activeKits)
        if (voice && voice->getPatch() == patch)
            voice->kit.router.set(track, note, slot, control, value);
}
void resetDrumKitStepFx(Patch* patch)
{
    for (auto* voice : activeKits)
        if (voice && voice->getPatch() == patch)
            voice->kit.router.reset();
}
void activateDrumKitTrack(Patch* patch, int track, int note)
{
    for (auto* voice : activeKits)
        if (voice && voice->getPatch() == patch)
            voice->kit.router.activate(track, note);
}
void stopDrumKitNote(Patch* patch, int note)
{
    if (note < 0)
        return;
    for (auto* voice : activeKits)
        if (voice && voice->getPatch() == patch)
            voice->kit.stop(note);
}
void stopDrumKit(Patch* patch)
{
    for (auto* voice : activeKits)
        if (voice && voice->getPatch() == patch)
            voice->stopAll();
}
std::unique_ptr<InstrumentType> createDrumKitSynth()
{
    return std::make_unique<InstrumentType>(
      InstrumentType{ .name = "KIT",
                      .createMetadata = &DrumKitSynthVoice::createMetadata,
                      .createVoice = [](int) -> std::unique_ptr<InstrumentVoice>
                      { return std::make_unique<DrumKitSynthVoice>(); } });
}
} // namespace polysynth
#endif
