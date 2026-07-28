"""从现有多文件源码机械生成根目录的单头文件 nes.h。"""

from pathlib import Path
import argparse
import re


HEADERS = [
    "inc/nes_default.h",
    "inc/nes_log.h",
    "inc/nes_rom.h",
    "inc/nes_cpu.h",
    "inc/nes_ppu.h",
    "inc/nes_apu.h",
    "inc/nes_mapper.h",
    "inc/nes.h",
]
CORE_SOURCES = [
    "src/nes_default.c",
    "src/nes_rom.c",
    "src/nes_cpu.c",
    "src/nes_ppu.c",
    "src/nes_apu.c",
    "src/nes.c",
]


def read_text(input_root: Path, relative: str) -> str:
    return (input_root / relative).read_text(encoding="utf-8-sig")


def strip_local_includes(text: str) -> str:
    """删除已被单头文件吸收的本地 include，保留标准库 include。"""
    text = re.sub(r'^\s*#\s*include\s+"nes(?:_[^"]+)?\.h"\s*$', "", text, flags=re.M)
    text = re.sub(r"^\s*#\s*pragma\s+once\s*$", "", text, flags=re.M)
    return text.strip()


def section(title: str, relative: str, body: str) -> str:
    return (
        "\n\n/* ========================================================================== */\n"
        f"/* {title}（原文件：{relative}） */\n"
        "/* ========================================================================== */\n"
        f'#line 1 "{relative}"\n{body}\n'
    )


def namespace_mapper_statics(body: str, mapper_id: str) -> str:
    """为原先分属不同翻译单元的 static 函数加预处理别名，避免合并后重名。"""
    names = set(re.findall(
        r"(?m)^\s*static\s+(?:inline\s+)?(?:const\s+)?"
        r"(?:[A-Za-z_]\w*\s+|\*\s*)+([A-Za-z_]\w*)\s*\(",
        body,
    ))
    for line in body.splitlines():
        if re.match(r"^\s*static\b", line) and "(" not in line:
            match = re.search(r"\b([A-Za-z_]\w*)\s*(?:\[|=|;)", line)
            if match:
                names.add(match.group(1))
    names = sorted(names)
    if not names:
        return body
    aliases = "\n".join(f"#define {name} nes_mapper{mapper_id}_local_{name}" for name in names)
    undefs = "\n".join(f"#undef {name}" for name in names)
    return f"{aliases}\n\n{body}\n\n{undefs}"


def parse_args() -> argparse.Namespace:
    script_path = Path(__file__).resolve()
    default_input = script_path.parents[2]
    default_output = script_path.parents[1] / "nes.h"

    parser = argparse.ArgumentParser(
        description="将 NES 多文件源码合并为一个 nes.h"
    )
    parser.add_argument(
        "-i", "--input",
        type=Path,
        default=default_input,
        metavar="PROJECT_DIR",
        help=f"原始项目根目录，其中应包含 inc 和 src（默认：{default_input}）",
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        default=default_output,
        metavar="NES_H",
        help=f"nes.h 输出路径（默认：{default_output}）",
    )
    return parser.parse_args()


def validate_input_root(input_root: Path) -> None:
    required = [*HEADERS, *CORE_SOURCES, "src/nes_mapper.c"]
    missing = [relative for relative in required if not (input_root / relative).is_file()]
    if not (input_root / "src/nes_mapper").is_dir():
        missing.append("src/nes_mapper/")
    if missing:
        raise SystemExit(
            f"错误：输入路径不是完整的 NES 源码项目：{input_root}\n"
            + "缺少：\n  "
            + "\n  ".join(missing)
        )


def main() -> None:
    args = parse_args()
    input_root = args.input.expanduser().resolve()
    output_path = args.output.expanduser().resolve()
    validate_input_root(input_root)

    declaration_parts = []
    header_titles = {
        "inc/nes_default.h": "基础配置与平台抽象接口",
        "inc/nes_log.h": "日志模块",
        "inc/nes_rom.h": "ROM 数据结构",
        "inc/nes_cpu.h": "CPU 模块",
        "inc/nes_ppu.h": "PPU 模块",
        "inc/nes_apu.h": "APU 音频模块",
        "inc/nes_mapper.h": "Mapper 公共接口",
        "inc/nes.h": "模拟器公共接口",
    }
    for relative in HEADERS:
        declaration_parts.append(
            section(
                header_titles[relative],
                relative,
                strip_local_includes(read_text(input_root, relative)),
            )
        )

    implementation_parts = []
    source_titles = {
        "src/nes_default.c": "默认平台适配实现",
        "src/nes_rom.c": "ROM 加载实现",
        "src/nes_cpu.c": "CPU 实现",
        "src/nes_ppu.c": "PPU 实现",
        "src/nes_apu.c": "APU 实现",
        "src/nes.c": "模拟器主循环与渲染实现",
        "src/nes_mapper.c": "Mapper 调度实现",
    }
    for relative in CORE_SOURCES:
        implementation_parts.append(
            section(
                source_titles[relative],
                relative,
                strip_local_includes(read_text(input_root, relative)),
            )
        )

    for path in sorted((input_root / "src/nes_mapper").glob("nes_mapper*.c"),
                       key=lambda p: int(re.search(r"(\d+)$", p.stem).group(1))):
        relative = path.relative_to(input_root).as_posix()
        mapper_id = re.search(r"(\d+)$", path.stem).group(1)
        mapper_body = strip_local_includes(read_text(input_root, relative))
        mapper_body = namespace_mapper_statics(mapper_body, mapper_id)
        implementation_parts.append(
            section(f"Mapper {mapper_id} 实现", relative, mapper_body)
        )

    implementation_parts.append(
        section("Mapper 调度实现", "src/nes_mapper.c",
                strip_local_includes(read_text(input_root, "src/nes_mapper.c")))
    )

    output = """/*
 * NES 单头文件版本
 *
 * 本文件由 tools/generate_single_header.py 从原始源码机械合并生成。
 * 原项目采用 Apache License 2.0，具体许可内容见仓库根目录 LICENSE。
 *
 * 使用方法：
 *   1. 在且仅在一个 C/C++ 源文件中定义 NES_IMPLEMENTATION 后包含本文件；
 *   2. 其他源文件直接包含本文件；
 *   3. 在包含前定义 NES_ENABLE_SOUND、NES_USE_FS、NES_COLOR_DEPTH 等配置；
 *   4. 平台可自行实现 nes_initex、nes_deinitex、nes_draw、nes_frame 和声音接口。
 */

#ifndef NES_SINGLE_HEADER_INCLUDED
#define NES_SINGLE_HEADER_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* 画面尺寸需要先定义，后续配置会据此计算帧缓冲区大小。 */
#ifndef NES_WIDTH
#define NES_WIDTH 256
#endif
#ifndef NES_HEIGHT
#define NES_HEIGHT 240
#endif

/* 原工程由 nes_conf.h 提供、但基础头未给默认值的配置。 */
#ifndef NES_USE_SRAM
#define NES_USE_SRAM 0
#endif

/* 未接入日志后端时静默输出；平台可在包含本文件前覆盖。 */
#ifndef nes_log_printf
#define nes_log_printf(...) ((void)0)
#endif

""" + "".join(declaration_parts) + """

#endif /* NES_SINGLE_HEADER_INCLUDED */

/*
 * 实现区：与 stb、Peanut-GB 类似，只允许在一个翻译单元中启用。
 */
#if defined(NES_IMPLEMENTATION) && !defined(NES_IMPLEMENTATION_INCLUDED)
#define NES_IMPLEMENTATION_INCLUDED

""" + "".join(implementation_parts) + """

#endif /* NES_IMPLEMENTATION */
"""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(output, encoding="utf-8", newline="\n")
    print(f"输入项目：{input_root}")
    print(f"输出文件：{output_path}")
    print(f"文件大小：{output_path.stat().st_size} 字节")


if __name__ == "__main__":
    main()
