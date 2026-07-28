"""把 .nes ROM 转换为可直接包含的 C 头文件。"""

from pathlib import Path
import argparse


def main() -> None:
    parser = argparse.ArgumentParser(description="将 NES ROM 转换为 game.h")
    parser.add_argument("rom", type=Path, help="输入的 .nes 文件")
    parser.add_argument("output", type=Path, nargs="?", default=Path("game.h"),
                        help="输出头文件，默认是当前目录的 game.h")
    args = parser.parse_args()

    data = args.rom.read_bytes()
    if len(data) < 16 or data[:4] != b"NES\x1a":
        parser.error(f"{args.rom} 不是有效的 iNES/NES 2.0 ROM")

    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")

    output = f"""/*
 * 由 tools/rom_to_header.py 从 {args.rom.name} 自动生成，请勿手工修改。
 */
#ifndef NES_EMBEDDED_GAME_H
#define NES_EMBEDDED_GAME_H

#include <stddef.h>
#include <stdint.h>

static const uint8_t game_rom[] = {{
{chr(10).join(lines)}
}};

static const size_t game_rom_size = sizeof(game_rom);

#endif /* NES_EMBEDDED_GAME_H */
"""
    args.output.write_text(output, encoding="utf-8", newline="\n")
    print(f"已生成 {args.output}：{len(data)} 字节")


if __name__ == "__main__":
    main()
