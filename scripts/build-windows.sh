#!/usr/bin/env bash
# GS-232B Open Control - rotator controller GUI.
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

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)

usage() {
    cat <<'EOF'
用法：./scripts/build-windows.sh

在 Linux x86_64 上通过 Wine/MSVC 编译并打包 Windows x64 Release 版本。
首次运行下载 Qt 5.15.2 和 MSVC/Windows SDK，之后复用缓存。
MSVC 首次安装需要接受微软许可；不需要 Windows 或管理员权限。

可选环境变量：
  JOBS                  编译并行数（默认 2）
  BUILD_DIR             构建与打包目录（默认 <项目>/build/windows-qt5-msvc-x86_64）
  WINDOWS_BUILD_CACHE   SDK 和工具缓存（默认 $XDG_CACHE_HOME/gs232b-windows，
                        未设置 XDG_CACHE_HOME 时使用 ~/.cache）
  QT_MIRROR             Qt 下载镜像根地址（默认 https://mirrors.aliyun.com/qt）
  QT_HOST_PATH          已安装的 Linux Qt 5.15.2 前缀（需 moc、rcc、uic、lrelease、lupdate）
  QT_WINDOWS_PREFIX     已安装的 Windows Qt 5.15.2 msvc2019_64 前缀
  OPENSSL_BIN_DIR       OpenSSL 1.1.1 x64 DLL 目录（默认自动下载 1.1.1w）
  MSVC_ROOT             已通过 msvc-wine 安装的 MSVC 前缀
  MSVC_REDIST_DIR       配套 x64 Microsoft.VC143.CRT 目录（默认从 MSVC_ROOT 探测）
  MSVC_ACCEPT_LICENSE   设为 1 表示已阅读并接受微软许可，允许非交互安装

依赖与输出说明：docs/build-windows.md
EOF
}

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    usage
    exit 0
fi
if (( $# != 0 )); then
    usage >&2
    exit 2
fi

fail() { printf '错误：%s\n' "$*" >&2; exit 1; }
trap 'printf "Windows 构建失败（第 %s 行）。\n" "$LINENO" >&2' ERR

[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || fail '此脚本支持 Linux x86_64 主机。'
for command_name in cmake ninja python3 curl sha256sum wine winepath objdump; do
    command -v "$command_name" >/dev/null || fail "缺少 $command_name；请参阅 docs/build-windows.md。"
done

qt_version=5.15.2
aqt_version=3.3.0
openssl_version=1.1.1w
openssl_sha256=89f22bcffe61279300c69a8b03a3c7ef9f5da179acb64f1b95e870217da0098c
openssl_url=https://wiki.overbyte.eu/arch/openssl-1.1.1w-win64.zip
cache_dir=${WINDOWS_BUILD_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/gs232b-windows}
build_dir=${BUILD_DIR:-$project_dir/build/windows-qt5-msvc-x86_64}
jobs=${JOBS:-2}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || fail 'JOBS 必须是正整数。'
mkdir -p -- "$cache_dir" "$build_dir"
cache_dir=$(cd -- "$cache_dir" && pwd)
build_dir=$(cd -- "$build_dir" && pwd)

# NOTE: 更换编译器或 Qt 主版本必须使用新目录；下载 SDK 前先阻止误用已有缓存。
if [[ -f $build_dir/CMakeCache.txt ]]; then
    cached_toolchain=$(sed -n 's/^CMAKE_TOOLCHAIN_FILE:[^=]*=//p' "$build_dir/CMakeCache.txt")
    [[ $cached_toolchain == "$project_dir/cmake/windows-msvc.cmake" ]] || \
        fail 'BUILD_DIR 已含其他工具链缓存；请使用新的构建目录。'
    if grep -qE '^Qt6[^:]*_DIR:' "$build_dir/CMakeCache.txt"; then
        fail 'BUILD_DIR 已含 Qt 6 缓存；请使用新的构建目录。'
    fi
fi

qt_dir=$cache_dir/Qt
qt_host=${QT_HOST_PATH:-$qt_dir/$qt_version/gcc_64}
qt_windows=${QT_WINDOWS_PREFIX:-$qt_dir/$qt_version/msvc2019_64}
msvc_root=${MSVC_ROOT:-$cache_dir/msvc2022}
openssl_cache_dir=$cache_dir/openssl-$openssl_version
openssl_bin_dir=${OPENSSL_BIN_DIR:-$openssl_cache_dir}

# NOTE: 编译、部署工具共用独立 Wine 前缀，不受用户原有 Wine 环境影响。
export WINEPREFIX=$cache_dir/wine-msvc
export WINEDEBUG=-all
export WINEDLLOVERRIDES='mscoree,mshtml='
if [[ -z ${MSVC_ROOT:-} && ! -f $msvc_root/.gs232b-msvc-ready ]]; then
    "$project_dir/scripts/prepare-windows-msvc.sh" "$cache_dir" "$msvc_root"
fi
for tool in cl link lib rc mt; do
    [[ -x $msvc_root/bin/x64/$tool ]] || fail "MSVC 缺少 $tool 包装器：$msvc_root"
done
msvc_root=$(cd -- "$msvc_root" && pwd)
export MSVC_ROOT=$msvc_root
export PATH="$msvc_root/bin/x64:$PATH"

prepare_openssl() {
    if [[ -n ${OPENSSL_BIN_DIR:-} ]]; then
        openssl_bin_dir=$(cd -- "$openssl_bin_dir" && pwd)
    else
        local archive_dir=$cache_dir/downloads
        local archive=$archive_dir/openssl-$openssl_version-win64.zip
        mkdir -p -- "$archive_dir"
        if [[ ! -f $archive ]]; then
            curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 \
                --output "$archive.part" "$openssl_url"
            mv -- "$archive.part" "$archive"
        fi
        printf '%s  %s\n' "$openssl_sha256" "$archive" | sha256sum -c -
        if [[ ! -f $openssl_cache_dir/libssl-1_1-x64.dll ||
              ! -f $openssl_cache_dir/libcrypto-1_1-x64.dll ||
              ! -f $openssl_cache_dir/LICENSE ]]; then
            mkdir -p -- "$openssl_cache_dir"
            python3 - "$archive" "$openssl_cache_dir" <<'PY'
import pathlib
import sys
import zipfile

archive, output = map(pathlib.Path, sys.argv[1:])
members = ("libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll", "LICENSE")
with zipfile.ZipFile(archive) as source:
    missing = [member for member in members if member not in source.namelist()]
    if missing:
        raise SystemExit(f"OpenSSL archive is missing: {', '.join(missing)}")
    for member in members:
        (output / member).write_bytes(source.read(member))
PY
        fi
        openssl_bin_dir=$(cd -- "$openssl_cache_dir" && pwd)
    fi
    for dll in libssl-1_1-x64.dll libcrypto-1_1-x64.dll; do
        [[ -f $openssl_bin_dir/$dll ]] || fail "OpenSSL 缺少 $dll：$openssl_bin_dir"
    done
    [[ -f $openssl_bin_dir/LICENSE ]] || fail "OpenSSL 缺少 LICENSE：$openssl_bin_dir"
}

prepare_openssl
export OPENSSL_BIN_DIR=$openssl_bin_dir

prepare_downloader() {
    local venv_dir=$cache_dir/aqt-venv
    if [[ ! -x $venv_dir/bin/python ]]; then
        python3 -m venv "$venv_dir"
    fi
    aqt_python=$venv_dir/bin/python
    if ! "$aqt_python" -c 'import importlib.metadata, sys; sys.exit(importlib.metadata.version("aqtinstall") != sys.argv[1])' \
        "$aqt_version" 2>/dev/null; then
        "$aqt_python" -m pip install --disable-pip-version-check "aqtinstall==$aqt_version"
    fi
}

install_qt() {
    local host=$1 arch=$2 prefix=$3
    shift 3
    prepare_downloader
    "$aqt_python" -m aqt install-qt "$host" desktop "$qt_version" "$arch" \
        --outputdir "$qt_dir" --base "${QT_MIRROR:-https://mirrors.aliyun.com/qt}" "$@"
    touch -- "$prefix/.gs232b-sdk-ready"
}

# NOTE: Linux 工具使用 SDK 配套的 ICU；Arch 系统 ICU 的主版本可能不同。
if [[ -z ${QT_HOST_PATH:-} && ( ! -f $qt_host/.gs232b-sdk-ready || ! -f $qt_host/lib/libicui18n.so.56 ) ]]; then
    install_qt linux gcc_64 "$qt_host" --archives qtbase qttools icu
fi
if [[ -z ${QT_WINDOWS_PREFIX:-} && ( ! -f $qt_windows/.gs232b-sdk-ready || ! -f $qt_windows/lib/Qt5Pdf.lib ) ]]; then
    install_qt windows win64_msvc2019_64 "$qt_windows" --archives qtbase qttools qttranslations \
        qtserialport qtwebsockets --modules qtwebengine
fi

for tool in qmake moc rcc uic lrelease lupdate; do
    [[ -x $qt_host/bin/$tool ]] || fail "Linux Qt 缺少 $tool：$qt_host"
done
for config in Qt5Core Qt5Widgets Qt5Network Qt5SerialPort Qt5WebSockets Qt5LinguistTools; do
    [[ -f $qt_windows/lib/cmake/$config/${config}Config.cmake ]] || fail "Windows Qt 缺少 $config：$qt_windows"
done
for tool in windeployqt qmake; do
    [[ -f $qt_windows/bin/$tool.exe ]] || fail "Windows Qt 缺少 $tool.exe：$qt_windows"
done
[[ -f $qt_windows/lib/Qt5Core.lib ]] || fail 'Windows Qt 必须是 msvc2019_64 SDK，不能使用 MinGW SDK。'
[[ -f $qt_windows/lib/Qt5Pdf.lib && -f $qt_windows/bin/Qt5Pdf.dll ]] || fail 'Windows Qt 缺少 QtPdf；请安装 qtwebengine 模块。'
grep -Fq "set(PACKAGE_VERSION $qt_version)" \
    "$qt_windows/lib/cmake/Qt5Core/Qt5CoreConfigVersion.cmake" || fail "Windows Qt 必须为 $qt_version。"
qt_host=$(cd -- "$qt_host" && pwd)
qt_windows=$(cd -- "$qt_windows" && pwd)
[[ $("$qt_host/bin/qmake" -query QT_VERSION) == "$qt_version" ]] || fail "Linux Qt 必须为 $qt_version，且构建工具必须能正常运行。"
"$qt_host/bin/lrelease" -version
python3 "$project_dir/scripts/verify-windows-runtime.py" "$qt_windows/bin/Qt5Core.dll" \
    "$qt_windows/plugins/platforms/qwindows.dll"

printf '配置 Windows x64 MSVC Release：%s\n' "$build_dir"
cmake -S "$project_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH="$project_dir/cmake/windows-msvc.cmake" \
    -DMSVC_ROOT="$msvc_root" \
    -DQT_WINDOWS_PREFIX="$qt_windows" \
    -DCMAKE_PREFIX_PATH="$qt_windows" \
    -DQT_HOST_PATH="$qt_host" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_WIN32_EXECUTABLE=ON \
    -DBUILD_TESTING=OFF
cmake --build "$build_dir" --target gs_232b_open_gui --parallel "$jobs"
python3 "$project_dir/scripts/verify-windows-runtime.py" "$build_dir/gs_232b_open_gui.exe"

"$project_dir/scripts/deploy-windows.sh" "$qt_windows" "$build_dir" "$cache_dir" "$openssl_bin_dir"
