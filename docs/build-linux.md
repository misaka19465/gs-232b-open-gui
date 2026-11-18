# Linux 构建（Qt 5.15）

依赖 CMake 3.21+、C++17 编译器、Ninja，以及 Qt 5.15 的 Core、Widgets、Network、SerialPort、WebSockets、Pdf、LinguistTools；运行测试还需要 Qt Test。QtPdf 随 QtWebEngine 提供，无需在应用中链接完整 QtWebEngine。

Arch Linux：

```sh
sudo pacman -S --needed base-devel cmake ninja qt5-base qt5-tools qt5-serialport qt5-websockets qt5-webengine
```

从项目根目录使用新的构建目录，避免复用 Qt 6 缓存：

```sh
cmake -S . -B build/qt5 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/qt5 --parallel 2
ctest --test-dir build/qt5 --output-on-failure
./build/qt5/gs_232b_open_gui
```

应用翻译、窗口图标和 DXCC 数据嵌入程序。应用设置与地图缓存格式保持兼容，无需迁移。

使用自备 Qt 5.15 SDK 时，通过 `-DCMAKE_PREFIX_PATH=/path/to/Qt/5.15/gcc_64` 指定安装前缀，并确保包含上述模块。Qt 5.15.2 官方 SDK 若只提供 QtPdf 二进制，构建会提取匹配版本的官方头文件；其他版本应安装对应 QtPdf 开发包。
