# 编译 Windows 版本

## GitHub Actions：原生 Windows 构建

`.github/workflows/windows.yml` 在推送、Pull Request 或手动触发时，使用
`windows-2022`、Visual Studio 2022 MSVC、CMake 和 Ninja 构建 Windows x64 Release。
Windows CI 固定为 Qt 5.15.2 `msvc2019_64` SDK，安装 QtBase、QtTools、QtTranslations、
QtSerialPort 和 QtWebSockets，并缓存 SDK。应用使用 Visual Studio 2022 编译器构建；
Visual Studio 2022 提供的 14.29（v142）工具集与 Qt SDK 匹配。构建关闭 C++ 测试目标，
但会运行无需 SDK 的 CI 发布、缓存清理和 Windows 运行库检查回归测试。
Python 测试统一放在 `tests/pytest/`，与 `tests/` 中的 C++/Qt 测试分开保存。

在仓库 **Actions → Windows** 查看进度；也可以点击 **Run workflow** 手动构建。
成功后下载运行页面中的 `gs-232b-open-gui-windows-x86_64` artifact，保留 14 天。
artifact 内包含 `gs-232b-open-gui-windows-x86_64.zip`，解压其中的完整程序目录，
运行 `gs_232b_open_gui.exe`。ZIP 包含 Qt5 DLL、平台插件、MSVC 可再分发 DLL、
中文翻译、许可证和 `qt.conf`。

Qt 5.15 的公开 Windows SDK 没有 QtPdf 模块。Windows 版本用系统自带的
`Windows.Data.Pdf` 渲染方位地图；该 API 从 Windows 10 build 10240 开始提供。
Windows 构建因此不下载或打包 QtPdf。Linux 和 macOS 仍通过 Qt 6 构建，并使用 QtPdf。

### 缓存与自动清理

- Python 工具安装到项目内 `.venv/`，使用 `uv pip sync` 安装
  `.github/requirements-ci.txt` 中锁定的依赖。`.venv` 和 uv 下载缓存一起保存；
  缓存键包含实际 Python 补丁版本与锁文件哈希，命中后直接激活，跳过创建环境和依赖安装。
- Qt SDK 保留原有缓存键，命中后跳过 SDK 下载和缓存上传。安装步骤改为直接调用
  `.venv` 中的 `python -m aqt`，避免 Qt 安装 action 每次都运行 `pip install`。
- 完整构建目录也进入缓存：包括 anytermqt、utf8proc 的源码与编译库、对象文件、
  CMake/Ninja 状态、应用 EXE、ccache 工具和对象缓存，以及打包目录和 ZIP。
- 构建缓存使用编译器、Windows SDK、Qt、CMake、Ninja、路径及依赖声明组成兼容前缀，
  再加项目源码内容哈希。源码和工具链完全匹配时直接复用编译成果；源码变化时恢复
  兼容前缀下的上一份构建，重新配置并增量编译，第三方源码无需再次下载。
- PE 检查用的 objdump 和配套 DLL 单独缓存，命中后跳过 MSYS2 安装。
- 缓存键不含提交号、运行编号或日期。恢复缓存后和构建完成后，分别对对象文件、静态库、
  编译中间文件和 EXE/DLL 计算内容哈希；**只要编译内容变化就上传新缓存**，没有变化则跳过。
  保存键包含编译内容哈希，避免 GitHub 不可覆盖的旧键阻止更新；仅修改时间、日志变化或
  重新压缩 ZIP 不触发上传。自定义缓存仅由默认分支写入。
- 每次默认分支构建完成保存后，分别保留 `.venv`、Qt、完整构建和 PE 工具的**最新两份**缓存，
  删除第三份及更早的条目，不按天数判断。每周日北京时间 11:17 也单独执行清理。
  清理仅匹配本工作流的四类 `gs232b-windows-*` 前缀，不影响其他工作流的缓存。

每次运行仍会重新打包和检查，确保 ZIP 内的 `CHANGELOG.md` 与此次提交一致。
缓存未命中时正常下载并构建，成功后才保存缓存。

### MSVC 与 ccache

CI 使用固定版本 ccache 4.14.3，通过 `CMAKE_C_COMPILER_LAUNCHER` 和
`CMAKE_CXX_COMPILER_LAUNCHER` 包装 C/C++ 编译；应用和第三方库都使用同一套设置。
ccache 工具在首次下载时校验 SHA256，之后随完整构建目录恢复，无需重复下载。

MSVC 配置位于 `cmake/ccache-msvc.conf`：明确指定 `compiler_type=msvc`，
按编译器内容判断版本，启用 depend mode、UTF-8 和压缩，对象缓存限制为 500 MiB。
`VSLANG=1033` 固定英文诊断，与 `msvc_dep_prefix=Note: including file:` 配套；
Ninja/MSVC 的 `/showIncludes` 提供头文件依赖，ccache 在 depend mode 下也会自动补齐该参数。

`CMP0141` 在首次 `project()` 前设为 `NEW`，CI 显式选择
`CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded`，生成 `/Z7` 调试信息，
避免 `/Zi`、`/ZI` 的共享编译 PDB；不叠加 `/MP`，由 Ninja 控制并行编译。
动态 CRT `/MD` 和既有 `/utf-8` 保持一致。

ccache 目录放在构建缓存内部，编译结果变化时与构建成果一起上传，沿用每类最新两份的
清理规则，不新增按运行编号生成的缓存。日志会输出 ccache 命中、未命中和不可缓存的统计。

更新 Python 工具依赖时，修改 `.github/requirements-ci.in` 后生成锁文件：

```powershell
uv pip compile .github/requirements-ci.in --python-version 3.12 --python-platform windows --output-file .github/requirements-ci.txt
```

### 可选的自动 Release

在 **Actions → Windows → Run workflow** 中，`publish_release` 默认不勾选。
普通推送、Pull Request、定时清理和未勾选的手动运行都不会发布 Release。

勾选后，构建和打包成功才执行独立的发布任务：

1. 从此次提交的 `CMakeLists.txt` 读取版本，例如 `0.3.0`，使用标签和标题 `v0.3.0`，
   标签指向此次构建的提交，不自动递增版本。
2. 将已验证的 ZIP 原样复制为 `gs-232b-open-gui-0.3.0-windows-x86_64.zip`，
   附带 `.zip.sha256` 校验文件，自动发布正式 Release。
3. 从根目录 `CHANGELOG.md` 的 `## [0.3.0]` 一节提取 Release 正文；
   完整 `CHANGELOG.md` 同时包含在便携 ZIP 内。

发布前应同时更新 CMake 版本和对应 changelog。缺少本版本更新说明时发布失败；
已存在同名 Release，或同名标签指向其他提交时，也会失败并保留已有发布内容。
只有发布任务获得 `contents: write`，普通构建保持只读仓库权限。

工作流通过 `scripts/deploy-windows.ps1` 调用原生 `windeployqt.exe` 打包。
MSVC DLL 仅来自当前工具链的 x64 CRT 可再分发目录（Windows CI 使用
`Microsoft.VC142.CRT`，本地 Qt 6 示例使用 `Microsoft.VC143.CRT`），
不会从 Windows 系统目录补齐 DLL。打包前复用 `verify-windows-runtime.py`
递归检查全部 EXE/DLL，拒绝非 x64、MinGW、旧 CRT 和 Debug CRT。
`objdump` 在编译完成后通过 MSYS2 binutils 安装，仅用于检查 PE 文件。

本地 Qt 6 原生构建可在 **VS 2022 x64 开发者 PowerShell（PowerShell 7）** 中运行：

```powershell
$qtPrefix = 'C:/Qt/6.8.3/msvc2022_64'
$env:PATH = "$qtPrefix/bin;$env:PATH"
cmake -S . -B build/windows-msvc-x86_64 -G Ninja `
    "-DCMAKE_PREFIX_PATH=$qtPrefix" -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_WIN32_EXECUTABLE=ON -DBUILD_TESTING=OFF
cmake --build build/windows-msvc-x86_64 --target gs_232b_open_gui --parallel 4
# 打包还需 python 和 GNU objdump 在 PATH 中；仅检查 PE，不使用 MinGW 编译。
./scripts/deploy-windows.ps1 -QtPrefix $qtPrefix
```

本地也要使用 ccache 时，需 CMake 3.25+ 和 PATH 中的 ccache，并在配置命令增加
`-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded`；在开发者 PowerShell 中设置
`$env:VSLANG='1033'` 和 `$env:CCACHE_CONFIGPATH=(Resolve-Path cmake/ccache-msvc.conf).Path`。

如开发者环境未设置 `VCToolsRedistDir`，可通过 `-MsvcRedistDir` 指定完整的
x64 `Microsoft.VC143.CRT` 目录。构建目录中的 `package/` 和
`gs-232b-open-gui-windows-x86_64.zip` 为最终输出。

## 在 Linux 上用 MSVC 交叉编译

`scripts/build-windows.sh` 面向 Linux x86_64 主机，通过 [msvc-wine](https://github.com/mstorsjo/msvc-wine) 在 Wine 中运行微软的 `cl.exe`、链接器和 Windows SDK 工具。CMake、Ninja 和 Qt 主机构建工具在 Linux 上运行。目标系统为 Windows 10（1809 或更新版本）/11 x64。

Windows 程序、静态依赖和 Qt DLL 统一使用 MSVC ABI 与动态 MSVC/UCRT 运行库（Release 使用 `/MD`）。不再使用 MinGW，也不再把 UCRT 编译器的运行库与旧 MSVCRT Qt SDK 混合打包。

## 安装依赖

Arch Linux：

```sh
sudo pacman -S --needed cmake ninja python python-pip wine msitools curl tar binutils
```

需要 CMake 3.25 或更新版本。构建工具链使用嵌入式调试信息，CMake 的编译器探测不依赖 `winbind`。安装系统依赖后，SDK 安装和编译无需管理员权限。

首次运行下载：

- 固定版本的 `msvc-wine`（提交 `5dd115047b7ddcbb53cbc8b5b14163a20f4334fe`，校验 SHA-256），以及微软 MSVC 14.44（VS 2022 v143）、Windows SDK 10.0.26100。微软组件由下载器根据官方安装清单校验。
- `aqtinstall 3.3.0` 和 Qt 6.8.3：Linux QtBase、QtTools、ICU，以及 Windows **msvc2022_64** 的 QtBase、QtTools、QtTranslations、QtSerialPort、QtWebSockets、QtPdf。
- 固定版本并校验 SHA-256 的 anytermqt 和 utf8proc；这两个依赖使用同一 MSVC 静态编译，无需额外 DLL。

SDK 下载器使用独立 Python 虚拟环境，默认使用阿里云 Qt 镜像并保留下载校验。首次安装微软工具链时，下载器会要求接受[微软许可](https://go.microsoft.com/fwlink/?LinkId=2086102)。阅读并接受后，可以用 `MSVC_ACCEPT_LICENSE=1` 进行非交互安装；脚本不会默认替用户接受许可。MSVC/SDK 安装目录是本机的构建缓存，不应随应用分发。

## 构建与输出

从项目根目录运行：

```sh
./scripts/build-windows.sh

# 已阅读并接受微软许可时，非交互首次安装并构建。
MSVC_ACCEPT_LICENSE=1 ./scripts/build-windows.sh
```

脚本可从其他工作目录通过绝对路径调用。默认并行编译数为 2，可通过 `JOBS=4` 调整。

成功后生成：

- `build/windows-msvc-x86_64/package/gs_232b_open_gui.exe`：Windows 程序。
- `build/windows-msvc-x86_64/package/`：完整程序目录，包含 Qt DLL、配套 MSVC 可再分发 DLL、平台插件、中文 Qt 翻译、许可证和 `qt.conf`。
- `build/windows-msvc-x86_64/gs-232b-open-gui-windows-x86_64.zip`：可分发 ZIP 包。

在 Windows 上解压 ZIP 后运行 `gs_232b_open_gui.exe`，保留完整目录。MSVC CRT 随程序部署，UCRT 由支持的 Windows 系统提供。应用自身的中文翻译嵌入 EXE。Windows 虚拟串口仍需预先安装配对驱动并创建 COM 对。

脚本只构建程序，关闭测试目标。打包时通过独立 Wine 前缀运行所选 Qt SDK 的 `windeployqt.exe`，然后递归检查所有 EXE/DLL 的架构和运行库导入；发现旧 `msvcrt.dll`、MinGW 运行库或 Debug CRT 会阻止生成新包。MSVC CRT 仅取自所选工具链的 x64 可再分发目录，不从 Wine 或系统中补 DLL。Wine 下的运行验证不能代替真实 Windows 上的设备连接验证。

## 缓存与自定义路径

SDK、下载器和独立 Wine 前缀默认缓存在 `${XDG_CACHE_HOME:-$HOME/.cache}/gs232b-windows`。再次运行复用缓存并增量编译。

```sh
# 改用 Qt 官方下载源。
QT_MIRROR=https://download.qt.io ./scripts/build-windows.sh

# 自定义缓存和构建目录。
WINDOWS_BUILD_CACHE=/path/to/cache BUILD_DIR=/path/to/build ./scripts/build-windows.sh

# 复用已通过 msvc-wine 安装的工具链和同版本 Qt SDK。
MSVC_ROOT=/path/to/msvc \
QT_HOST_PATH=/path/to/Qt/6.8.3/gcc_64 \
QT_WINDOWS_PREFIX=/path/to/Qt/6.8.3/msvc2022_64 \
./scripts/build-windows.sh
```

`MSVC_ROOT` 必须包含 `msvc-wine` 安装生成的 `bin/x64/{cl,link,lib,rc,mt}` 包装器。自备 Windows Qt 必须是 Qt 6.8.3 **msvc2022_64**，且含上述模块。`MSVC_REDIST_DIR` 可显式指定配套的 x64 `Microsoft.VC143.CRT` 目录。

旧的 `build/windows-x86_64` 目录和其中 ZIP 含不兼容的 MinGW/MSVCRT 组合，不能继续分发。新默认目录 `build/windows-msvc-x86_64` 避免复用旧缓存；显式指定含其他工具链缓存的 `BUILD_DIR` 时脚本会拒绝继续。更换工具链或 SDK 时也应使用新目录。

运行 `./scripts/build-windows.sh --help` 可查看所有环境变量。可独立复查任意目录：

```sh
python3 scripts/verify-windows-runtime.py build/windows-msvc-x86_64/package

# 运行打包运行库约束的回归检查，无需安装 Windows SDK。
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/pytest -p test_windows_runtime.py
```
