#!/usr/bin/env python3
"""在开始打包前核对标签、项目版本和发布说明。"""

import re
import sys
from pathlib import Path


def validate(tag: str, root: Path) -> Path:
    if not re.fullmatch(r"v\d+\.\d+\.\d+", tag):
        raise ValueError("版本标签必须采用 v主版本.次版本.修订号 格式")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(ClickFlow\s+VERSION\s+(\d+\.\d+\.\d+)\b", cmake)
    if not match or match.group(1) != tag[1:]:
        raise ValueError("标签版本与 CMakeLists.txt 项目版本不一致")
    notes = root / "docs" / "releases" / f"{tag}.md"
    if not notes.is_file() or not notes.read_text(encoding="utf-8").strip():
        raise ValueError(f"缺少非空的中文发布说明：docs/releases/{tag}.md")
    return notes


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("用法：python3 scripts/validate-release.py v版本号")
        validate(sys.argv[1], Path(__file__).resolve().parents[1])
    except ValueError as error:
        sys.exit(str(error))
    print("版本标签、项目版本和发布说明检查通过")
