#include <SDL.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "audio.h"
#include "gfx.h"
#include "input.h"
#include "native_game.h"
#include "native_io.h"
#include "native_pad.h"
}

#include "host_platform.h"

namespace {

constexpr int kLogicalWidth = GFX_LOGICAL_WIDTH;
constexpr int kLogicalHeight = GFX_LOGICAL_HEIGHT;
constexpr int kFramebufferWidth = GFX_WIDTH;
constexpr int kFramebufferHeight = GFX_HEIGHT;
constexpr int kWindowScale = 6;
constexpr int kStickActivityThreshold = 6000;
constexpr int kGameStateTitle = 0;
constexpr int kGameStateGameOver = 4;

static_assert(kFramebufferWidth == kLogicalWidth * GFX_SCALE);
static_assert(kFramebufferHeight == kLogicalHeight * GFX_SCALE);
static_assert((kFramebufferWidth & 1) == 0);

constexpr uint16_t kXinputDpadUp = 0x0001;
constexpr uint16_t kXinputDpadDown = 0x0002;
constexpr uint16_t kXinputDpadLeft = 0x0004;
constexpr uint16_t kXinputDpadRight = 0x0008;
constexpr uint16_t kXinputStart = 0x0010;
constexpr uint16_t kXinputBack = 0x0020;
constexpr uint16_t kXinputA = 0x1000;
constexpr uint16_t kXinputB = 0x2000;
constexpr uint16_t kXinputY = 0x8000;
constexpr uint8_t kHidKeyQ = 20;

constexpr uint32_t kPalette[16] = {
    0xff050713u, 0xff101b35u, 0xff40204au, 0xff0f4a42u,
    0xff75402cu, 0xff4b5366u, 0xffa4adbdu, 0xfffff7e8u,
    0xffff3f5fu, 0xffff9f2du, 0xffffe66du, 0xff40d99cu,
    0xff3db4ffu, 0xff8174b8u, 0xffed78b5u, 0xffffc0a0u,
};

struct Controller {
    SDL_GameController *handle = nullptr;
    SDL_JoystickID instance_id = -1;
};

void print_usage(const char *program) {
    std::printf(
        "Usage: %s [--frames N] [--mute] [--demo] [--god] [--no-fire] [--fast] [--screenshot FILE]\n"
        "  --frames N  Exit after N rendered frames (useful for smoke tests).\n"
        "  --mute       Do not initialize the SDL audio device.\n"
        "  --demo       Drive the game with a synthetic controller.\n"
        "  --god        Select GOD TEST for a synthetic demo run.\n"
        "  --no-fire    Disable synthetic firing during a demo (visual QA).\n"
        "  --fast       Use fixed 60 Hz steps without real-time frame pacing.\n"
        "  --screenshot FILE  Save the final frame as a BMP image.\n",
        program);
}

bool parse_nonnegative(const char *text, int *value) {
    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < 0 || parsed > INT_MAX) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool open_controller(Controller *controller, int device_index) {
    if (controller->handle != nullptr || !SDL_IsGameController(device_index)) {
        return false;
    }

    SDL_GameController *handle = SDL_GameControllerOpen(device_index);
    if (handle == nullptr) {
        std::fprintf(stderr, "Controller open failed: %s\n", SDL_GetError());
        return false;
    }

    controller->handle = handle;
    controller->instance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(handle));
    std::printf("Controller: %s\n", SDL_GameControllerName(handle));
    return true;
}

void close_controller(Controller *controller) {
    if (controller->handle != nullptr) {
        SDL_GameControllerClose(controller->handle);
    }
    controller->handle = nullptr;
    controller->instance_id = -1;
    native_pad_init();
}

bool controller_button(const Controller &controller, SDL_GameControllerButton button) {
    return controller.handle != nullptr &&
        SDL_GameControllerGetButton(controller.handle, button) != 0;
}

uint8_t trigger_to_byte(Sint16 value) {
    if (value <= 0) {
        return 0;
    }
    return static_cast<uint8_t>((static_cast<int32_t>(value) * 255) / 32767);
}

Sint16 invert_axis(Sint16 value) {
    return value == INT16_MIN ? INT16_MAX : static_cast<Sint16>(-value);
}

void update_controller(const Controller &controller) {
    if (controller.handle == nullptr) {
        return;
    }

    uint16_t buttons = 0;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) buttons |= kXinputDpadUp;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) buttons |= kXinputDpadDown;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) buttons |= kXinputDpadLeft;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= kXinputDpadRight;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_START)) buttons |= kXinputStart;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_BACK)) buttons |= kXinputBack;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_A)) buttons |= kXinputA;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_B)) buttons |= kXinputB;
    if (controller_button(controller, SDL_CONTROLLER_BUTTON_Y)) buttons |= kXinputY;

    const Sint16 lx = SDL_GameControllerGetAxis(controller.handle, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 ly = invert_axis(
        SDL_GameControllerGetAxis(controller.handle, SDL_CONTROLLER_AXIS_LEFTY));
    const Sint16 rx = SDL_GameControllerGetAxis(controller.handle, SDL_CONTROLLER_AXIS_RIGHTX);
    const Sint16 ry = invert_axis(
        SDL_GameControllerGetAxis(controller.handle, SDL_CONTROLLER_AXIS_RIGHTY));
    const uint8_t lt = trigger_to_byte(
        SDL_GameControllerGetAxis(controller.handle, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    const uint8_t rt = trigger_to_byte(
        SDL_GameControllerGetAxis(controller.handle, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));

    native_pad_update_xinput(0, buttons, lx, ly, rx, ry, lt, rt);
}

bool any_key(const Uint8 *keys, std::initializer_list<SDL_Scancode> choices) {
    for (const SDL_Scancode key : choices) {
        if (keys[key]) {
            return true;
        }
    }
    return false;
}

void update_keyboard_and_mouse(SDL_Renderer *renderer, bool mouse_mode) {
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    int window_x = 0;
    int window_y = 0;
    const Uint32 mouse = SDL_GetMouseState(&window_x, &window_y);

    float render_x = static_cast<float>(kFramebufferWidth) * 0.5f;
    float render_y = static_cast<float>(kFramebufferHeight) * 0.5f;
    SDL_RenderWindowToLogical(renderer, window_x, window_y, &render_x, &render_y);
    const int aim_x = std::clamp(
        static_cast<int>(std::floor(render_x / static_cast<float>(GFX_SCALE))),
        0, kLogicalWidth - 1);
    const int aim_y = std::clamp(
        static_cast<int>(std::floor(render_y / static_cast<float>(GFX_SCALE))),
        0, kLogicalHeight - 1);
    host_io_set_mouse_aim(
        mouse_mode,
        aim_x,
        aim_y);

    host_input_set_button(0, 0, any_key(keys, {SDL_SCANCODE_LEFT, SDL_SCANCODE_A}));
    host_input_set_button(0, 1, any_key(keys, {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D}));
    host_input_set_button(0, 2, any_key(keys, {SDL_SCANCODE_UP, SDL_SCANCODE_W}));
    host_input_set_button(0, 3, any_key(keys, {SDL_SCANCODE_DOWN, SDL_SCANCODE_S}));
    host_input_set_button(
        0, 4,
        any_key(keys, {SDL_SCANCODE_SPACE, SDL_SCANCODE_Z}) ||
            (mouse & SDL_BUTTON_LMASK) != 0);
    host_input_set_button(
        0, 5,
        any_key(keys, {SDL_SCANCODE_R, SDL_SCANCODE_X, SDL_SCANCODE_B}) ||
            (mouse & SDL_BUTTON_RMASK) != 0);
    host_input_set_button(0, 6, any_key(keys, {SDL_SCANCODE_RETURN, SDL_SCANCODE_P}));

    host_io_set_service(any_key(keys, {SDL_SCANCODE_TAB, SDL_SCANCODE_BACKSPACE}));
    host_io_set_coin(keys[SDL_SCANCODE_C] != 0);
    input_key_callback(kHidKeyQ, 'q', keys[SDL_SCANCODE_Q] != 0, 0);
}

void unpack_framebuffer(uint32_t *pixels) {
    const uint8_t *packed = gfx_get_fb();
    for (int y = 0; y < kFramebufferHeight; ++y) {
        for (int x = 0; x < kFramebufferWidth; x += 2) {
            const uint8_t pair = packed[y * (kFramebufferWidth / 2) + x / 2];
            pixels[y * kFramebufferWidth + x] = kPalette[pair & 0x0f];
            pixels[y * kFramebufferWidth + x + 1] = kPalette[(pair >> 4) & 0x0f];
        }
    }
}

bool render_frame(SDL_Renderer *renderer, SDL_Texture *texture) {
    static uint32_t pixels[kFramebufferWidth * kFramebufferHeight];
    unpack_framebuffer(pixels);
    if (SDL_UpdateTexture(
            texture, nullptr, pixels, kFramebufferWidth * sizeof(uint32_t)) != 0) {
        std::fprintf(stderr, "Texture update failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (SDL_RenderCopy(renderer, texture, nullptr, nullptr) != 0) {
        std::fprintf(stderr, "Render copy failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_RenderPresent(renderer);
    return true;
}

bool save_frame_bmp(const char *path) {
    uint32_t *pixels = new uint32_t[kFramebufferWidth * kFramebufferHeight];
    unpack_framebuffer(pixels);
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
        pixels,
        kFramebufferWidth,
        kFramebufferHeight,
        32,
        kFramebufferWidth * static_cast<int>(sizeof(uint32_t)),
        0x00ff0000u,
        0x0000ff00u,
        0x000000ffu,
        0xff000000u);
    bool saved = false;
    if (surface != nullptr) {
        saved = SDL_SaveBMP(surface, path) == 0;
        SDL_FreeSurface(surface);
    }
    if (!saved) std::fprintf(stderr, "Screenshot failed: %s\n", SDL_GetError());
    delete[] pixels;
    return saved;
}

void pace_frame(Uint64 frame_started, Uint64 performance_frequency) {
    const double target_seconds = 1.0 / 60.0;
    for (;;) {
        const Uint64 now = SDL_GetPerformanceCounter();
        const double elapsed = static_cast<double>(now - frame_started) /
            static_cast<double>(performance_frequency);
        const double remaining = target_seconds - elapsed;
        if (remaining <= 0.0) {
            return;
        }
        if (remaining > 0.002) {
            SDL_Delay(static_cast<Uint32>(remaining * 1000.0) - 1);
        } else {
            SDL_Delay(0);
        }
    }
}

}  // namespace

int main(int argc, char **argv) {
    int frame_limit = -1;
    bool muted = false;
    bool demo = false;
    bool god_demo = false;
    bool demo_fire = true;
    bool fast = false;
    const char *screenshot_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0) {
            if (++i >= argc || !parse_nonnegative(argv[i], &frame_limit)) {
                std::fprintf(stderr, "--frames requires a nonnegative integer\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--mute") == 0) {
            muted = true;
        } else if (std::strcmp(argv[i], "--demo") == 0) {
            demo = true;
        } else if (std::strcmp(argv[i], "--god") == 0) {
            god_demo = true;
            demo = true;
        } else if (std::strcmp(argv[i], "--no-fire") == 0) {
            demo_fire = false;
        } else if (std::strcmp(argv[i], "--fast") == 0) {
            fast = true;
        } else if (std::strcmp(argv[i], "--screenshot") == 0) {
            if (++i >= argc) {
                std::fprintf(stderr, "--screenshot requires a file path\n");
                return 2;
            }
            screenshot_path = argv[i];
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Fruit Jam: Vector Raid (SDL host)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kLogicalWidth * kWindowScale,
        kLogicalHeight * kWindowScale,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) {
        std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        std::fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, kFramebufferWidth, kFramebufferHeight);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        kFramebufferWidth,
        kFramebufferHeight);
    if (texture == nullptr) {
        std::fprintf(stderr, "Texture creation failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Controller controller;
    for (int index = 0; index < SDL_NumJoysticks() && controller.handle == nullptr; ++index) {
        (void)open_controller(&controller, index);
    }

    gfx_init();
    native_pad_init();
    native_io_init();
    if (!muted && !audio_init()) {
        std::fprintf(stderr, "Audio unavailable; continuing without sound: %s\n", SDL_GetError());
    }
    native_game_init();

    const Uint64 performance_frequency = SDL_GetPerformanceFrequency();
    Uint64 previous_counter = SDL_GetPerformanceCounter();
    int rendered_frames = 0;
    bool running = frame_limit != 0;
    bool mouse_mode = true;

    while (running) {
        const Uint64 frame_started = SDL_GetPerformanceCounter();
        double dt = static_cast<double>(frame_started - previous_counter) /
            static_cast<double>(performance_frequency);
        previous_counter = frame_started;
        if (fast || dt <= 0.0 || dt > 0.05) {
            dt = 1.0 / 60.0;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        running = false;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    mouse_mode = true;
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    if (event.caxis.which == controller.instance_id &&
                        (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                         event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) &&
                        std::abs(static_cast<int>(event.caxis.value)) > kStickActivityThreshold) {
                        mouse_mode = false;
                    }
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    if (event.cbutton.which == controller.instance_id) {
                        mouse_mode = false;
                    }
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                    (void)open_controller(&controller, event.cdevice.which);
                    break;
                case SDL_CONTROLLERDEVICEREMOVED:
                    if (event.cdevice.which == controller.instance_id) {
                        close_controller(&controller);
                    }
                    break;
                default:
                    break;
            }
        }
        if (!running) {
            break;
        }

        const Uint8 *keys = SDL_GetKeyboardState(nullptr);
        if (any_key(keys, {
                SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
                SDL_SCANCODE_A, SDL_SCANCODE_D,
                SDL_SCANCODE_W, SDL_SCANCODE_S})) {
            mouse_mode = false;
        }

        update_keyboard_and_mouse(renderer, mouse_mode);
        if (demo) {
            int target_x = 64;
            int target_y = 64;
            const bool has_target = native_game_demo_target(&target_x, &target_y);
            host_io_set_mouse_aim(true, target_x, target_y);
            uint16_t buttons = 0;
            const int game_state = native_game_state();
            if (game_state == kGameStateTitle && god_demo && rendered_frames < 2) {
                if (rendered_frames == 0) buttons |= kXinputDpadRight;
            } else if ((game_state == kGameStateTitle || game_state == kGameStateGameOver) &&
                       (rendered_frames & 1) == 0) {
                    buttons |= kXinputStart;
            }
            if (rendered_frames % 150 == 20) buttons |= kXinputB;
            if (rendered_frames % 420 == 80) buttons |= kXinputY;
            const uint8_t trigger = demo_fire && has_target && rendered_frames % 11 == 0 ? 255 : 0;
            native_pad_update_xinput(0, buttons, 0, 0, 0, 0, 0, trigger);
        } else {
            update_controller(controller);
        }
        input_update();
        native_game_frame(static_cast<float>(dt), SDL_GetTicks());
        if (!render_frame(renderer, texture)) {
            running = false;
            break;
        }

        ++rendered_frames;
        if (frame_limit >= 0 && rendered_frames >= frame_limit) {
            running = false;
        } else if (!fast) {
            pace_frame(frame_started, performance_frequency);
        }
    }

    if (screenshot_path != nullptr && !save_frame_bmp(screenshot_path)) {
        close_controller(&controller);
        host_audio_shutdown();
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (demo) {
        std::printf("Demo summary: state=%d wave=%d score=%d enemies=%d frames=%d "
                    "god=%d ammo=%d lives=%d shield=%d\n",
                    native_game_state(), native_game_wave(), native_game_score(),
                    native_game_live_enemies(), rendered_frames,
                    native_game_god_mode() ? 1 : 0, native_game_ammo(),
                    native_game_lives(), native_game_shield());
    }

    close_controller(&controller);
    host_audio_shutdown();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
