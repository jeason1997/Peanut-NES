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
gcc -std=c11 -O2 nes_sdl.c $(sdl2-config --cflags --libs) -o nes_sdl
./nes_sdl /path/to/game.nes
```

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
#define NES_RAM_LACK 1
#define NES_FRAME_SKIP 0
#define NES_ENABLE_HEAVY_MAPPERS 0
```

实际 RAM 和 Flash 占用会受到 ROM 大小、画面模式、声音和 Mapper 的明显影响，
应结合目标芯片的链接映射文件进行确认。

## 从主工程重新生成 `nes.h`

生成器可以分别指定原始项目根目录和输出文件：

```bash
python3 tools/generate_single_header.py \
  --input /path/to/original/nes \
  --output /path/to/Peanut-NES/nes.h
```

在当前仓库布局中，可从工程根目录执行：

```bash
python3 Peanut-NES/tools/generate_single_header.py \
  --input . \
  --output Peanut-NES/nes.h
```

查看完整参数：

```bash
python3 Peanut-NES/tools/generate_single_header.py --help
```

生成器只对原源码执行机械合并，并为原本分属不同 `.c` 文件的 Mapper 私有
`static` 符号添加编号前缀，避免单翻译单元中的重名冲突。

## 许可证

Peanut-NES 沿用主工程的 Apache License 2.0，详见
[`../LICENSE`](../LICENSE)。
