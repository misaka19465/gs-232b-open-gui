# 编译 macOS ARM64 版本

## GitHub Actions

`.github/workflows/macos-arm64.yml` 在推送、Pull Request 和手动运行时构建 macOS ARM64
Release。工作流使用 GitHub `macos-15` ARM runner、Apple Clang、CMake、Ninja 和 Qt 6.8.3
`clang_64` SDK；CMake 目标架构固定为 `arm64`，最低部署版本为 macOS 12。Qt 官方支持 Qt 6.8
在 macOS 12 或更新版本运行，并支持 `arm64`。

成功后可从 Actions 运行页下载 `gs-232b-open-gui-macos-arm64` artifact，其中包含自包含的
`GS-232B Open GUI.app` 和 ZIP。`macdeployqt` 部署 Qt Framework、Cocoa 平台插件和 Qt 运行时，
应用包还包含简体中文 Qt 翻译、项目更新日志和依赖许可证。应用包未经 Apple Developer ID
签名或公证；固件升级功能仍要求 `esptool` 可从应用启动时的 `PATH` 找到。

### 缓存与清理

- `.venv` 和 uv 下载缓存一起保存；缓存命中后直接使用虚拟环境，跳过 Python 工具安装。
- Qt 6.8.3 SDK 使用独立缓存，命中后跳过 Qt 下载。
- 完整构建目录缓存 anytermqt、utf8proc、CMake/Ninja 状态、ARM64 编译文件、应用、ccache
  工具和对象缓存，以及打包目录和 ZIP。
- 缓存按 Apple Clang、Xcode/macOS SDK、Qt、CMake、Ninja、部署目标、依赖和源码内容隔离。
  源码及工具链匹配时复用已编译程序；源码变化时从兼容缓存继续增量构建。
- 编译成果根据 `.o`、`.a`、动态库和应用可执行文件内容计算指纹；编译内容变化才上传新缓存，
  时间戳、日志或重新打包变化不会产生重复缓存。
- 只在默认分支保存缓存。每类缓存保留最新两份并立即删除第三份及更早的条目，不使用天数门槛。

### 可选的自动 Release

在 **Actions → macOS ARM64 → Run workflow** 中，`publish_release` 默认关闭。
勾选后，只有编译和打包成功才发布；版本来自 `CMakeLists.txt`，Release 正文来自
`CHANGELOG.md` 中对应版本章节，附件包含带版本号的 ZIP 和 SHA256 文件。

Windows 和 macOS 工作流可以把不同平台的 ZIP 加到同一版本 Release。若该版本 Release 已由
另一平台创建，后续平台运行要求版本标签指向相同提交；重复上传同名资产会失败，不覆盖已有文件。
需要重新发布同一平台时请递增 CMake 版本，并为新版本补充 changelog。

## 本地 Apple Silicon 构建

需要 Xcode 命令行工具、CMake、Ninja、Python 3.12 和 `uv`。以下命令使用项目锁定的 Qt 版本与模块：

```sh
uv venv --python 3.12 .venv
uv pip sync --python .venv/bin/python .github/requirements-macos-ci.txt

QT_INSTALL_DIR="$PWD/.ci-cache/Qt"
.venv/bin/python -m aqt install-qt mac desktop 6.8.3 clang_64 \
  --outputdir "$QT_INSTALL_DIR" \
  --archives qtbase qttools qttranslations \
  --modules qtserialport qtwebsockets qtpdf

export QT_ROOT_DIR="$QT_INSTALL_DIR/6.8.3/macos"
export PATH="$QT_ROOT_DIR/bin:$PATH"
export BUILD_DIR="$PWD/build/macos-arm64"
cmake -S . -B "$BUILD_DIR" -G Ninja \
  "-DCMAKE_PREFIX_PATH=$QT_ROOT_DIR" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$BUILD_DIR" --target gs_232b_open_gui --parallel 2
./scripts/deploy-macos.sh
```

最终输出位于 `build/macos-arm64/GS-232B Open GUI.app`（`package/` 下）和
`build/macos-arm64/gs-232b-open-gui-macos-arm64.zip`。当前工作流生成未签名的 ARM64 包，
不执行 Apple Developer ID 签名或公证。
