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

cmake_minimum_required(VERSION 3.21)

project(gs_232b_open_gui VERSION 0.3.0 LANGUAGES C CXX)

add_compile_definitions(GS_232B_OPEN_GUI_VERSION="${PROJECT_VERSION}")

if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "Windows builds require MSVC and the Qt 5.15 msvc2019_64 SDK; use scripts/build-windows.sh")
    endif()
    # NOTE: 与 Qt 共享动态 CRT，避免跨模块分配和释放内存时使用不同的堆。
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/utf-8)
endif()

include(CTest)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets LinguistTools Network SerialPort WebSockets)

include(cmake/Qt5Pdf.cmake)
include(cmake/Qt5HostTools.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_library(gs232b_targeting STATIC
    MaidenheadLocator.cpp
    MaidenheadLocator.h
    targeting/DxccDatabase.cpp
    targeting/DxccDatabase.h
    targeting/TargetBearing.cpp
    targeting/TargetBearing.h
)
target_compile_features(gs232b_targeting PUBLIC cxx_std_17)
target_include_directories(gs232b_targeting PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_targeting PUBLIC Qt5::Core)
target_sources(gs232b_targeting PRIVATE targeting/data/dxcc_data.qrc)
configure_file(targeting/data/COPYING
    "${CMAKE_CURRENT_BINARY_DIR}/licenses/country-files-COPYING" COPYONLY)

if(WIN32)
    set(VIRTUAL_SERIAL_PLATFORM_SOURCE virtual_serial/VirtualSerialEndpoint_win.cpp)
elseif(UNIX)
    set(VIRTUAL_SERIAL_PLATFORM_SOURCE virtual_serial/VirtualSerialEndpoint_posix.cpp)
else()
    message(FATAL_ERROR "Virtual serial is supported on Windows, macOS, and Linux")
endif()

add_library(gs232b_virtual_serial STATIC
    virtual_serial/CrDelimitedBuffer.cpp
    virtual_serial/CrDelimitedBuffer.h
    virtual_serial/VirtualSerialBridge.cpp
    virtual_serial/VirtualSerialBridge.h
    virtual_serial/VirtualSerialEndpoint.h
    ${VIRTUAL_SERIAL_PLATFORM_SOURCE}
)
target_compile_features(gs232b_virtual_serial PUBLIC cxx_std_17)
target_include_directories(gs232b_virtual_serial PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_virtual_serial PUBLIC Qt5::Core)
if(WIN32)
    target_link_libraries(gs232b_virtual_serial PRIVATE Qt5::SerialPort)
endif()

add_library(gs232b_n1mm STATIC
    n1mm/N1mmProtocol.cpp
    n1mm/N1mmProtocol.h
    n1mm/N1mmController.cpp
    n1mm/N1mmController.h
)
target_compile_features(gs232b_n1mm PUBLIC cxx_std_17)
target_include_directories(gs232b_n1mm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_n1mm PUBLIC Qt5::Core Qt5::Network)

add_library(gs232b_gridtracker STATIC
    gridtracker/GridTrackerProtocol.cpp
    gridtracker/GridTrackerProtocol.h
    gridtracker/GridTrackerController.cpp
    gridtracker/GridTrackerController.h
)
target_compile_features(gs232b_gridtracker PUBLIC cxx_std_17)
target_include_directories(gs232b_gridtracker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_gridtracker PUBLIC Qt5::Core Qt5::Network)

add_library(gs232b_wsjtx STATIC
    wsjtx/WsjtXProtocol.cpp
    wsjtx/WsjtXProtocol.h
    wsjtx/WsjtXController.cpp
    wsjtx/WsjtXController.h
)
target_compile_features(gs232b_wsjtx PUBLIC cxx_std_17)
target_include_directories(gs232b_wsjtx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_wsjtx PUBLIC Qt5::Core Qt5::Network gs232b_targeting)

add_library(gs232b_wind_protection STATIC
    weather/WindProtectionController.cpp
    weather/WindProtectionController.h
)
target_compile_features(gs232b_wind_protection PUBLIC cxx_std_17)
target_include_directories(gs232b_wind_protection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_wind_protection PUBLIC Qt5::Core Qt5::Network gs232b_targeting)

set(GUI_SOURCES
    AntennaGainDialog.cpp
    AntennaGainDialog.h
    AppSettings.cpp
    AppSettings.h
    AnglePresets.cpp
    AnglePresets.h
    AzimuthScale.h
    AzimuthOffsetDialog.cpp
    AzimuthOffsetDialog.h
    CalibrationDialog.cpp
    CalibrationDialog.h
    CommandLog.cpp
    CommandLog.h
    CommunicationProtocol.cpp
    CommunicationProtocol.h
    MainWindow.cpp
    MainWindowView.cpp
    MainWindowConnection.cpp
    MainWindowCalibration.cpp
    MainWindowNetworkAddresses.cpp
    MainWindowN1mm.cpp
    MainWindowGridTracker.cpp
    MainWindowWsjtX.cpp
    MainWindowRotorModel.cpp
    MainWindow.h
    NetworkAddresses.cpp
    NetworkAddresses.h
    n1mm/N1mmNameDialog.cpp
    n1mm/N1mmNameDialog.h
    ConnectionDialog.cpp
    ConnectionDialog.h
    ControlPanel.cpp
    ControlPanelMotion.cpp
    ControlPanel.h
    MotionButtons.cpp
    MotionButtons.h
    Compass.cpp
    Compass.h
    CompassLayout.h
    GreatCircleMap.cpp
    GreatCircleMap.h
    GreatCircleMapPalette.h
    ConsolePanel.cpp
    ConsolePanel.h
    RotorModelDialog.cpp
    RotorModelDialog.h
    StationGridDialog.cpp
    StationGridDialog.h
    FaultStatusMonitor.cpp
    FaultStatusMonitor.h
    FaultStatusPanel.cpp
    FaultStatusPanel.h
    WifiProvisioningDialog.cpp
    WifiProvisioningDialog.h
    ManagementKeyDialog.cpp
    ManagementKeyDialog.h
    PasswordChangeDialog.cpp
    PasswordChangeDialog.h
    weather/WindWeatherSettingsDialog.cpp
    weather/WindWeatherSettingsDialog.h
    NetworkPortConfigDialog.cpp
    NetworkPortConfigDialog.h
    FirmwareUpgradeDialog.cpp
    FirmwareUpgradeDialog.h
)

add_executable(gs_232b_open_gui main.cpp ${GUI_SOURCES})

target_sources(gs_232b_open_gui PRIVATE assets/branding.qrc)
if(WIN32)
    target_sources(gs_232b_open_gui PRIVATE assets/app.rc)
    target_include_directories(gs_232b_open_gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()

target_compile_features(gs_232b_open_gui PRIVATE cxx_std_17)

target_link_libraries(gs_232b_open_gui
    PRIVATE
        Qt5::Widgets
        Qt5::Network
        Qt5::Pdf
        Qt5::SerialPort
        Qt5::WebSockets
        gs232b_virtual_serial
        gs232b_targeting
        gs232b_n1mm
        gs232b_gridtracker
        gs232b_wsjtx
        gs232b_wind_protection
)

if(BUILD_TESTING)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
    add_executable(wind_protection_tests tests/WindProtectionTests.cpp)
    target_link_libraries(wind_protection_tests PRIVATE gs232b_wind_protection Qt5::Test)
    add_test(NAME wind_protection_tests COMMAND wind_protection_tests)

    add_executable(wsjtx_tests tests/WsjtXTests.cpp)
    target_link_libraries(wsjtx_tests PRIVATE gs232b_wsjtx Qt5::Test)
    add_test(NAME wsjtx_tests COMMAND wsjtx_tests)
    set_tests_properties(wsjtx_tests PROPERTIES RESOURCE_LOCK wsjtx_udp_port)

    add_executable(network_addresses_tests
        tests/NetworkAddressesTests.cpp
        NetworkAddresses.cpp
        NetworkAddresses.h
    )
    target_compile_features(network_addresses_tests PRIVATE cxx_std_17)
    target_include_directories(network_addresses_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(network_addresses_tests PRIVATE Qt5::Network Qt5::Test)
    add_test(NAME network_addresses_tests COMMAND network_addresses_tests)

    add_executable(app_settings_tests
        tests/AppSettingsTests.cpp
        AppSettings.cpp
        AppSettings.h
    )
    target_compile_features(app_settings_tests PRIVATE cxx_std_17)
    target_include_directories(app_settings_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(app_settings_tests PRIVATE gs232b_targeting gs232b_n1mm Qt5::Test)
    add_test(NAME app_settings_tests COMMAND app_settings_tests)

    add_executable(n1mm_tests tests/N1mmTests.cpp)
    target_link_libraries(n1mm_tests PRIVATE gs232b_n1mm Qt5::Test)
    add_test(NAME n1mm_tests COMMAND n1mm_tests)

    add_executable(n1mm_connection_tests tests/N1mmConnectionTests.cpp ${GUI_SOURCES})
    target_link_libraries(n1mm_connection_tests PRIVATE
        Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::SerialPort Qt5::WebSockets Qt5::Test
        gs232b_virtual_serial gs232b_targeting gs232b_n1mm gs232b_gridtracker gs232b_wsjtx
        gs232b_wind_protection)
    add_test(NAME n1mm_connection_tests COMMAND n1mm_connection_tests)
    set_tests_properties(n1mm_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        RESOURCE_LOCK n1mm_udp_ports)
    set_tests_properties(n1mm_tests PROPERTIES RESOURCE_LOCK n1mm_udp_ports)

    add_executable(gridtracker_tests tests/GridTrackerTests.cpp)
    target_link_libraries(gridtracker_tests PRIVATE gs232b_gridtracker Qt5::Test)
    add_test(NAME gridtracker_tests COMMAND gridtracker_tests)

    add_executable(gridtracker_connection_tests tests/GridTrackerConnectionTests.cpp ${GUI_SOURCES})
    target_link_libraries(gridtracker_connection_tests PRIVATE
        Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::SerialPort Qt5::WebSockets Qt5::Test
        gs232b_virtual_serial gs232b_targeting gs232b_n1mm gs232b_gridtracker gs232b_wsjtx
        gs232b_wind_protection)
    add_test(NAME gridtracker_connection_tests COMMAND gridtracker_connection_tests)
    set_tests_properties(gridtracker_tests PROPERTIES RESOURCE_LOCK gridtracker_udp_ports)
    set_tests_properties(gridtracker_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        RESOURCE_LOCK "gridtracker_udp_ports;n1mm_udp_ports")

    add_executable(great_circle_map_tests
        tests/GreatCircleMapTests.cpp
        GreatCircleMap.cpp
        GreatCircleMap.h
    )
    target_compile_features(great_circle_map_tests PRIVATE cxx_std_17)
    target_include_directories(great_circle_map_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(great_circle_map_tests PRIVATE gs232b_targeting Qt5::Gui Qt5::Network Qt5::Pdf Qt5::Test)
    add_test(NAME great_circle_map_tests COMMAND great_circle_map_tests)
    set_tests_properties(great_circle_map_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(target_bearing_tests tests/TargetBearingTests.cpp)
    target_link_libraries(target_bearing_tests PRIVATE gs232b_targeting Qt5::Test)
    add_test(NAME target_bearing_tests COMMAND target_bearing_tests)

    add_executable(target_control_tests
        tests/TargetControlTests.cpp
        AnglePresets.cpp AnglePresets.h
        ControlPanel.cpp ControlPanel.h
        ControlPanelMotion.cpp
        MotionButtons.cpp MotionButtons.h
        AppSettings.cpp AppSettings.h
        Compass.cpp Compass.h
        GreatCircleMap.cpp GreatCircleMap.h
    )
    target_link_libraries(target_control_tests PRIVATE
        gs232b_targeting gs232b_n1mm Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::Test)
    add_test(NAME target_control_tests COMMAND target_control_tests)
    set_tests_properties(target_control_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(angle_compensation_tests
        tests/AngleCompensationTests.cpp
        AnglePresets.cpp AnglePresets.h
        ControlPanel.cpp ControlPanel.h
        ControlPanelMotion.cpp
        MotionButtons.cpp MotionButtons.h
        AppSettings.cpp AppSettings.h
        AzimuthScale.h
        Compass.cpp Compass.h
        GreatCircleMap.cpp GreatCircleMap.h
    )
    target_link_libraries(angle_compensation_tests PRIVATE
        gs232b_targeting gs232b_n1mm Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::Test)
    add_test(NAME angle_compensation_tests COMMAND angle_compensation_tests)
    set_tests_properties(angle_compensation_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(shortest_path_tests
        tests/ShortestPathTests.cpp
        AnglePresets.cpp AnglePresets.h
        ControlPanel.cpp ControlPanel.h
        ControlPanelMotion.cpp
        MotionButtons.cpp MotionButtons.h
        AppSettings.cpp AppSettings.h
        Compass.cpp Compass.h
        GreatCircleMap.cpp GreatCircleMap.h
    )
    target_link_libraries(shortest_path_tests PRIVATE
        gs232b_targeting gs232b_n1mm Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::Test)
    add_test(NAME shortest_path_tests COMMAND shortest_path_tests)
    set_tests_properties(shortest_path_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(motion_control_tests
        tests/MotionControlTests.cpp
        AnglePresets.cpp AnglePresets.h
        ControlPanel.cpp ControlPanel.h ControlPanelMotion.cpp
        MotionButtons.cpp MotionButtons.h
        AppSettings.cpp AppSettings.h
        Compass.cpp Compass.h
        GreatCircleMap.cpp GreatCircleMap.h
    )
    target_link_libraries(motion_control_tests PRIVATE
        gs232b_targeting gs232b_n1mm Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::Test)
    add_test(NAME motion_control_tests COMMAND motion_control_tests)
    set_tests_properties(motion_control_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(motion_connection_tests tests/MotionConnectionTests.cpp ${GUI_SOURCES})
    target_link_libraries(motion_connection_tests PRIVATE
        Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::SerialPort Qt5::WebSockets Qt5::Test
        gs232b_virtual_serial gs232b_targeting gs232b_n1mm gs232b_gridtracker gs232b_wsjtx
        gs232b_wind_protection)
    add_test(NAME motion_connection_tests COMMAND motion_connection_tests)
    set_tests_properties(motion_connection_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(command_log_tests
        tests/CommandLogTests.cpp
        CommandLog.cpp
        CommandLog.h
    )
    target_compile_features(command_log_tests PRIVATE cxx_std_17)
    target_include_directories(command_log_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(command_log_tests PRIVATE Qt5::Core Qt5::Test)
    add_test(NAME command_log_tests COMMAND command_log_tests)

    add_executable(virtual_serial_tests
        tests/VirtualSerialTests.cpp
    )
    target_compile_features(virtual_serial_tests PRIVATE cxx_std_17)
    target_link_libraries(virtual_serial_tests
        PRIVATE
            gs232b_virtual_serial
            Qt5::Test
    )
    add_test(NAME virtual_serial_tests COMMAND virtual_serial_tests)

    add_executable(fault_status_tests
        tests/FaultStatusMonitorTests.cpp
        FaultStatusMonitor.cpp
        FaultStatusMonitor.h
    )
    target_compile_features(fault_status_tests PRIVATE cxx_std_17)
    target_include_directories(fault_status_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fault_status_tests PRIVATE Qt5::Core Qt5::Test)
    add_test(NAME fault_status_tests COMMAND fault_status_tests)

    add_executable(rotor_model_connection_tests
        tests/RotorModelConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(rotor_model_connection_tests PRIVATE cxx_std_17)
    target_include_directories(rotor_model_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(rotor_model_connection_tests
        PRIVATE Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::SerialPort Qt5::WebSockets Qt5::Test gs232b_virtual_serial gs232b_targeting gs232b_n1mm gs232b_gridtracker gs232b_wsjtx gs232b_wind_protection
    )
    add_test(NAME rotor_model_connection_tests COMMAND rotor_model_connection_tests)
    set_tests_properties(rotor_model_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(ap_provisioning_connection_tests
        tests/ApProvisioningConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(ap_provisioning_connection_tests PRIVATE cxx_std_17)
    target_include_directories(ap_provisioning_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(ap_provisioning_connection_tests
        PRIVATE Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::SerialPort Qt5::WebSockets Qt5::Test gs232b_virtual_serial gs232b_targeting gs232b_n1mm gs232b_gridtracker gs232b_wsjtx gs232b_wind_protection
    )
    add_test(NAME ap_provisioning_connection_tests COMMAND ap_provisioning_connection_tests)
    set_tests_properties(ap_provisioning_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(virtual_serial_connection_tests
        tests/VirtualSerialConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(virtual_serial_connection_tests PRIVATE cxx_std_17)
    target_include_directories(virtual_serial_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(virtual_serial_connection_tests
        PRIVATE Qt5::Widgets Qt5::Network Qt5::Pdf Qt5::SerialPort Qt5::WebSockets Qt5::Test gs232b_virtual_serial gs232b_targeting gs232b_n1mm gs232b_gridtracker gs232b_wsjtx gs232b_wind_protection
    )
    add_test(NAME virtual_serial_connection_tests COMMAND virtual_serial_connection_tests)
    set_tests_properties(virtual_serial_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

qt5_add_translation(APP_TRANSLATIONS translations/gs_232b_open_gui_zh_CN.ts)
configure_file(translations/i18n.qrc.in i18n.qrc @ONLY)
target_sources(gs_232b_open_gui PRIVATE ${APP_TRANSLATIONS} "${CMAKE_CURRENT_BINARY_DIR}/i18n.qrc")
