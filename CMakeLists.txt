       cmake_minimum_required(VERSION 3.16)
project(LightController VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets OpenGLWidgets)

# libftdi (MSYS2: mingw-w64-x86_64-libftdi)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBFTDI1 REQUIRED IMPORTED_TARGET libftdi1 libusb-1.0)

# ========== Protocol 库 ==========
add_library(lightprotocol SHARED
    protocol/lightprotocol_global.h
    # Art-Net
    protocol/ArtNetSender.h
    protocol/ArtNetSender.cpp
    protocol/ArtNetReceiver.h
    protocol/ArtNetReceiver.cpp
    protocol/ArtNetCommon.h

    # 灯具模型 + 宇宙管理
    protocol/Fixture.h
    protocol/Fixture.cpp
    protocol/Universe.h
    protocol/Universe.cpp

    # DMX512 USB
    protocol/dmxinterface.h
    protocol/dmxinterface.cpp
    protocol/dmxusbwidget.h
    protocol/dmxusbwidget.cpp
    protocol/enttecdmxusbopen.h
    protocol/enttecdmxusbopen.cpp
    protocol/libftdi-interface.h
    protocol/libftdi-interface.cpp
)

target_include_directories(lightprotocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    ${LIBFTDI1_INCLUDE_DIRS}
)

target_compile_definitions(lightprotocol PRIVATE
    LIGHTPROTOCOL_LIBRARY
    LIBFTDI1
)

target_link_libraries(lightprotocol PUBLIC
    Qt6::Core
    Qt6::Network
    PkgConfig::LIBFTDI1
)

# ========== 主程序 ==========
add_executable(LightController
    main.cpp
    mainwindow.ui
    mainwindow.h
    mainwindow.cpp
    fixtureitem.h
    fixtureitem.cpp
    dropview.h
    draglibrarylist.h
    fixturedialog.h
    addresspage.h
    librarypage.h
    programpage.h
    domainpage.h
    globe3d.h
    globe3d.cpp
    colorwheel.h
    beamwidget.h
    timeline.h
    stagelayout.h
)

target_include_directories(LightController PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBFTDI1_INCLUDE_DIRS}
)

target_compile_definitions(LightController PRIVATE LIBFTDI1)

target_link_libraries(LightController PRIVATE
    lightprotocol
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
    Qt6::OpenGLWidgets
    OpenGL32
    PkgConfig::LIBFTDI1
)
