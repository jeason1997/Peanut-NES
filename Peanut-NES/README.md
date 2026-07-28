# Peanut-NES：单头文件 NES 模拟器

Peanut-NES 是本工程的单头文件发行形式。完整的 CPU、PPU、APU、ROM 加载及
Mapper 逻辑均合并在一个 [`nes.h`](./nes.h) 中，使用方式类似
[Peanut-GB](https://github.com/deltabeard/Peanut-GB)，便于移植到 STM32、
ESP32、RT-Thread、裸机及其他资源受限平台。

> 本目录只提供模拟器和移植示例。使用 ROM 时请遵守所在地法律，并只使用你有权
> 使用的游戏镜像。

## 特性

- 单个 `nes.h` 包含模拟器公开接口与完整实现；
- 采用 C11，平台无关的模拟器逻辑不依赖 SDL；
- 编译期配置画面格式、声音、文件系统、跳帧和 Mapper 范围；
- 支持从内存数组加载 ROM，适合将游戏存放在 MCU Flash；
- 平台只需实现初始化、绘图、声音和每帧处理等少量回调；
- 提供 SDL2 桌面验证程序及 ROM 转 C 数组工具；
- 可从主工程源码重新生成，避免手工维护合并后的大文件。

## 目录结构

```text
Peanut-NES/
├── nes.h                         单头文件模拟器
├── README.md                     本文档
├── tests/
│   ├── single_header_compile.c   最小嵌入式移植模板
│   ├── nes_sdl.c                 SDL2 桌面运行示例
│   ├── size_check.c              静态内存占用检查
│   └── game.h                    ROM 数组头文件
└── tools/
    ├── generate_single_header.py 从主工程生成 nes.h
    └── rom_to_header.py          将 .nes ROM 转换为 game.h
```

## 快速集成

在且仅在一个 `.c` 文件中定义 `NES_IMPLEMENTATION`：

```c
#define NES_USE_FS 0
#define NES_ENABLE_SOUND 0
#define NES_COLOR_DEPTH 16
#define NES_RENDER_LINE 1

#define NES_IMPLEMENTATION
#include "nes.h"
```

其他翻译单元只包含声明：

```c
#include "nes.h"
```

不要在多个源文件中定义 `NES_IMPLEMENTATION`，否则链接时会出现重复符号。
所有影响数据结构的配置宏也应在整个工程中保持一致。

## 平台移植接口

应用需要实现以下接口：

```c
int nes_initex(nes_t *nes);
int nes_deinitex(nes_t *nes);
int nes_draw(int x1, int y1, int x2, int y2, nes_color_t *pixels);
void nes_frame(nes_t *nes);
int nes_sound_output(uint8_t *buffer, size_t len);
```

各接口的典型职责：

- `nes_initex()`：初始化 LCD、按键、定时器、音频和存储设备；
- `nes_deinitex()`：释放平台资源；
- `nes_draw()`：把指定矩形的像素发送到 LCD 或帧缓冲区；
- `nes_frame()`：扫描按键、进行 60 Hz 帧同步并处理退出条件；
- `nes_sound_output()`：把音频数据送入 DAC、I2S 或 DMA。

### 逐行渲染（低内存平台推荐）

默认 `NES_RENDER_LINE=0`，继续使用原有整帧或半帧缓冲和 `nes_draw()`，因此现有
平台代码无需修改。

STM32F103 等 RAM 较小的平台应启用：

```c
#define NES_COLOR_DEPTH 16
#define NES_RENDER_LINE 1
#define NES_IMPLEMENTATION
#include "nes.h"
```

启用后，模拟器不再申请 `256 × 240` 帧缓冲，只保留一行 `256` 像素：

- RGB565：扫描线缓冲为 512 字节；
- ARGB8888：扫描线缓冲为 1024 字节。

定义并注册逐行回调：

```c
static void lcd_draw_line(nes_t *nes,
                          const nes_color_t pixels[NES_WIDTH],
                          uint16_t line)
{
    (void)nes;

    /* 示例：设置 LCD 写入窗口，然后立即发送这一行。 */
    lcd_set_window(0, line, NES_WIDTH - 1, line);
    lcd_write_pixels(pixels, NES_WIDTH);
}

int main(void)
{
    nes_t *nes = nes_init();
    nes->nes_draw_line = lcd_draw_line;

    /* 加载 ROM 后运行…… */
}
```

`pixels` 已经完成背景和精灵混合，只在回调执行期间有效；回调返回后同一缓冲区会被
下一条扫描线复用。如果使用 DMA，必须在返回前等待传输完成，或者把该行复制到由
平台管理的双缓冲中。逐行模式下不会调用 `nes_draw()`，但该接口仍保留以兼容原有
整屏模式。

按键状态保存在：

```c
nes->nes_cpu.joypad.A1;
nes->nes_cpu.joypad.B1;
nes->nes_cpu.joypad.U1;
nes->nes_cpu.joypad.D1;
nes->nes_cpu.joypad.L1;
nes->nes_cpu.joypad.R1;
nes->nes_cpu.joypad.ST1;
nes->nes_cpu.joypad.SE1;
```

需要结束模拟时设置：

```c
nes->nes_quit = 1;
```

## 将 ROM 转换为 `game.h`

进入 `Peanut-NES/tests`：

```bash
python3 ../tools/rom_to_header.py /path/to/game.nes game.h
```

工具会验证 iNES/NES 2.0 文件头，并生成：

```c
static const uint8_t game_rom[] = {
    /* ROM 数据 */
};

static const size_t game_rom_size = sizeof(game_rom);
```

程序随后可以直接从数组加载：

```c
#include "game.h"

nes_t *nes = nes_init();
if (nes != NULL && nes_load_rom(nes, game_rom) == NES_OK) {
    nes_run(nes);
    nes_unload_rom(nes);
}
nes_deinit(nes);
```

## 编译最小嵌入式模板

先生成真实的 `game.h`，然后执行：

```bash
cd Peanut-NES/tests
gcc -std=c11 -O2 single_header_compile.c -o nes_template
./nes_template
```

该模板故意将渲染、声音、控制和帧同步留空，用于复制到嵌入式工程后逐项实现。
未实现退出输入时，`nes_run()` 不会自行返回。

## 使用 SDL2 验证 ROM

Ubuntu/WSL 安装依赖：

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev
```

编译并运行：

```bash
cd Peanut-NES/tests

# 默认整屏渲染
gcc -std=c11 -O2 nes_sdl.c $(sdl2-config --cflags --libs) -o nes_sdl
./nes_sdl /path/to/game.nes
```

使用同一个 SDL 示例测试逐行渲染：

```bash
gcc -std=c11 -O2 -DNES_SDL_RENDER_LINE=1 \
  nes_sdl.c $(sdl2-config --cflags --libs) \
  -o nes_sdl_line

./nes_sdl_line /path/to/game.nes
```

`NES_SDL_RENDER_LINE=0` 时调用原有 `nes_draw()` 更新整帧纹理；设为 `1` 时会映射
到核心的 `NES_RENDER_LINE=1`，每生成一条扫描线便更新 SDL 纹理。窗口仍在一帧
结束后统一呈现，以避免显示撕裂。

默认按键：

| 功能 | 按键 |
|---|---|
| 方向 | 方向键或 `WASD` |
| A | `J` 或 `Z` |
| B | `K` 或 `X` |
| Select | `V` 或右 Shift |
| Start | `B` 或 Enter |
| 退出 | Esc |

## 常用配置

配置宏必须放在包含 `nes.h` 之前：

| 宏 | 默认值 | 说明 |
|---|---:|---|
| `NES_ENABLE_SOUND` | `0` | 是否编译 APU 声音输出 |
| `NES_USE_FS` | `0` | 是否启用文件加载接口 |
| `NES_USE_SRAM` | `0` | 是否使用 SRAM 存档 |
| `NES_COLOR_DEPTH` | `32` | `16` 为 RGB565，`32` 为 ARGB8888 |
| `NES_COLOR_SWAP` | `0` | RGB565 字节/通道交换 |
| `NES_RAM_LACK` | `0` | 启用低内存绘制模式 |
| `NES_RENDER_LINE` | `0` | 逐行输出，只保留一条扫描线缓冲 |
| `NES_FRAME_SKIP` | `0` | 跳帧数量 |
| `NES_ROM_STREAM` | `0` | 从文件流式读取 ROM Bank |
| `NES_ENABLE_HEAVY_MAPPERS` | `0` | 启用内存占用较大的 Mapper |
| `NES_ENABLE_PLANE1_MAPPERS` | `0` | 启用 NES 2.0 Mapper 256～511 |
| `NES_ENABLE_PLANE2_MAPPERS` | `0` | 启用 NES 2.0 Mapper 512～767 |

面向 STM32 的起始配置示例：

```c
#define NES_USE_FS 0
#define NES_ENABLE_SOUND 0
#define NES_COLOR_DEPTH 16
#define NES_RENDER_LINE 1
#define NES_FRAME_SKIP 0
#define NES_ENABLE_HEAVY_MAPPERS 0
```

实际 RAM 和 Flash 占用会受到 ROM 大小、画面模式、声音和 Mapper 的明显影响，
应结合目标芯片的链接映射文件进行确认。

可以在桌面环境快速比较 `nes_t` 的静态内存占用：

```bash
cd Peanut-NES/tests

# RGB565 逐行模式
gcc -std=c11 -DNES_COLOR_DEPTH=16 -DNES_RENDER_LINE=1 size_check.c -o size_line
./size_line

# RGB565 原整帧模式
gcc -std=c11 -DNES_COLOR_DEPTH=16 size_check.c -o size_frame
./size_frame
```

## 从主工程重新生成 `nes.h`

生成器可以分别指定原始项目根目录和输出文件：

```bash
python3 tools/generate_single_header.py \
  --input /path/to/original/nes \
  --output /path/to/Peanut-NES/nes.h \
  --mappers all
```

在当前仓库布局中，可从工程根目录执行：

```bash
python3 Peanut-NES/tools/generate_single_header.py \
  --input . \
  --output Peanut-NES/nes.h
```

默认会包含所有已有 Mapper。资源受限的平台可以只生成游戏实际需要的 Mapper：

`--mappers all`（以及不提供 `--mappers` 参数）会完整保留原始
`src/nes_mapper.c`，包括全部前置声明和调度逻辑，生成结果与未加入筛选功能前一致。

```bash
python3 Peanut-NES/tools/generate_single_header.py \
  --input . \
  --output Peanut-NES/nes.h \
  --mappers 0,1,2,3,4,7
```

Mapper 参数支持单个编号、逗号列表和闭区间，三种写法可以组合：

```bash
# 只包含 Mapper 0
--mappers 0

# 包含 Mapper 0、1、2、4
--mappers 0,1,2,4

# 包含 Mapper 0～4、7、10～25
--mappers 0-4,7,10-25
```

短参数形式为 `-m`：

```bash
python3 Peanut-NES/tools/generate_single_header.py -i . -o Peanut-NES/nes.h -m 0,1,2,4
```

生成器会同时过滤 Mapper 实现和调度器中的加载分支。若 ROM 使用了未包含的
Mapper，`nes_load_rom()` 会报告该 Mapper 不受支持。传入不存在的 Mapper 编号时，
生成器会直接报错，不会产生不完整的头文件。

查看完整参数：

```bash
python3 Peanut-NES/tools/generate_single_header.py --help
```

生成器只对原源码执行机械合并，并为原本分属不同 `.c` 文件的 Mapper 私有
`static` 符号添加编号前缀，避免单翻译单元中的重名冲突。

## 许可证

Peanut-NES 沿用主工程的 Apache License 2.0，详见
[`../LICENSE`](../LICENSE)。
