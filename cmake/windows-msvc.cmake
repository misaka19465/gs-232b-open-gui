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

# NOTE: Embedded 调试信息让 CMake 的编译器探测无需启动 mspdbsrv/winbind。
cmake_minimum_required(VERSION 3.25)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT MSVC_ROOT OR NOT QT_WINDOWS_PREFIX)
    message(FATAL_ERROR "MSVC_ROOT and QT_WINDOWS_PREFIX are required")
endif()

set(CMAKE_C_COMPILER "${MSVC_ROOT}/bin/x64/cl")
set(CMAKE_CXX_COMPILER "${MSVC_ROOT}/bin/x64/cl")
set(CMAKE_LINKER "${MSVC_ROOT}/bin/x64/link")
set(CMAKE_AR "${MSVC_ROOT}/bin/x64/lib")
set(CMAKE_RC_COMPILER "${MSVC_ROOT}/bin/x64/rc")
set(CMAKE_MT "${MSVC_ROOT}/bin/x64/mt")
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# NOTE: 配置探测也使用相同工具链和目标 Qt，禁止搜索 Linux 的头文件和库。
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MSVC_ROOT QT_WINDOWS_PREFIX)
list(APPEND CMAKE_FIND_ROOT_PATH "${QT_WINDOWS_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
