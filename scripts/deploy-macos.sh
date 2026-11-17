#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
build_dir="${BUILD_DIR:-$project_dir/build/macos-arm64}"
qt_root="${QT_ROOT_DIR:-}"
build_dir="$(cd -- "$build_dir" && pwd -P)"

if [[ -z "$qt_root" || ! -x "$qt_root/bin/macdeployqt" ]]; then
    echo "QT_ROOT_DIR must point to the Qt macOS SDK containing bin/macdeployqt" >&2
    exit 1
fi
qt_root="$(cd -- "$qt_root" && pwd -P)"

source_app="$build_dir/gs_232b_open_gui.app"
executable="$source_app/Contents/MacOS/gs_232b_open_gui"
for required in \
    "$executable" \
    "$qt_root/translations/qtbase_zh_CN.qm" \
    "$build_dir/licenses/anytermqt-LICENSE" \
    "$build_dir/licenses/utf8proc-LICENSE.md" \
    "$build_dir/licenses/country-files-COPYING" \
    "$project_dir/LICENSE" \
    "$project_dir/CHANGELOG.md"; do
    if [[ ! -f "$required" ]]; then
        echo "Missing deployment input: $required" >&2
        exit 1
    fi
done

architectures="$(lipo -archs "$executable")"
if [[ "$architectures" != "arm64" ]]; then
    echo "Expected an ARM64-only app executable; found: $architectures" >&2
    exit 1
fi

staging_root="$(mktemp -d "$build_dir/.macos-package.XXXXXX")"
trap 'rm -rf -- "$staging_root"' EXIT
staging_app="$staging_root/GS-232B Open GUI.app"
package_dir="$build_dir/package"
package_app="$package_dir/GS-232B Open GUI.app"
archive="$build_dir/gs-232b-open-gui-macos-arm64.zip"
staging_archive="$staging_root/gs-232b-open-gui-macos-arm64.zip"

ditto "$source_app" "$staging_app"
"$qt_root/bin/macdeployqt" "$staging_app" -verbose=2

resources="$staging_app/Contents/Resources"
translations="$resources/translations"
mkdir -p "$translations" "$resources/licenses"
cp "$qt_root/translations/qtbase_zh_CN.qm" "$translations/"
cp "$project_dir/LICENSE" "$resources/"
cp "$project_dir/CHANGELOG.md" "$resources/"
cp "$build_dir/licenses/anytermqt-LICENSE" "$build_dir/licenses/utf8proc-LICENSE.md" \
    "$build_dir/licenses/country-files-COPYING" "$resources/licenses/"

for license_dir in "$qt_root/LICENSES" "$qt_root/licenses"; do
    if [[ -d "$license_dir" ]]; then
        ditto "$license_dir" "$resources/qt-licenses"
        break
    fi
done

plutil -lint "$staging_app/Contents/Info.plist"
deployed_executable="$staging_app/Contents/MacOS/gs_232b_open_gui"
if [[ "$(lipo -archs "$deployed_executable")" != "arm64" ]]; then
    echo "Deployed executable is not ARM64-only" >&2
    exit 1
fi
for required in \
    "$staging_app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore" \
    "$staging_app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui" \
    "$staging_app/Contents/Frameworks/QtWidgets.framework/Versions/A/QtWidgets" \
    "$staging_app/Contents/Frameworks/QtNetwork.framework/Versions/A/QtNetwork" \
    "$staging_app/Contents/Frameworks/QtPdf.framework/Versions/A/QtPdf" \
    "$staging_app/Contents/Frameworks/QtSerialPort.framework/Versions/A/QtSerialPort" \
    "$staging_app/Contents/Frameworks/QtWebSockets.framework/Versions/A/QtWebSockets" \
    "$staging_app/Contents/PlugIns/platforms/libqcocoa.dylib" \
    "$translations/qtbase_zh_CN.qm" \
    "$resources/CHANGELOG.md"; do
    if [[ ! -f "$required" ]]; then
        echo "Missing deployed macOS runtime file: $required" >&2
        exit 1
    fi
done
if otool -L "$deployed_executable" | grep -F "$qt_root" >/dev/null; then
    echo "App executable still references the cached Qt SDK" >&2
    exit 1
fi

ditto -c -k --sequesterRsrc --keepParent "$staging_app" "$staging_archive"
mkdir -p "$package_dir"
rm -rf "$package_app"
mv "$staging_app" "$package_app"
mv -f "$staging_archive" "$archive"
printf 'macOS app bundle: %s\nmacOS ZIP: %s\n' "$package_app" "$archive"
