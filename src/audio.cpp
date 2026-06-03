#include "audio.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <atomic>
#include <cmath>
#include <cstdio>

namespace kerr {

namespace {

struct Voice {
    std::atomic<bool>  active{false};
    std::atomic<int>   kind{0};      // AudioEvent
    std::atomic<float> phase{0.0f};  // seconds since trigger
};

constexpr int kNumVoices = 4;
Voice g_voices[kNumVoices];
ma_device g_device{};
bool g_inited = false;
unsigned int g_seed = 0xACE1u;

inline float frand() {
    g_seed = g_seed * 1664525u + 1013904223u;
    return (float)((g_seed >> 8) & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
}

float sample(int kind, float t) {
    switch ((AudioEvent)kind) {
    case AudioEvent::OUTER_HORIZON: {
        if (t > 3.5f) return 0.0f;
        float env = std::exp(-t * 0.6f);
        float rumble = std::sin(2.0f * 3.14159f * (40.0f - t * 4.0f) * t);
        float noise  = frand() * std::exp(-t * 1.2f) * 0.4f;
        return env * (0.7f * rumble + noise);
    }
    case AudioEvent::INNER_HORIZON: {
        if (t > 0.6f) return 0.0f;
        float freq = 800.0f + t * 6000.0f;
        float env  = std::exp(-((t - 0.15f) * (t - 0.15f)) * 40.0f);
        return 0.5f * env * std::sin(2.0f * 3.14159f * freq * t);
    }
    case AudioEvent::RING_SINGULAR: {
        if (t > 2.0f) return 0.0f;
        // Reverse cymbal: noise that rises in amplitude then cuts.
        float env = (t < 1.7f) ? (t / 1.7f) : (1.0f - (t - 1.7f) / 0.3f);
        env = env * env;
        return env * frand() * 0.45f;
    }
    case AudioEvent::NEGATIVE_AMBI: {
        if (t > 6.0f) return 0.0f;
        float env  = 0.25f * std::exp(-t * 0.3f);
        float a    = std::sin(2.0f * 3.14159f * (55.0f) * t);
        float b    = std::sin(2.0f * 3.14159f * (82.5f) * t + std::sin(t) * 2.0f);
        float dist = std::tanh((a + b) * 1.8f);
        return env * dist;
    }
    }
    return 0.0f;
}

void cb(ma_device* dev, void* out, const void*, ma_uint32 frames) {
    float* buf = (float*)out;
    float sr = (float)dev->sampleRate;
    for (ma_uint32 i = 0; i < frames; ++i) {
        float mix = 0.0f;
        for (auto& v : g_voices) {
            if (!v.active.load()) continue;
            float t = v.phase.load();
            mix += sample(v.kind.load(), t);
            v.phase.store(t + 1.0f / sr);
            if (t > 7.0f) v.active.store(false);
        }
        // Stereo interleaved.
        buf[2*i+0] = mix;
        buf[2*i+1] = mix;
    }
}

} // namespace

bool Audio::init() {
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate        = 48000;
    cfg.dataCallback      = cb;
    if (ma_device_init(nullptr, &cfg, &g_device) != MA_SUCCESS) {
        std::fprintf(stderr, "[audio] device init failed; running silent\n");
        return false;
    }
    if (ma_device_start(&g_device) != MA_SUCCESS) {
        ma_device_uninit(&g_device);
        return false;
    }
    g_inited = ok_ = true;
    return true;
}

void Audio::shutdown() {
    if (!g_inited) return;
    ma_device_uninit(&g_device);
    g_inited = false;
    ok_ = false;
}

void Audio::trigger(AudioEvent e) {
    if (!ok_) return;
    for (auto& v : g_voices) {
        if (!v.active.load()) {
            v.kind.store((int)e);
            v.phase.store(0.0f);
            v.active.store(true);
            return;
        }
    }
}

} // namespace kerr
