#pragma once

#include <array>
#include <optional>

namespace polysynth::drumkit
{
constexpr int kDrums = 6;
constexpr std::array<int, kDrums> kNotes{ 60, 62, 64, 65, 67, 69 };
constexpr std::array<const char*, kDrums> kNames{ "Bass Drum", "Snare",    "Closed Hat",
                                                  "Hand Clap", "Open Hat", "Cowbell" };

inline int drumForNote(int note)
{
    if (note < 0 || note > 127)
        return -1;
    // Repeat the six-drum keyboard in every octave. Adjacent black keys are
    // aliases, so the Mini's octave/scale setting cannot make the kit silent.
    constexpr std::array<int, 12> mapping{ 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 5 };
    return mapping[note % 12];
}

struct PreviewEvent
{
    int part;
    int note;
};

class PreviewLatch
{
  public:
    std::optional<PreviewEvent> press(int part, int section)
    {
        if (held || part < 0 || part >= 3)
            return std::nullopt;
        held = PreviewEvent{ part, kNotes[section >= 0 && section < kDrums ? section : 0] };
        return held;
    }
    std::optional<PreviewEvent> release()
    {
        auto previous = held;
        held.reset();
        return previous;
    }

  private:
    std::optional<PreviewEvent> held;
};
} // namespace polysynth::drumkit
