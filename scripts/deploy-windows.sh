#!/usr/bin/env bash
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

set -Eeuo pipefail

# NOTE: 内部打包入口，由 build-windows.sh 传入 Qt 前缀、构建目录、缓存目录和 OpenSSL 目录。
[[ $# == 4 ]] || { printf '用法：%s <Windows Qt 前缀> <构建目录> <缓存目录> <OpenSSL 目录>\n' "$0" >&2; exit 2; }
qt_windows=$1
build_dir=$2
cache_dir=$3
openssl_bin_dir=$4
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
package_name=gs-232b-open-gui-windows-x86_64
staging_dir=$(mktemp -d "$build_dir/.windows-package.XXXXXX")
trap 'rm -rf -- "$staging_dir"' EXIT
package_dir=$staging_dir/$package_name
mkdir -p -- "$package_dir"
cp -- "$build_dir/gs_232b_open_gui.exe" "$package_dir/"

# NOTE: 仅部署所选 MSVC 的可再分发 CRT，禁止从 Wine 系统目录或其他编译器补 DLL。
[[ -n ${MSVC_ROOT:-} ]] || { printf '缺少 MSVC_ROOT。\n' >&2; exit 1; }
redist_dir=${MSVC_REDIST_DIR:-}
if [[ -z $redist_dir ]]; then
    redist_dir=$(python3 - "$MSVC_ROOT" <<'PY'
import pathlib
import sys

folders = list(pathlib.Path(sys.argv[1]).glob("VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT"))
if folders:
    print(max(folders, key=lambda folder: tuple(int(part) for part in folder.parts[-3].split("."))))
PY
)
fi
for dll in msvcp140.dll vcruntime140.dll vcruntime140_1.dll; do
    [[ -f $redist_dir/$dll ]] || { printf '缺少 MSVC x64 CRT：%s；请检查 MSVC_REDIST_DIR。\n' "$dll" >&2; exit 1; }
done
cp -- "$redist_dir/"*.dll "$package_dir/"

for dll in libssl-1_1-x64.dll libcrypto-1_1-x64.dll; do
    [[ -f $openssl_bin_dir/$dll ]] || { printf '缺少 OpenSSL x64 运行库：%s；请检查 OPENSSL_BIN_DIR。\n' "$dll" >&2; exit 1; }
    cp -- "$openssl_bin_dir/$dll" "$package_dir/"
done
[[ -f $openssl_bin_dir/LICENSE ]] || { printf '缺少 OpenSSL LICENSE；请检查 OPENSSL_BIN_DIR。\n' >&2; exit 1; }

# NOTE: 部署工具也使用应用本地 CRT；Wine 的 system32 占位 DLL 会先于 PATH 被搜索。
# NOTE: 临时工具目录中的 qt.conf 指回所选 SDK，不改写自备 SDK，也不依赖注册表中的 Qt。
tools_dir=$staging_dir/deploy-tools
mkdir -p -- "$tools_dir"
cp -- "$qt_windows/bin/windeployqt.exe" "$qt_windows/bin/qmake.exe" \
    "$qt_windows/bin/Qt5Core.dll" "$tools_dir/"
cp -- "$redist_dir/"*.dll "$tools_dir/"

# NOTE: 独立 Wine 前缀供编译和部署使用，不改动用户已有的 Wine 环境。
export WINEPREFIX=$cache_dir/wine-msvc
export WINEDEBUG=-all
export WINEDLLOVERRIDES='mscoree,mshtml=;vcruntime140,vcruntime140_1,msvcp140,msvcp140_1=n'
package_windows=$(winepath -w "$package_dir")
qt_bin_windows=$(winepath -w "$qt_windows/bin")
qt_prefix_windows=$(winepath -w "$qt_windows")
tools_windows=$(winepath -w "$tools_dir")
printf '[Paths]\nPrefix=%s\n' "${qt_prefix_windows//\\//}" > "$tools_dir/qt.conf"
export WINEPATH="$tools_windows;$qt_bin_windows"
wine "$tools_dir/windeployqt.exe" \
    --release --no-compiler-runtime \
    --no-opengl-sw --no-system-d3d-compiler --no-translations \
    --dir "$package_windows" "$package_windows\\gs_232b_open_gui.exe"

cat > "$package_dir/qt.conf" <<'EOF'
[Paths]
Prefix=.
Plugins=.
Translations=translations
EOF
# NOTE: Qt 5.15.2 官方简体中文目录使用 qt 名称；其他 Qt 5.15 发行包可能提供 qtbase。
mkdir -p -- "$package_dir/translations"
qt_translation=$qt_windows/translations/qtbase_zh_CN.qm
if [[ ! -f $qt_translation ]]; then
    qt_translation=$qt_windows/translations/qt_zh_CN.qm
fi
cp -- "$qt_translation" "$package_dir/translations/"
cp -- "$project_dir/LICENSE" "$package_dir/"
cp -R -- "$build_dir/licenses" "$package_dir/"
cp -- "$openssl_bin_dir/LICENSE" "$package_dir/licenses/OpenSSL-LICENSE.txt"
for license_dir in LICENSES licenses; do
    if [[ -d $qt_windows/$license_dir ]]; then
        cp -R -- "$qt_windows/$license_dir" "$package_dir/qt-licenses"
        break
    fi
done

python3 "$project_dir/scripts/verify-windows-runtime.py" "$package_dir"

python3 - "$staging_dir" "$package_name" "$build_dir" <<'PY'
import pathlib
import shutil
import sys

staging, name, output = sys.argv[1:]
shutil.make_archive(str(pathlib.Path(output) / name), "zip", staging, name)
PY
# NOTE: 仅替换固定的生成目录；打包失败时保留上一次成功的 package。
rm -rf -- "$build_dir/package"
mv -- "$package_dir" "$build_dir/package"
printf '\nWindows 程序目录：%s/package\nWindows ZIP 包：%s/%s.zip\n' "$build_dir" "$build_dir" "$package_name"
