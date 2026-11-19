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

find_package(Qt5Pdf 5.15 QUIET)
if(TARGET Qt5::Pdf)
    return()
endif()

# NOTE: Qt 5.15.2 官方 SDK 含 QtPdf 二进制，但未打包开发头文件和 CMake 配置。
# NOTE: 仅为这个精确版本补齐对应官方头文件，禁止借用其他补丁版本的 ABI。
if(NOT Qt5Core_VERSION_STRING VERSION_EQUAL "5.15.2")
    message(FATAL_ERROR "Install the Qt 5.15 Pdf development package for this Qt SDK")
endif()
get_filename_component(qt5_prefix "${Qt5Core_DIR}/../../.." ABSOLUTE)
find_library(QT5_PDF_LIBRARY NAMES Qt5Pdf PATHS "${qt5_prefix}/lib" NO_DEFAULT_PATH REQUIRED)

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(qt5pdf_headers
    URL https://api.github.com/repos/qt/qtwebengine/tarball/v5.15.2
    URL_HASH SHA256=32cbea353267df6f13b22eb7561b5c358a36332c1a9215641cdb301f7fbb5021
)
FetchContent_MakeAvailable(qt5pdf_headers)
set(qt5pdf_include "${CMAKE_CURRENT_BINARY_DIR}/qt5pdf/include")
foreach(header qpdfdocument qpdfdocumentrenderoptions qpdfnamespace qpdfselection qtpdfglobal)
    configure_file("${qt5pdf_headers_SOURCE_DIR}/src/pdf/api/${header}.h"
        "${qt5pdf_include}/QtPdf/${header}.h" COPYONLY)
endforeach()
file(CONFIGURE OUTPUT "${qt5pdf_include}/QtPdf/QPdfDocument" CONTENT "#include \"qpdfdocument.h\"\n" @ONLY)
configure_file("${qt5pdf_headers_SOURCE_DIR}/LICENSE.LGPL3"
    "${CMAKE_CURRENT_BINARY_DIR}/licenses/QtPdf-LICENSE.LGPL3" COPYONLY)
configure_file("${qt5pdf_headers_SOURCE_DIR}/LICENSE.GPL3"
    "${CMAKE_CURRENT_BINARY_DIR}/licenses/QtPdf-LICENSE.GPL3" COPYONLY)

add_library(Qt5::Pdf SHARED IMPORTED)
set_target_properties(Qt5::Pdf PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${qt5pdf_include};${qt5pdf_include}/QtPdf"
    INTERFACE_COMPILE_DEFINITIONS QT_PDF_LIB
    INTERFACE_LINK_LIBRARIES "Qt5::Core;Qt5::Gui"
)
if(WIN32)
    set_target_properties(Qt5::Pdf PROPERTIES
        IMPORTED_CONFIGURATIONS "RELEASE;DEBUG"
        IMPORTED_IMPLIB_RELEASE "${QT5_PDF_LIBRARY}"
        IMPORTED_LOCATION_RELEASE "${qt5_prefix}/bin/Qt5Pdf.dll"
        IMPORTED_IMPLIB_DEBUG "${qt5_prefix}/lib/Qt5Pdfd.lib"
        IMPORTED_LOCATION_DEBUG "${qt5_prefix}/bin/Qt5Pdfd.dll"
    )
else()
    set_target_properties(Qt5::Pdf PROPERTIES IMPORTED_LOCATION "${QT5_PDF_LIBRARY}")
endif()
