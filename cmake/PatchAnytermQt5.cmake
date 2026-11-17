if(NOT QT_VERSION_MAJOR EQUAL 5)
    return()
endif()

set(qtpyte_cmake "${anytermqt_SOURCE_DIR}/qtpyte/CMakeLists.txt")
file(READ "${qtpyte_cmake}" qtpyte_cmake_contents)
set(qtpyte_cmake_original "${qtpyte_cmake_contents}")
string(REPLACE
    "find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)"
    "find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)"
    qtpyte_cmake_contents "${qtpyte_cmake_contents}")
string(REPLACE "Qt6::Widgets" "Qt5::Widgets"
    qtpyte_cmake_contents "${qtpyte_cmake_contents}")
if(qtpyte_cmake_contents MATCHES "Qt6|find_package\(Qt6")
    message(FATAL_ERROR "Could not adapt the pinned anytermqt CMake project to Qt 5.15")
endif()
if(NOT "${qtpyte_cmake_contents}" STREQUAL "${qtpyte_cmake_original}")
    file(WRITE "${qtpyte_cmake}" "${qtpyte_cmake_contents}")
endif()

set(qtpyte_selection "${anytermqt_SOURCE_DIR}/qtpyte/src/selection.cpp")
file(READ "${qtpyte_selection}" qtpyte_selection_contents)
set(qtpyte_selection_original "${qtpyte_selection_contents}")
string(REGEX MATCHALL "event->position\(\)\.toPoint\(\)" qt6_mouse_api_uses
    "${qtpyte_selection_contents}")
list(LENGTH qt6_mouse_api_uses qt6_mouse_api_count)
if(qt6_mouse_api_count EQUAL 3)
    string(REPLACE "event->position().toPoint()" "event->pos()"
        qtpyte_selection_contents "${qtpyte_selection_contents}")
elseif(qt6_mouse_api_count EQUAL 0)
    string(REGEX MATCHALL "event->pos\(\)" qt5_mouse_api_uses "${qtpyte_selection_contents}")
    list(LENGTH qt5_mouse_api_uses qt5_mouse_api_count)
    if(NOT qt5_mouse_api_count EQUAL 3)
        message(FATAL_ERROR "The pinned anytermqt selection source changed; recheck its Qt 5 mouse API patch")
    endif()
else()
    message(FATAL_ERROR "The pinned anytermqt selection source changed; recheck its Qt 5 mouse API patch")
endif()
if(NOT "${qtpyte_selection_contents}" STREQUAL "${qtpyte_selection_original}")
    file(WRITE "${qtpyte_selection}" "${qtpyte_selection_contents}")
endif()
