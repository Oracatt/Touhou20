// Software mixer for the TH_SDL3 build, following the TH10 web runtime's
// miniaudio (no device IO) -> SDL3 stream split. Original PCM, DirectSound
// gain units and notification offsets remain part of TH20's recovered audio
// logic; no PCM crosses a JS message port.
#define MA_NO_DEVICE_IO
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_WAV
#define MA_NO_MP3
#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MA_NO_THREADING
#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio.h"
#include "../platform/Audio.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <memory>

namespace th20::web::audio {
namespace {
std::uint16_t word16(const std::uint8_t* p) { return std::uint16_t(p[0] | (p[1] << 8)); }
std::uint32_t word32(const std::uint8_t* p) { std::uint32_t v; std::memcpy(&v, p, 4); return v; }

struct Host {
    ma_engine engine{};
    SDL_AudioStream* stream = nullptr;
    bool ready = false, paused = false, refilling = true, primed = false;
    static constexpr int chunk = 1024, low = 4096, high = 6144;
    std::uint64_t clock_ms = 1000;
    std::map<std::uint32_t, bool> events;
    std::uint32_t next_event = 1;
    std::vector<SoundBuffer*> watched; // buffers with active notification ranges
    void (*handler)(std::uint32_t, void*) = nullptr;
    void* handler_data = nullptr;
    std::uint32_t pumps = 0, empty_checks = 0, mixed_frames = 0, min_queued = ~0u, error = 0;
    float rms = 0;
} host;

std::uint32_t block_align(const SoundBuffer& b) { return std::max(1u, std::uint32_t(word16(b.format + 12))); }

void controls(SoundBuffer& b) {
    if (!b.voice_ready) return;
    auto& sound = *static_cast<ma_sound*>(b.voice);
    ma_sound_set_volume(&sound, std::pow(10.f, float(b.volume) / 2000.f));
    ma_sound_set_pan_mode(&sound, ma_pan_mode_balance);
    ma_sound_set_pan(&sound, b.pan >= 0 ? 1.f - std::pow(10.f, -float(b.pan) / 2000.f)
                                        : std::pow(10.f, float(b.pan) / 2000.f) - 1.f);
}

bool prepare_voice(SoundBuffer& b) {
    if (b.voice_ready || (b.flags & 1)) return true; // primary-buffer flag: no voice
    const auto channels = word16(b.format + 2), align = word16(b.format + 12), bits = word16(b.format + 14);
    if (word16(b.format) != 1 || !align || !channels || (bits != 8 && bits != 16)) return false;
    if (!b.data_source) b.data_source = new ma_audio_buffer{};
    if (!b.voice) b.voice = new ma_sound{};
    auto& data = *static_cast<ma_audio_buffer*>(b.data_source);
    auto& sound = *static_cast<ma_sound*>(b.voice);
    if (!b.data_ready) {
        auto cfg = ma_audio_buffer_config_init(bits == 8 ? ma_format_u8 : ma_format_s16,
                                               channels, b.length / align, b.pcm->data(), nullptr);
        cfg.sampleRate = word32(b.format + 4);
        if (ma_audio_buffer_init(&cfg, &data) != MA_SUCCESS) return false;
        b.data_ready = true;
    }
    if (ma_sound_init_from_data_source(&host.engine, &data, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &sound) != MA_SUCCESS)
        return false;
    b.voice_ready = true;
    controls(b);
    return true;
}

void discard_voice(SoundBuffer& b) {
    if (b.voice_ready) { ma_sound_uninit(static_cast<ma_sound*>(b.voice)); b.voice_ready = false; }
    if (b.data_ready) { ma_audio_buffer_uninit(static_cast<ma_audio_buffer*>(b.data_source)); b.data_ready = false; }
}

std::uint32_t position_of(SoundBuffer& b) {
    if (!b.playing) return b.cursor;
    if (b.voice_ready) {
        ma_uint64 frame = 0;
        ma_sound_get_cursor_in_pcm_frames(static_cast<ma_sound*>(b.voice), &frame);
        return std::uint32_t(frame * block_align(b) % b.length);
    }
    const auto pos = b.cursor + std::uint32_t((host.clock_ms - b.started_ms) * b.frequency / 1000) * block_align(b);
    if (b.loop) return pos % b.length;
    if (pos >= b.length) { b.playing = false; return b.cursor = 0; }
    return pos;
}
}

SoundBuffer::~SoundBuffer() {
    auto& watched = host.watched;
    watched.erase(std::remove(watched.begin(), watched.end(), this), watched.end());
    discard_voice(*this);
    delete static_cast<ma_audio_buffer*>(data_source);
    delete static_cast<ma_sound*>(voice);
}

HRESULT SoundBuffer::GetStatus(DWORD* out) {
    if (out) *out = playing ? (DSBSTATUS_PLAYING | (loop ? DSBSTATUS_LOOPING : 0u)) : 0u;
    return S_OK;
}
HRESULT SoundBuffer::Restore() { return S_OK; }
HRESULT SoundBuffer::Lock(DWORD offset, DWORD bytes, void** first, DWORD* first_size, void** second, DWORD* second_size, DWORD) {
    offset %= length;
    const auto wanted = bytes ? bytes : length;
    if (wanted > length) return E_INVALIDARG;
    const auto head = std::min(wanted, length - offset);
    *first = pcm->data() + offset;
    *first_size = head;
    if (second) *second = wanted > head ? pcm->data() : nullptr;
    if (second_size) *second_size = wanted - head;
    return S_OK;
}
HRESULT SoundBuffer::Unlock(void*, DWORD, void*, DWORD) { return S_OK; }
HRESULT SoundBuffer::Play(DWORD, DWORD, DWORD play_flags) {
    if (!prepare_voice(*this)) return E_FAIL;
    if (playing) cursor = position_of(*this);
    playing = true;
    loop = (play_flags & DSBPLAY_LOOPING) != 0;
    started_ms = host.clock_ms;
    last_cursor = cursor;
    if (voice_ready) {
        auto& sound = *static_cast<ma_sound*>(voice);
        ma_sound_set_looping(&sound, loop);
        ma_sound_seek_to_pcm_frame(&sound, cursor / block_align(*this));
        ma_sound_start(&sound);
    }
    return S_OK;
}
HRESULT SoundBuffer::Stop() {
    cursor = position_of(*this);
    playing = false;
    if (voice_ready) ma_sound_stop(static_cast<ma_sound*>(voice));
    return S_OK;
}
HRESULT SoundBuffer::SetCurrentPosition(DWORD byte_position) {
    cursor = byte_position % length;
    started_ms = host.clock_ms;
    if (voice_ready) ma_sound_seek_to_pcm_frame(static_cast<ma_sound*>(voice), cursor / block_align(*this));
    return S_OK;
}
HRESULT SoundBuffer::GetCurrentPosition(DWORD* play, DWORD* write) {
    const auto pos = position_of(*this);
    if (play) *play = pos;
    if (write) *write = (pos + 2048) % length;
    return S_OK;
}
HRESULT SoundBuffer::SetVolume(LONG millibels) { volume = millibels; controls(*this); return S_OK; }
HRESULT SoundBuffer::SetPan(LONG millibels) { pan = millibels; controls(*this); return S_OK; }
HRESULT SoundBuffer::SetFormat(const WAVEFORMATEX* wfx) {
    discard_voice(*this);
    std::memcpy(format, wfx, 18);
    frequency = word32(format + 4);
    return S_OK;
}
HRESULT SoundBuffer::SetNotificationPositions(DWORD count, const DSBPOSITIONNOTIFY* positions) {
    notifications.clear();
    for (std::uint32_t i = 0; i < count; ++i)
        notifications.emplace_back(positions[i].dwOffset,
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(positions[i].hEventNotify)));
    auto& watched = host.watched;
    if (notifications.empty()) watched.erase(std::remove(watched.begin(), watched.end(), this), watched.end());
    else if (std::find(watched.begin(), watched.end(), this) == watched.end()) watched.push_back(this);
    last_cursor = position_of(*this);
    return S_OK;
}
HRESULT SoundBuffer::QueryInterface(REFIID, void** out) {
    if (!out) return E_POINTER;
    *out = this; // one native buffer owns both buffer and notify surfaces
    return S_OK;
}
ULONG SoundBuffer::Release() {
    delete this;
    return 0;
}

bool Device::initialize(std::uint32_t, std::uint32_t rate, std::uint32_t) {
    if (host.ready) return true;
    auto cfg = ma_engine_config_init();
    cfg.noDevice = MA_TRUE;
    cfg.sampleRate = rate;
    cfg.channels = 2;
    // DirectSound changes are immediate; do not add miniaudio's default
    // short gain ramp to the game's own fades.
    cfg.defaultVolumeSmoothTimeInPCMFrames = 0;
    if (ma_engine_init(&cfg, &host.engine) != MA_SUCCESS) { host.error = 1; return false; }
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "2048");
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, static_cast<int>(std::min<std::uint32_t>(rate, 44100u))};
    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
        host.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!host.stream) { ma_engine_uninit(&host.engine); host.error = 2; return false; }
    host.ready = true;
    host.paused = false;
    SDL_ResumeAudioStreamDevice(host.stream);
    return true;
}

HRESULT Device::CreateSoundBuffer(const DSBUFFERDESC* description, SoundBuffer** out, IUnknown*) {
    if (!description || !out) return E_INVALIDARG;
    auto b = std::make_unique<SoundBuffer>();
    b->flags = description->dwFlags;
    b->length = description->dwBufferBytes ? description->dwBufferBytes : 4096;
    if (description->lpwfxFormat) std::memcpy(b->format, description->lpwfxFormat, 18);
    else {
        const std::uint8_t pcm16[18]{1, 0, 2, 0, 0x68, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 4, 0, 16, 0, 0, 0};
        std::memcpy(b->format, pcm16, 18);
    }
    b->frequency = word32(b->format + 4);
    b->pcm = std::make_shared<std::vector<std::uint8_t>>(b->length, word16(b->format + 14) == 8 ? 128 : 0);
    *out = b.release();
    return S_OK;
}
HRESULT Device::DuplicateSoundBuffer(SoundBuffer* source, SoundBuffer** out) {
    if (!source || !out) return E_INVALIDARG;
    auto b = std::make_unique<SoundBuffer>();
    b->flags = source->flags;
    b->length = source->length;
    std::memcpy(b->format, source->format, 18);
    b->frequency = source->frequency;
    b->volume = source->volume;
    b->pan = source->pan;
    b->pcm = source->pcm; // clones share storage, cursors stay independent
    *out = b.release();
    return S_OK;
}

Device& device() {
    static Device instance;
    return instance;
}
bool ready() { return host.ready; }

std::uint32_t create_event() { const auto id = host.next_event++; host.events[id] = false; return id; }
void close_event(std::uint32_t id) { host.events.erase(id); }
bool consume_event(std::uint32_t id) {
    auto it = host.events.find(id);
    if (it == host.events.end()) return false;
    const bool signaled = it->second;
    it->second = false;
    return signaled;
}
void set_notification_handler(void (*handler)(std::uint32_t, void*), void* userdata) {
    host.handler = handler;
    host.handler_data = userdata;
}

void advance(std::uint32_t milliseconds) { host.clock_ms += milliseconds; }

namespace {
std::uint32_t render_frames(float* out, std::uint32_t count);
void service_notifications() {
    for (auto* buffer : host.watched) {
        if (!buffer->playing) { buffer->last_cursor = buffer->cursor; continue; }
        const auto pos = position_of(*buffer), last = buffer->last_cursor;
        for (const auto& note : buffer->notifications) {
            const bool crossed = (pos >= last && note.first >= last && note.first < pos) ||
                                 (pos < last && (note.first >= last || note.first < pos));
            if (!crossed) continue;
            auto it = host.events.find(note.second);
            if (it != host.events.end()) it->second = true;
            if (host.handler) host.handler(note.second, host.handler_data);
        }
        buffer->last_cursor = pos;
    }
}
std::uint32_t render_frames(float* out, std::uint32_t count) {
    std::uint32_t done = 0;
    while (done < count) {
        ma_uint64 frames = 0;
        const auto wanted = std::min<std::uint32_t>(Host::chunk, count - done);
        if (ma_engine_read_pcm_frames(&host.engine, out + done * 2, wanted, &frames) != MA_SUCCESS) { host.error = 3; break; }
        done += std::uint32_t(frames);
        // Service refill notifications between mixer slices. A single delayed
        // display callback must not collapse several ring notifications.
        service_notifications();
        if (frames < wanted) break;
    }
    return done;
}
}

void pump() {
    if (!host.ready || host.paused) return;
    int queued = std::max(0, SDL_GetAudioStreamQueued(host.stream)) / 8;
    host.min_queued = std::min(host.min_queued, std::uint32_t(queued));
    if (host.primed && queued == 0) ++host.empty_checks;
    if (!host.refilling && queued < Host::low) host.refilling = true;
    if (!host.refilling) return;
    if (queued >= Host::high) { host.refilling = false; host.primed = true; return; }
    for (int block = 0; block < (Host::high + Host::chunk - 1) / Host::chunk && queued < Host::high; ++block) {
        float pcm[Host::chunk * 2]{};
        if (render_frames(pcm, Host::chunk) != Host::chunk) { host.error = 3; return; }
        double energy = 0;
        for (auto& sample : pcm) { sample = std::clamp(sample, -1.f, 1.f); energy += double(sample) * sample; }
        host.rms = std::sqrt(energy / (Host::chunk * 2));
        if (!SDL_PutAudioStreamData(host.stream, pcm, sizeof(pcm))) { host.error = 4; return; }
        ++host.pumps;
        host.mixed_frames += Host::chunk;
        queued += Host::chunk;
    }
    if (queued >= Host::high) { host.refilling = false; host.primed = true; }
}

void pause(bool paused) {
    if (!host.stream) { host.paused = paused; return; }
    if (host.paused == paused) return;
    host.paused = paused;
    if (paused) SDL_PauseAudioStreamDevice(host.stream);
    else SDL_ResumeAudioStreamDevice(host.stream);
}
void shutdown() {
    if (host.stream) SDL_DestroyAudioStream(host.stream);
    host.stream = nullptr;
    if (host.ready) ma_engine_uninit(&host.engine);
    host.ready = false;
}

const Statistics& statistics() {
    static Statistics out;
    out.ready = host.ready;
    out.events = std::uint32_t(host.events.size());
    out.pumps = host.pumps;
    out.mixed_frames = host.mixed_frames;
    out.queued_frames = host.stream ? std::uint32_t(std::max(0, SDL_GetAudioStreamQueued(host.stream)) / 8) : 0;
    out.empty_checks = host.empty_checks;
    out.min_queued = host.min_queued;
    out.error = host.error;
    out.paused = host.paused;
    out.rms = host.rms;
    out.clock_ms = std::uint32_t(host.clock_ms);
    return out;
}
std::uint32_t render(float* out, std::uint32_t frames) { return host.ready ? render_frames(out, frames) : 0; }
}

// --- JS-facing lifecycle/music controls (shell.mjs protocol) ---------------
namespace th20::web::audio {
namespace {
bool music_enabled = true;
void* active_reader = nullptr; // WaveReader currently streaming, if any
}
bool music_is_enabled() { return music_enabled; }
void track_reader(void* reader) { active_reader = reader; }
void* current_reader() { return active_reader; }
}

namespace th20::source::audio { void reopen_music_stream_file(); }

extern "C" {
__attribute__((export_name("sdl_audio_pump"))) void sdl_audio_pump() try { th20::web::audio::pump(); }
catch (const std::exception& e) { std::fprintf(stderr, "sdl_audio_pump: exception: %s\n", e.what()); }
catch (...) { std::fprintf(stderr, "sdl_audio_pump: unknown exception\n"); }
__attribute__((export_name("sdl_audio_pause"))) void sdl_audio_pause(std::uint32_t paused) { th20::web::audio::pause(paused != 0); }
__attribute__((export_name("sdl_audio_shutdown"))) void sdl_audio_shutdown() { th20::web::audio::shutdown(); }
__attribute__((export_name("sdl_audio_stats"))) const th20::web::audio::Statistics* sdl_audio_stats() { return &th20::web::audio::statistics(); }
__attribute__((export_name("sdl_audio_render"))) std::uint32_t sdl_audio_render(float* out, std::uint32_t frames) try { return th20::web::audio::render(out, frames); }
catch (const std::exception& e) { std::fprintf(stderr, "sdl_audio_render: exception: %s\n", e.what()); return 0; }
catch (...) { std::fprintf(stderr, "sdl_audio_render: unknown exception\n"); return 0; }
__attribute__((export_name("sdl_audio_event"))) std::uint32_t sdl_audio_event(std::uint32_t op, std::uint32_t id) {
    if (op == 0) return th20::web::audio::create_event();
    if (op == 2) { th20::web::audio::close_event(id); return 0; }
    return th20::web::audio::consume_event(id) ? 0 : 0x102;
}
// BGM enable follows the TH10 protocol: music keeps its timing and stream
// state, but the PCM the game reads becomes silence.
__attribute__((export_name("sdl_music_enabled"))) void sdl_music_enabled(std::uint32_t enabled) { th20::web::audio::music_enabled = enabled != 0; }
__attribute__((export_name("sdl_music_resource_changed"))) void sdl_music_resource_changed() {
    // thbgm.dat arrives through the file host; drop the streaming handle so
    // the next read reopens against the fresh MEMFS bytes.
    th20::source::audio::reopen_music_stream_file();
}
__attribute__((export_name("sdl_music_stats"))) const std::uint32_t* sdl_music_stats() {
    static std::uint32_t out[6]{};
    out[0] = th20::web::audio::music_is_enabled();
    out[1] = 0; // no OGG decode mode on this runtime (thbgm.dat is canonical)
    out[2] = th20::web::audio::current_reader() ? 1 : 0;
    return out;
}
__attribute__((export_name("sdl_ogg_decode_mode"))) void sdl_ogg_decode_mode(std::uint32_t) {
    // The canonical BGM source is thbgm.dat itself; there is no decode mode.
}
}
