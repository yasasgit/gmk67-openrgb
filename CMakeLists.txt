cmake_minimum_required(VERSION 3.21)
project(OpenRGBGMK67Plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(OPENRGB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OpenRGB" CACHE PATH "OpenRGB source at commit 8e16de27")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)

add_library(OpenRGBGMK67Plugin SHARED
    src/GMK67Plugin.h
    src/GMK67Plugin.cpp
    src/GMK67Plugin.json
    src/GMK67Device.h
    src/GMK67Device.cpp
    resources/resources.qrc
    ${OPENRGB_DIR}/OpenRGBPluginInterface.h
)

target_include_directories(OpenRGBGMK67Plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENRGB_DIR}
    ${OPENRGB_DIR}/RGBController
    ${OPENRGB_DIR}/dependencies/json
)

target_compile_definitions(OpenRGBGMK67Plugin PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(OpenRGBGMK67Plugin PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets hid setupapi cfgmgr32)
