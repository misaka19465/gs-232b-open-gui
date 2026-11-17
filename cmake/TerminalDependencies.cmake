include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# NOTE: 固定归档和校验值；先声明 utf8proc，覆盖上游依赖中的 Git 下载方式。
FetchContent_Declare(utf8proc
    URL https://api.github.com/repos/JuliaStrings/utf8proc/tarball/v2.11.3
    URL_HASH SHA256=3e01616be4c767492b084d5eaece31b5d007b846522a5e19783c438277e6b0e8
)
FetchContent_Declare(anytermqt
    URL https://api.github.com/repos/scottpeterman/anytermqt/tarball/9943b0ee15ced240e1e146c7f107b9ab202193f1
    URL_HASH SHA256=26826fe786e897806ccd8475856045d5de769ffa104e470653997c01ec519bac
    SOURCE_SUBDIR qtpyte
)
set(QTPYTE_BUILD_TESTS OFF CACHE BOOL "Build anytermqt tests" FORCE)
set(QTPYTE_BUILD_APP OFF CACHE BOOL "Build anytermqt example" FORCE)
FetchContent_GetProperties(anytermqt)
if(NOT anytermqt_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(anytermqt)
endif()
include(cmake/PatchAnytermQt5.cmake)
add_subdirectory("${anytermqt_SOURCE_DIR}/qtpyte" "${anytermqt_BINARY_DIR}/qtpyte")
set_target_properties(utf8proc PROPERTIES AUTOMOC OFF AUTOUIC OFF)
include("${CMAKE_CURRENT_LIST_DIR}/PreserveTerminalHistory.cmake")

configure_file("${anytermqt_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_BINARY_DIR}/licenses/anytermqt-LICENSE" COPYONLY)
configure_file("${utf8proc_SOURCE_DIR}/LICENSE.md"
    "${CMAKE_CURRENT_BINARY_DIR}/licenses/utf8proc-LICENSE.md" COPYONLY)
