cmake_minimum_required(VERSION 3.21)

# Select the MSVC debug format before project() enables the compiler. CI uses
# Embedded (/Z7) so ccache does not depend on a shared compiler PDB (/Zi, /ZI).
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(gs_232b_open_gui VERSION 0.3.0 LANGUAGES C CXX)

if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "Windows builds require MSVC; use scripts/build-windows.sh")
    endif()
    # NOTE: 与 Qt 共享动态 CRT，避免跨模块分配和释放内存时使用不同的堆。
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/utf-8)
endif()

include(CTest)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network SerialPort WebSockets)
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Network Pdf SerialPort WebSockets)
    set(QT_MAP_RENDER_TARGETS Qt6::Pdf)
elseif(QT_VERSION_MAJOR EQUAL 5)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets LinguistTools Network SerialPort WebSockets)
    set(QT_MAP_RENDER_TARGETS windowsapp)
    if(NOT WIN32)
        message(FATAL_ERROR "Qt 5.15 is supported for Windows compatibility builds; Linux and macOS builds use Qt 6.")
    endif()
else()
    message(FATAL_ERROR "Only Qt 5.15 and Qt 6 are supported")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_standard_project_setup()
else()
    set(CMAKE_AUTOMOC ON)
endif()
set(CMAKE_AUTORCC ON)

# Keep Qt 6's target finalization behavior while using native CMake commands
# for Qt 5.15, whose SDK does not provide the Qt 6 target helpers.
function(project_add_library)
    if(QT_VERSION_MAJOR EQUAL 6)
        qt_add_library(${ARGV})
    else()
        add_library(${ARGV})
    endif()
endfunction()

function(project_add_executable)
    if(QT_VERSION_MAJOR EQUAL 6)
        qt_add_executable(${ARGV})
    else()
        add_executable(${ARGV})
    endif()
endfunction()

project_add_library(gs232b_targeting STATIC
    MaidenheadLocator.cpp
    MaidenheadLocator.h
    targeting/DxccDatabase.cpp
    targeting/DxccDatabase.h
    targeting/TargetBearing.cpp
    targeting/TargetBearing.h
    resources/dxcc.qrc
)
target_compile_features(gs232b_targeting PUBLIC cxx_std_17)
target_include_directories(gs232b_targeting PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_targeting PUBLIC Qt${QT_VERSION_MAJOR}::Core)
configure_file(targeting/data/COPYING
    "${CMAKE_CURRENT_BINARY_DIR}/licenses/country-files-COPYING" COPYONLY)

include(cmake/TerminalDependencies.cmake)
project_add_library(gs232b_terminal STATIC
    terminal/CommandLine.cpp
    terminal/CommandLine.h
    terminal/TerminalWidget.cpp
    terminal/TerminalWidget.h
    terminal/TerminalOutputBuffer.cpp
    terminal/TerminalOutputBuffer.h
)
target_compile_features(gs232b_terminal PUBLIC cxx_std_17)
target_include_directories(gs232b_terminal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_terminal PUBLIC qtpyte::core)

if(WIN32)
    set(VIRTUAL_SERIAL_PLATFORM_SOURCE virtual_serial/VirtualSerialEndpoint_win.cpp)
elseif(UNIX)
    set(VIRTUAL_SERIAL_PLATFORM_SOURCE virtual_serial/VirtualSerialEndpoint_posix.cpp)
else()
    message(FATAL_ERROR "Virtual serial is supported on Windows, macOS, and Linux")
endif()

project_add_library(gs232b_virtual_serial STATIC
    virtual_serial/CrDelimitedBuffer.cpp
    virtual_serial/CrDelimitedBuffer.h
    virtual_serial/VirtualSerialBridge.cpp
    virtual_serial/VirtualSerialBridge.h
    virtual_serial/VirtualSerialEndpoint.h
    ${VIRTUAL_SERIAL_PLATFORM_SOURCE}
)
target_compile_features(gs232b_virtual_serial PUBLIC cxx_std_17)
target_include_directories(gs232b_virtual_serial PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_virtual_serial PUBLIC Qt${QT_VERSION_MAJOR}::Core)
if(WIN32)
    target_link_libraries(gs232b_virtual_serial PRIVATE Qt${QT_VERSION_MAJOR}::SerialPort)
endif()

project_add_library(gs232b_n1mm STATIC
    n1mm/N1mmProtocol.cpp
    n1mm/N1mmProtocol.h
    n1mm/N1mmController.cpp
    n1mm/N1mmController.h
)
target_compile_features(gs232b_n1mm PUBLIC cxx_std_17)
target_include_directories(gs232b_n1mm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_n1mm PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)

project_add_library(gs232b_gridtracker STATIC
    gridtracker/GridTrackerProtocol.cpp
    gridtracker/GridTrackerProtocol.h
    gridtracker/GridTrackerController.cpp
    gridtracker/GridTrackerController.h
)
target_compile_features(gs232b_gridtracker PUBLIC cxx_std_17)
target_include_directories(gs232b_gridtracker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gs232b_gridtracker PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)

set(GUI_SOURCES
    AppSettings.cpp
    AppSettings.h
    AngleReading.cpp
    AngleReading.h
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
    NetworkPortConfigDialog.cpp
    NetworkPortConfigDialog.h
    FirmwareUpgradeDialog.cpp
    FirmwareUpgradeDialog.h
)

project_add_executable(gs_232b_open_gui main.cpp ${GUI_SOURCES} resources/branding.qrc)
if(APPLE)
    set_target_properties(gs_232b_open_gui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.gs232b.open-gui"
        MACOSX_BUNDLE_BUNDLE_NAME "GS-232B Open GUI"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
endif()
if(WIN32)
    target_sources(gs_232b_open_gui PRIVATE assets/app.rc)
    target_include_directories(gs_232b_open_gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()

target_compile_features(gs_232b_open_gui PRIVATE cxx_std_17)

target_link_libraries(gs_232b_open_gui
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        ${QT_MAP_RENDER_TARGETS}
        Qt${QT_VERSION_MAJOR}::SerialPort
        Qt${QT_VERSION_MAJOR}::WebSockets
        gs232b_virtual_serial
        gs232b_terminal
        gs232b_targeting
        gs232b_n1mm
        gs232b_gridtracker
)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    project_add_executable(network_addresses_tests
        tests/NetworkAddressesTests.cpp
        NetworkAddresses.cpp
        NetworkAddresses.h
    )
    target_compile_features(network_addresses_tests PRIVATE cxx_std_17)
    target_include_directories(network_addresses_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(network_addresses_tests PRIVATE Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME network_addresses_tests COMMAND network_addresses_tests)

    project_add_executable(app_settings_tests
        tests/AppSettingsTests.cpp
        AppSettings.cpp
        AppSettings.h
    )
    target_compile_features(app_settings_tests PRIVATE cxx_std_17)
    target_include_directories(app_settings_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(app_settings_tests PRIVATE gs232b_targeting gs232b_n1mm Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME app_settings_tests COMMAND app_settings_tests)

    project_add_executable(n1mm_tests tests/N1mmTests.cpp)
    target_link_libraries(n1mm_tests PRIVATE gs232b_n1mm Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME n1mm_tests COMMAND n1mm_tests)

    project_add_executable(n1mm_connection_tests tests/N1mmConnectionTests.cpp ${GUI_SOURCES})
    target_link_libraries(n1mm_connection_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::SerialPort Qt${QT_VERSION_MAJOR}::WebSockets Qt${QT_VERSION_MAJOR}::Test
        gs232b_virtual_serial gs232b_terminal gs232b_targeting gs232b_n1mm gs232b_gridtracker)
    add_test(NAME n1mm_connection_tests COMMAND n1mm_connection_tests)
    set_tests_properties(n1mm_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        RESOURCE_LOCK n1mm_udp_ports)
    set_tests_properties(n1mm_tests PROPERTIES RESOURCE_LOCK n1mm_udp_ports)

    project_add_executable(gridtracker_tests tests/GridTrackerTests.cpp)
    target_link_libraries(gridtracker_tests PRIVATE gs232b_gridtracker Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME gridtracker_tests COMMAND gridtracker_tests)

    project_add_executable(gridtracker_connection_tests tests/GridTrackerConnectionTests.cpp ${GUI_SOURCES})
    target_link_libraries(gridtracker_connection_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::SerialPort Qt${QT_VERSION_MAJOR}::WebSockets Qt${QT_VERSION_MAJOR}::Test
        gs232b_virtual_serial gs232b_terminal gs232b_targeting gs232b_n1mm gs232b_gridtracker)
    add_test(NAME gridtracker_connection_tests COMMAND gridtracker_connection_tests)
    set_tests_properties(gridtracker_tests PROPERTIES RESOURCE_LOCK gridtracker_udp_ports)
    set_tests_properties(gridtracker_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        RESOURCE_LOCK "gridtracker_udp_ports;n1mm_udp_ports")

    project_add_executable(great_circle_map_tests
        tests/GreatCircleMapTests.cpp
        GreatCircleMap.cpp
        GreatCircleMap.h
    )
    target_compile_features(great_circle_map_tests PRIVATE cxx_std_17)
    target_include_directories(great_circle_map_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(great_circle_map_tests PRIVATE gs232b_targeting Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME great_circle_map_tests COMMAND great_circle_map_tests)
    set_tests_properties(great_circle_map_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    project_add_executable(target_bearing_tests tests/TargetBearingTests.cpp)
    target_link_libraries(target_bearing_tests PRIVATE gs232b_targeting Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME target_bearing_tests COMMAND target_bearing_tests)

    project_add_executable(target_control_tests
        tests/TargetControlTests.cpp
        AngleReading.cpp AngleReading.h
        AnglePresets.cpp AnglePresets.h
        ControlPanel.cpp ControlPanel.h
        ControlPanelMotion.cpp
        MotionButtons.cpp MotionButtons.h
        AppSettings.cpp AppSettings.h
        Compass.cpp Compass.h
        GreatCircleMap.cpp GreatCircleMap.h
    )
    target_link_libraries(target_control_tests PRIVATE
        gs232b_targeting gs232b_n1mm Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME target_control_tests COMMAND target_control_tests)
    set_tests_properties(target_control_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    project_add_executable(angle_compensation_tests
        tests/AngleCompensationTests.cpp
        AngleReading.cpp AngleReading.h
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
        gs232b_targeting gs232b_n1mm Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME angle_compensation_tests COMMAND angle_compensation_tests)
    set_tests_properties(angle_compensation_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    project_add_executable(shortest_path_tests
        tests/ShortestPathTests.cpp
        AngleReading.cpp AngleReading.h
        AnglePresets.cpp AnglePresets.h
        ControlPanel.cpp ControlPanel.h
        ControlPanelMotion.cpp
        MotionButtons.cpp MotionButtons.h
        AppSettings.cpp AppSettings.h
        Compass.cpp Compass.h
        GreatCircleMap.cpp GreatCircleMap.h
    )
    target_link_libraries(shortest_path_tests PRIVATE
        gs232b_targeting gs232b_n1mm Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME shortest_path_tests COMMAND shortest_path_tests)
    set_tests_properties(shortest_path_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    project_add_executable(motion_control_tests
        tests/MotionControlTests.cpp
        AngleReading.cpp AngleReading.h
        AnglePresets.cpp AnglePresets.h
        ControlPanel.cpp ControlPanel.h ControlPanelMotion.cpp
        MotionButtons.cpp MotionButtons.h
        AppSettings.cpp AppSettings.h
        Compass.cpp Compass.h
        GreatCircleMap.cpp GreatCircleMap.h
    )
    target_link_libraries(motion_control_tests PRIVATE
        gs232b_targeting gs232b_n1mm Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME motion_control_tests COMMAND motion_control_tests)
    set_tests_properties(motion_control_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    project_add_executable(motion_connection_tests tests/MotionConnectionTests.cpp ${GUI_SOURCES})
    target_link_libraries(motion_connection_tests PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::SerialPort Qt${QT_VERSION_MAJOR}::WebSockets Qt${QT_VERSION_MAJOR}::Test
        gs232b_virtual_serial gs232b_terminal gs232b_targeting gs232b_n1mm gs232b_gridtracker)
    add_test(NAME motion_connection_tests COMMAND motion_connection_tests)
    set_tests_properties(motion_connection_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    project_add_executable(terminal_tests
        tests/TerminalTests.cpp
        ConsolePanel.cpp
        ConsolePanel.h
        CommandLog.cpp
        CommandLog.h
    )
    target_link_libraries(terminal_tests PRIVATE gs232b_terminal Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME terminal_tests COMMAND terminal_tests)
    set_tests_properties(terminal_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    project_add_executable(command_log_tests
        tests/CommandLogTests.cpp
        CommandLog.cpp
        CommandLog.h
    )
    target_compile_features(command_log_tests PRIVATE cxx_std_17)
    target_include_directories(command_log_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(command_log_tests PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME command_log_tests COMMAND command_log_tests)

    project_add_executable(virtual_serial_tests
        tests/VirtualSerialTests.cpp
    )
    target_compile_features(virtual_serial_tests PRIVATE cxx_std_17)
    target_link_libraries(virtual_serial_tests
        PRIVATE
            gs232b_virtual_serial
            Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME virtual_serial_tests COMMAND virtual_serial_tests)

    project_add_executable(fault_status_tests
        tests/FaultStatusMonitorTests.cpp
        FaultStatusMonitor.cpp
        FaultStatusMonitor.h
    )
    target_compile_features(fault_status_tests PRIVATE cxx_std_17)
    target_include_directories(fault_status_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(fault_status_tests PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME fault_status_tests COMMAND fault_status_tests)

    project_add_executable(rotor_model_connection_tests
        tests/RotorModelConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(rotor_model_connection_tests PRIVATE cxx_std_17)
    target_include_directories(rotor_model_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(rotor_model_connection_tests
        PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::SerialPort Qt${QT_VERSION_MAJOR}::WebSockets Qt${QT_VERSION_MAJOR}::Test gs232b_virtual_serial gs232b_terminal gs232b_targeting gs232b_n1mm gs232b_gridtracker
    )
    add_test(NAME rotor_model_connection_tests COMMAND rotor_model_connection_tests)
    set_tests_properties(rotor_model_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    project_add_executable(ap_provisioning_connection_tests
        tests/ApProvisioningConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(ap_provisioning_connection_tests PRIVATE cxx_std_17)
    target_include_directories(ap_provisioning_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(ap_provisioning_connection_tests
        PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::SerialPort Qt${QT_VERSION_MAJOR}::WebSockets Qt${QT_VERSION_MAJOR}::Test gs232b_virtual_serial gs232b_terminal gs232b_targeting gs232b_n1mm gs232b_gridtracker
    )
    add_test(NAME ap_provisioning_connection_tests COMMAND ap_provisioning_connection_tests)
    set_tests_properties(ap_provisioning_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    project_add_executable(virtual_serial_connection_tests
        tests/VirtualSerialConnectionTests.cpp
        ${GUI_SOURCES}
    )
    target_compile_features(virtual_serial_connection_tests PRIVATE cxx_std_17)
    target_include_directories(virtual_serial_connection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(virtual_serial_connection_tests
        PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network ${QT_MAP_RENDER_TARGETS} Qt${QT_VERSION_MAJOR}::SerialPort Qt${QT_VERSION_MAJOR}::WebSockets Qt${QT_VERSION_MAJOR}::Test gs232b_virtual_serial gs232b_terminal gs232b_targeting gs232b_n1mm gs232b_gridtracker
    )
    add_test(NAME virtual_serial_connection_tests COMMAND virtual_serial_connection_tests)
    set_tests_properties(virtual_serial_connection_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_translations(gs_232b_open_gui
        TS_FILES translations/gs_232b_open_gui_zh_CN.ts
        RESOURCE_PREFIX "/i18n"
    )
else()
    qt5_add_translation(APP_TRANSLATION_FILE translations/gs_232b_open_gui_zh_CN.ts)
    add_custom_target(gs232b_app_translation DEPENDS ${APP_TRANSLATION_FILE})
    add_dependencies(gs_232b_open_gui gs232b_app_translation)
endif()
