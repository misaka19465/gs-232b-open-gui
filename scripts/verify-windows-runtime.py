#!/usr/bin/env python3
# GS-232B Open GUI - rotator controller GUI.
# Copyright (C) 2026 BG5GEB <bg5geb@misakanet.team>
#
# SPDX-License-Identifier: GPL-3.0-only
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""检查 Windows x64 发布文件及 Qt 5 TLS 运行库，拒绝不完整发布包。"""

import argparse
import os
import pathlib
import re
import subprocess
import sys


OPENSSL_RUNTIME_DLLS = ("libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll")


def inspect_imports(path: pathlib.Path) -> set[str]:
    """读取 PE 导入表；无法读取或目标架构错误时拒绝继续打包。"""
    result = subprocess.run(
        ["objdump", "-p", str(path)], capture_output=True, text=True,
        env={**os.environ, "LC_ALL": "C"}, check=True,
    )
    if "file format pei-x86-64" not in result.stdout:
        raise ValueError(f"不是 Windows x64 PE：{path}")
    return {name.lower() for name in re.findall(r"DLL Name:\s*(\S+)", result.stdout)}


def verify(path: pathlib.Path) -> None:
    """检查一个模块的运行库；Qt 和应用本体必须显式依赖共享 MSVC 运行库。"""
    imports = inspect_imports(path)
    forbidden = sorted(name for name in imports | {path.name.lower()} if (
        name.startswith("msvcr")
        or name.startswith(("libstdc++", "libgcc", "libwinpthread"))
        or (name.startswith("msvcp") and not name.startswith("msvcp140"))
        or re.fullmatch(r"(?:msvcp|vcruntime)\d+.*d\.dll", name)
        or name == "ucrtbased.dll"
    ))
    if forbidden:
        raise ValueError(f"{path}：混入不兼容或 Debug 运行库：{', '.join(forbidden)}")
    # NOTE: 中性系统辅助 DLL 可以没有 CRT 导入；应用和 Qt 必须使用动态 MSVC CRT。
    if path.suffix.lower() == ".exe" or path.name.lower().startswith("qt5") or path.name.lower() == "qwindows.dll":
        if not any(name.startswith(("vcruntime140", "msvcp140")) for name in imports):
            raise ValueError(f"{path}：未发现动态 MSVC 运行库")
        if not any(name == "ucrtbase.dll" or name.startswith("api-ms-win-crt-") for name in imports):
            raise ValueError(f"{path}：未发现动态 UCRT")
    print(f"MSVC/UCRT x64：{path}")


def verify_directory(path: pathlib.Path) -> None:
    """递归检查发布目录，并确保 Qt Network 的 OpenSSL 运行库随包提供。"""
    files = sorted(file for file in path.rglob("*")
                   if file.is_file() and file.suffix.lower() in (".exe", ".dll"))
    if not files:
        raise ValueError(f"未找到 Windows 发布文件：{path}")
    for file in files:
        verify(file)
    if (path / "Qt5Network.dll").is_file():
        missing = [name for name in OPENSSL_RUNTIME_DLLS if not (path / name).is_file()]
        if missing:
            raise ValueError(
                f"{path}：Qt5Network.dll 需要随包提供 OpenSSL 运行库：{', '.join(missing)}"
            )


def main() -> int:
    """检查文件或递归检查目录内的所有 EXE/DLL，任一失败返回非零状态。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=pathlib.Path)
    args = parser.parse_args()
    try:
        for path in args.paths:
            if path.is_dir():
                verify_directory(path)
            else:
                verify(path)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"运行库检查失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
