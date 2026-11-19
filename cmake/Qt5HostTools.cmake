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

# NOTE: Qt 5 的目标 SDK 不识别 QT_HOST_PATH；交叉编译时显式使用同版本 Linux 工具。
if(CMAKE_CROSSCOMPILING AND QT_HOST_PATH)
    foreach(tool moc rcc uic lrelease lupdate)
        set(host_tool "${QT_HOST_PATH}/bin/${tool}")
        if(NOT EXISTS "${host_tool}")
            message(FATAL_ERROR "Qt 5.15 host tool not found: ${host_tool}")
        endif()
        set_target_properties(Qt5::${tool} PROPERTIES IMPORTED_LOCATION "${host_tool}")
        if(TARGET Qt::${tool})
            set_target_properties(Qt::${tool} PROPERTIES IMPORTED_LOCATION "${host_tool}")
        endif()
    endforeach()
endif()
