// SDL3 input host for the TH_SDL3 build, replacing DirectInput8/XInput/WinMM
// and the WMI device query. Sources: launcher keyboard bridge (sdl_key), SDL
// keyboard state, SDL gamepads (as DirectInput-kind devices) and the shared
// gesture controller (touch). Everything reported to the game is real input
// state; APIs that have no browser meaning report honest absence (XInput and
// WinMM joysticks stay unconnected).
#include "../../../source_reconstruction/input/input.hpp"
namespace th20::source::gameplay { class GameController; extern GameController* controller; }
#include "../platform/Time.hpp"
#include "../../../portable/input/TouchController.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace th20::source::input {
namespace {
struct Key {const char* code;const char* sdl;std::uint32_t scan,vk;bool hosted=false;SDL_Scancode native=SDL_SCANCODE_UNKNOWN;};
#include "../../../portable/input/KeyboardMap.inc"

touhou::input::TouchController gestures;
std::uint8_t synthetic_keys[256]{};
bool touch_keys[256]{};

SDL_Gamepad* pads[4]{};
constexpr SDL_GamepadButton pad_slots[]={SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_WEST,
    SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_BACK,SDL_GAMEPAD_BUTTON_START,SDL_GAMEPAD_BUTTON_LEFT_STICK,SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_GUIDE};
constexpr SDL_GamepadAxis pad_axes[]={SDL_GAMEPAD_AXIS_LEFTX,SDL_GAMEPAD_AXIS_LEFTY,SDL_GAMEPAD_AXIS_RIGHTX,
    SDL_GAMEPAD_AXIS_RIGHTY,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER};

int pad_index(IDirectInputDevice8W* device) {
    return static_cast<int>(reinterpret_cast<std::uintptr_t>(device)) - 1;
}
void add_pad(SDL_JoystickID id) {
    for (const auto* p : pads) if (p && SDL_GetGamepadID(const_cast<SDL_Gamepad*>(p)) == id) return;
    for (auto*& p : pads) if (!p) { p = SDL_OpenGamepad(id); return; }
}
void remove_pad(SDL_JoystickID id) {
    for (auto*& p : pads) if (p && SDL_GetGamepadID(p) == id) { SDL_CloseGamepad(p); p = nullptr; }
}
void press(std::uint8_t* out, std::uint32_t vk) { out[vk] |= 0x80; }

// Scene adapter for gesture context: gameplay when a GameController exists.
touhou::input::TouchState touch_state() {
    touhou::input::TouchState s;
    s.context = gameplay::controller ? 1 : 0;
    s.ready = s.context == 1; // keys and stick work; absolute drag needs player anchors
    s.min_x = -184; s.max_x = 184; s.min_y = 32; s.max_y = 432;
    return s;
}

// The SDL implementation is the Host definition on this platform (no base
// win32 implementation exists here).
struct SdlHost : Host {
    BOOL keyboard(std::uint8_t* output) override {
        std::memcpy(output, synthetic_keys, 256);
        const bool* physical = SDL_GetKeyboardState(nullptr);
        for (const auto& k : keyboard_map)
            if ((k.native != SDL_SCANCODE_UNKNOWN && physical[k.native]) || k.hosted) {
                press(output, k.vk);
                if (k.vk >= 160 && k.vk <= 165) press(output, 16 + (k.vk - 160) / 2);
            }
        for (int vk = 0; vk < 256; ++vk) if (touch_keys[vk]) press(output, std::uint32_t(vk));
        return TRUE;
    }
    BOOL set_keyboard(std::uint8_t* input) override {
        // Our keyboard state is synthetic, so the original clear-high-bits
        // write genuinely applies to it.
        std::memcpy(synthetic_keys, input, 256);
        return TRUE;
    }
    DWORD xinput(DWORD, XINPUT_STATE*) override {
        // No XInput on the web; SDL gamepads enumerate as DirectInput-kind
        // devices instead. Honest "not connected" for all four slots.
        return ERROR_DEVICE_NOT_CONNECTED;
    }
    MMRESULT joystick(UINT, JOYINFOEX*) override { return JOYERR_UNPLUGGED; }
    MMRESULT joystick_caps(UINT, JOYCAPSW*) override { return JOYERR_UNPLUGGED; }
    HRESULT poll(IDirectInputDevice8W*) override { return S_OK; }
    HRESULT acquire(IDirectInputDevice8W*) override { return S_OK; }
    HRESULT device_state(IDirectInputDevice8W* device, DIJOYSTATE2* output) override {
        const int index = pad_index(device);
        if (index < 0 || index >= 4 || !pads[index] || !SDL_GamepadConnected(pads[index])) return DIERR_INPUTLOST;
        auto* pad = pads[index];
        std::memset(output, 0, sizeof *output);
        LONG* axes = &output->lX;
        for (int n = 0; n < 6; ++n) {
            const int value = SDL_GetGamepadAxis(pad, pad_axes[n]);
            const double unit = value < 0 ? value / 32768. : value / 32767.;
            axes[n] = LONG(std::floor((unit + 1) * 32767.5 + .5));
        }
        for (std::uint32_t n = 0; n < sizeof(pad_slots) / sizeof(*pad_slots); ++n)
            if (SDL_GetGamepadButton(pad, pad_slots[n])) output->rgbButtons[n] = 0x80;
        const int x = int(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) - int(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT));
        const int y = int(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) - int(SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP));
        if (x) { output->lX = x > 0 ? 65535 : 0; }
        if (y) { output->lY = y > 0 ? 65535 : 0; }
        output->rgdwPOV[0] = (x || y)
            ? std::uint32_t((int(std::round(std::atan2(double(x), double(-y)) * 180 / 3.141592653589793)) + 360) % 360) * 100
            : ~0u;
        for (int n = 1; n < 4; ++n) output->rgdwPOV[n] = ~0u;
        return S_OK;
    }
};
SdlHost sdl_host;

void pump_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) add_pad(event.gdevice.which);
        else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) remove_pad(event.gdevice.which);
        else if (event.type == SDL_EVENT_FINGER_CANCELED) { gestures.cancel_transient(); }
        else if (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION || event.type == SDL_EVENT_FINGER_UP) {
            const auto type = event.type == SDL_EVENT_FINGER_DOWN ? 0 : event.type == SDL_EVENT_FINGER_MOTION ? 1 : 2;
            gestures.pointer(type, int(event.tfinger.fingerID), event.tfinger.x, event.tfinger.y, SDL_GetTicks(),
                             touch_state(), (synthetic_keys[16] & 0x80) != 0);
        }
    }
}
}

BOOL Host::keyboard(std::uint8_t* output) { return sdl_host.SdlHost::keyboard(output); }
BOOL Host::set_keyboard(std::uint8_t* input) { return sdl_host.SdlHost::set_keyboard(input); }
DWORD Host::xinput(DWORD index, XINPUT_STATE* output) { return sdl_host.SdlHost::xinput(index, output); }
MMRESULT Host::joystick(UINT index, JOYINFOEX* output) { return sdl_host.SdlHost::joystick(index, output); }
MMRESULT Host::joystick_caps(UINT index, JOYCAPSW* output) { return sdl_host.SdlHost::joystick_caps(index, output); }
HRESULT Host::poll(IDirectInputDevice8W* device) { return sdl_host.SdlHost::poll(device); }
HRESULT Host::acquire(IDirectInputDevice8W* device) { return sdl_host.SdlHost::acquire(device); }
HRESULT Host::device_state(IDirectInputDevice8W* device, DIJOYSTATE2* output) { return sdl_host.SdlHost::device_state(device, output); }

Host& sdl_input_host() { return sdl_host; }

// Called once per game tick by the frame loop (the sdl_native_input role).
void sample_native_input() {
    pump_events();
    const auto state = touch_state();
    const auto sample = gestures.sample(state, SDL_GetTicks(), (synthetic_keys[16] & 0x80) != 0,
        (synthetic_keys[37] | synthetic_keys[38] | synthetic_keys[39] | synthetic_keys[40]) & 0x80);
    std::memcpy(touch_keys, sample.keys, sizeof(touch_keys));
    (void)sample.motion; // absolute-drag movement has no analog channel in TH20 (documented)
}

void initialize_input_host() {
    for (auto& k : keyboard_map) k.native = SDL_GetScancodeFromName(k.sdl);
    SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    int count = 0;
    auto* ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i) add_pad(ids[i]);
    SDL_free(ids);
    gestures.begin_session();
}
void shutdown_input_host() {
    for (auto*& p : pads) if (p) { SDL_CloseGamepad(p); p = nullptr; }
    gestures.cancel_transient();
}

// Gamepad slots for Controller::initialize_direct_input: ids as pointers.
IDirectInputDevice8W* host_pad(unsigned index) {
    return index < 4 && pads[index] ? reinterpret_cast<IDirectInputDevice8W*>(std::uintptr_t(index + 1)) : nullptr;
}
}

extern "C" {
__attribute__((export_name("sdl_native_input"))) void sdl_native_input() { th20::source::input::sample_native_input(); }
__attribute__((export_name("sdl_key"))) void sdl_key(const char* code, std::uint32_t down) {
    for (auto& k : th20::source::input::keyboard_map)
        if (!std::strcmp(code, k.code)) { k.hosted = down != 0; return; }
}
__attribute__((export_name("sdl_keys_clear"))) void sdl_keys_clear() {
    for (auto& k : th20::source::input::keyboard_map) k.hosted = false;
    std::memset(th20::source::input::synthetic_keys, 0, sizeof(th20::source::input::synthetic_keys));
    th20::source::input::gestures.reset();
}
__attribute__((export_name("sdl_touch"))) void sdl_touch(std::uint32_t type, std::int32_t id, float x, float y) {
    th20::source::input::gestures.pointer(int(type), id, x, y, SDL_GetTicks(), th20::source::input::touch_state(), false);
}
__attribute__((export_name("sdl_touch_cancel"))) void sdl_touch_cancel() { th20::source::input::gestures.cancel_transient(); }
__attribute__((export_name("sdl_touch_options"))) void sdl_touch_options(std::uint32_t on, std::uint32_t free_mode, float speed) {
    auto& g = th20::source::input::gestures;
    g.enabled = on; g.unlimited = free_mode; g.sensitivity = std::clamp(speed, 1.f, 3.f);
    if (!on) g.cancel_transient();
}
__attribute__((export_name("sdl_touch_gestures"))) void sdl_touch_gestures(std::uint32_t two, std::uint32_t taps) {
    th20::source::input::gestures.two_finger = two; th20::source::input::gestures.double_tap = taps;
}
__attribute__((export_name("sdl_touch_mode"))) void sdl_touch_mode(std::uint32_t mode) {
    th20::source::input::gestures.set_mode(int(mode));
}
__attribute__((export_name("sdl_touch_controls"))) void sdl_touch_controls(std::uint32_t shoot, std::uint32_t slow,
    std::uint32_t bomb, std::uint32_t escape, float x, float y) {
    th20::source::input::gestures.controls(shoot, slow, bomb, escape, x, y);
}
}
