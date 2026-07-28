/*
 * NES 单头文件 SDL2 示例。
 *
 * 编译：
 *   gcc -std=c11 -O2 nes_sdl.c $(sdl2-config --cflags --libs) -o nes_sdl
 *
 * 运行：
 *   ./nes_sdl /path/to/game.nes
 */

#define NES_ENABLE_SOUND 1
#define NES_USE_FS 1
#define NES_USE_SRAM 0
#define NES_COLOR_DEPTH 32
#define NES_ENABLE_HEAVY_MAPPERS 1
#define NES_LOG_LEVEL 3
#define nes_log_printf(...) printf(__VA_ARGS__)
#define NES_IMPLEMENTATION
#include "../nes.h"

#include <SDL2/SDL.h>

static SDL_Window *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture *s_texture;
static SDL_AudioDeviceID s_audio_device;
static uint64_t s_next_frame_tick;

static void nes_sdl_set_key(nes_t *nes, SDL_Scancode key, uint8_t pressed)
{
    switch (key) {
    case SDL_SCANCODE_W:
    case SDL_SCANCODE_UP:     nes->nes_cpu.joypad.U1 = pressed; break;
    case SDL_SCANCODE_S:
    case SDL_SCANCODE_DOWN:   nes->nes_cpu.joypad.D1 = pressed; break;
    case SDL_SCANCODE_A:
    case SDL_SCANCODE_LEFT:   nes->nes_cpu.joypad.L1 = pressed; break;
    case SDL_SCANCODE_D:
    case SDL_SCANCODE_RIGHT:  nes->nes_cpu.joypad.R1 = pressed; break;
    case SDL_SCANCODE_J:
    case SDL_SCANCODE_Z:      nes->nes_cpu.joypad.A1 = pressed; break;
    case SDL_SCANCODE_K:
    case SDL_SCANCODE_X:      nes->nes_cpu.joypad.B1 = pressed; break;
    case SDL_SCANCODE_V:
    case SDL_SCANCODE_RSHIFT: nes->nes_cpu.joypad.SE1 = pressed; break;
    case SDL_SCANCODE_B:
    case SDL_SCANCODE_RETURN: nes->nes_cpu.joypad.ST1 = pressed; break;
    default: break;
    }
}

static void nes_sdl_poll_events(nes_t *nes)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            nes->nes_quit = 1;
        } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            const uint8_t pressed = event.type == SDL_KEYDOWN;
            if (pressed && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                nes->nes_quit = 1;
            } else {
                nes_sdl_set_key(nes, event.key.keysym.scancode, pressed);
            }
        }
    }
}

int nes_initex(nes_t *nes)
{
    SDL_AudioSpec desired;
    (void)nes;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL 初始化失败：%s\n", SDL_GetError());
        return NES_ERROR;
    }

    s_window = SDL_CreateWindow(
        "NES 单头文件模拟器",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        NES_WIDTH, NES_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (s_window == NULL) {
        fprintf(stderr, "创建窗口失败：%s\n", SDL_GetError());
        return NES_ERROR;
    }

    s_renderer = SDL_CreateRenderer(
        s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (s_renderer == NULL) {
        /* WSL 或远程桌面没有硬件加速时退回软件渲染。 */
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (s_renderer == NULL) {
        fprintf(stderr, "创建渲染器失败：%s\n", SDL_GetError());
        return NES_ERROR;
    }

    SDL_RenderSetLogicalSize(s_renderer, NES_WIDTH, NES_HEIGHT);
    s_texture = SDL_CreateTexture(
        s_renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);
    if (s_texture == NULL) {
        fprintf(stderr, "创建画面纹理失败：%s\n", SDL_GetError());
        return NES_ERROR;
    }

    SDL_zero(desired);
    desired.freq = NES_APU_SAMPLE_RATE;
    desired.format = AUDIO_U8;
    desired.channels = 1;
    desired.samples = NES_APU_SAMPLE_PER_SYNC;
    s_audio_device = SDL_OpenAudioDevice(
        NULL, SDL_FALSE, &desired, NULL, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (s_audio_device == 0) {
        fprintf(stderr, "音频不可用，将静音运行：%s\n", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(s_audio_device, SDL_FALSE);
    }
    return NES_OK;
}

int nes_deinitex(nes_t *nes)
{
    (void)nes;
    if (s_audio_device != 0) {
        SDL_CloseAudioDevice(s_audio_device);
        s_audio_device = 0;
    }
    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    s_texture = NULL;
    s_renderer = NULL;
    s_window = NULL;
    SDL_Quit();
    return NES_OK;
}

int nes_draw(int x1, int y1, int x2, int y2, nes_color_t *color_data)
{
    SDL_Rect rect = {x1, y1, x2 - x1 + 1, y2 - y1 + 1};
    if (s_texture == NULL) {
        return NES_ERROR;
    }
    return SDL_UpdateTexture(s_texture, &rect, color_data,
                             rect.w * (int)sizeof(nes_color_t)) == 0
               ? NES_OK
               : NES_ERROR;
}

int nes_sound_output(uint8_t *buffer, size_t len)
{
    const uint32_t max_queue = NES_APU_SAMPLE_PER_SYNC * 4u;
    if (s_audio_device == 0) {
        return NES_OK;
    }
    if (SDL_GetQueuedAudioSize(s_audio_device) > max_queue) {
        SDL_ClearQueuedAudio(s_audio_device);
    }
    return SDL_QueueAudio(s_audio_device, buffer, (uint32_t)len) == 0
               ? NES_OK
               : NES_ERROR;
}

void nes_frame(nes_t *nes)
{
    const uint64_t frequency = SDL_GetPerformanceFrequency();
    const uint64_t frame_ticks = frequency / 60u;
    uint64_t now;

    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
    nes_sdl_poll_events(nes);

    if (s_next_frame_tick == 0) {
        s_next_frame_tick = SDL_GetPerformanceCounter();
    }
    s_next_frame_tick += frame_ticks;
    now = SDL_GetPerformanceCounter();
    if (now < s_next_frame_tick) {
        SDL_Delay((uint32_t)((s_next_frame_tick - now) * 1000u / frequency));
    } else if (now - s_next_frame_tick > frame_ticks * 2u) {
        s_next_frame_tick = now;
    }
}

int main(int argc, char **argv)
{
    nes_t *nes;
    int result = 1;

    if (argc != 2) {
        fprintf(stderr, "用法：%s <游戏 ROM.nes>\n", argv[0]);
        return 1;
    }

    nes = nes_init();
    if (nes == NULL || s_window == NULL || s_renderer == NULL || s_texture == NULL) {
        fprintf(stderr, "模拟器初始化失败。\n");
        if (nes != NULL) {
            nes_deinit(nes);
        }
        return 1;
    }

    if (nes_load_file(nes, argv[1]) != NES_OK) {
        fprintf(stderr, "ROM 加载失败：%s\n", argv[1]);
        goto cleanup;
    }

    printf("已加载：%s\n", argv[1]);
    printf("按键：方向键/WASD，A=J/Z，B=K/X，Select=V/右Shift，Start=B/Enter，退出=Esc。\n");
    nes_run(nes);
    nes_unload_file(nes);
    result = 0;

cleanup:
    nes_deinit(nes);
    return result;
}
