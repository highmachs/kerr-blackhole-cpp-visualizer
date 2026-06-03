// Procedural audio for spec §14 events.
//   - Outer horizon crossing  : low rumble + Doppler-shifted white noise
//   - Inner horizon crossing  : high-pitched upward sweep (with whiteout)
//   - Ring singularity        : reverse cymbal (noise w/ rising envelope)
//   - Negative universe       : distorted reversed ambience
//
// Uses miniaudio (single-header).  All synthesis is generated live in the
// audio callback — no asset files needed.

#pragma once
namespace kerr {

enum class AudioEvent {
    OUTER_HORIZON  = 0,
    INNER_HORIZON  = 1,
    RING_SINGULAR  = 2,
    NEGATIVE_AMBI  = 3,
};

class Audio {
public:
    bool init();
    void shutdown();
    void trigger(AudioEvent e);
    bool ok() const { return ok_; }
private:
    bool ok_ = false;
};

} // namespace kerr
