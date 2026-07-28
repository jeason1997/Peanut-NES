/*
 * NES 单头文件最小移植模板。
 *
 * 首先把 ROM 转换为 game.h：
 *   python3 ../tools/rom_to_header.py game.nes game.h
 *
 * 桌面测试编译：
 *   gcc -std=c11 -O2 single_header_compile.c -o nes_template
 *
 * 桌面测试运行：
 *   ./nes_template
 */
#define NES_USE_FS 0
#define NES_ENABLE_SOUND 0
#define NES_COLOR_DEPTH 16
#define NES_RENDER_LINE 1
#define NES_LOG_LEVEL 3
#define nes_log_printf(...) printf(__VA_ARGS__)
#define NES_IMPLEMENTATION
#include "../nes.h"
#include "game.h"

/*
 * 逐行显示回调，当前只保留移植接口。
 *
 * pixels 是一行 256 个 RGB565 像素。实际移植时可在这里设置 LCD 地址窗口，
 * 然后通过 SPI、8080 并口或 DMA 立即发送这一行。回调返回后缓冲会被复用。
 */
static void lcd_draw_line(nes_t *nes,
                          const nes_color_t pixels[NES_WIDTH],
                          uint16_t line)
{
    (void)nes;
    (void)pixels;
    (void)line;
}

int nes_initex(nes_t *nes)
{
    /*
     * 平台初始化入口。
     * 可在这里初始化 LCD、音频设备、按键、定时器和文件系统。
     */
    (void)nes;
    return NES_OK;
}

int nes_deinitex(nes_t *nes)
{
    /* 在这里释放或关闭平台外设。 */
    (void)nes;
    return NES_OK;
}

int nes_draw(int x1, int y1, int x2, int y2, nes_color_t *color_data)
{
    /*
     * 画面输出入口，当前故意留空。
     *
     * NES_COLOR_DEPTH=16 时，color_data 是 RGB565 像素。
     * 可在这里把矩形区域发送到 LCD 或显示控制器。
     */
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color_data;
    return NES_OK;
}

int nes_sound_output(uint8_t *buffer, size_t len)
{
    /*
     * 声音输出入口，当前故意留空。
     * 开启 NES_ENABLE_SOUND 后，可把数据送入 DAC、I2S 或音频 DMA。
     */
    (void)buffer;
    (void)len;
    return NES_OK;
}

void nes_frame(nes_t *nes)
{
    /*
     * 每帧结束时调用，当前故意不做帧率控制。
     *
     * 嵌入式移植通常在这里：
     *   1. 扫描按键并更新 nes->nes_cpu.joypad；
     *   2. 等待 60 Hz 定时器或垂直同步；
     *   3. 在退出条件成立时设置 nes->nes_quit = 1。
     *
     * 按键示例：
     *   nes->nes_cpu.joypad.A1  = button_a_pressed;
     *   nes->nes_cpu.joypad.B1  = button_b_pressed;
     *   nes->nes_cpu.joypad.U1  = button_up_pressed;
     *   nes->nes_cpu.joypad.D1  = button_down_pressed;
     *   nes->nes_cpu.joypad.L1  = button_left_pressed;
     *   nes->nes_cpu.joypad.R1  = button_right_pressed;
     *   nes->nes_cpu.joypad.ST1 = button_start_pressed;
     *   nes->nes_cpu.joypad.SE1 = button_select_pressed;
     */
    (void)nes;
}

int main(void)
{
    nes_t *nes;
    int result = 1;

    nes = nes_init();
    if (nes == NULL) {
        printf("模拟器实例创建失败。\n");
        return 1;
    }
    nes->nes_draw_line = lcd_draw_line;

    /*
     * game_rom 由 game.h 提供。嵌入式编译器通常会把 const 数组放入
     * Flash/只读数据区，不占用运行时栈空间。
     */
    if (nes_load_rom(nes, game_rom) != NES_OK) {
        printf("内置 ROM 加载失败。\n");
        goto cleanup;
    }

    printf("内置 ROM 加载成功，大小：%u 字节。\n", (unsigned)game_rom_size);
    printf("开始运行；当前模板未实现显示、声音、控制和退出条件。\n");

    /*
     * nes_run() 会持续运行，直到平台代码把 nes->nes_quit 设为 1。
     * 当前空模板没有退出输入，因此正常情况下不会自行返回。
     */
    nes_run(nes);

    nes_unload_rom(nes);
    result = 0;

cleanup:
    nes_deinit(nes);
    return result;
}
