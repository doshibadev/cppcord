cmake_minimum_required(VERSION 3.16)

# Set vcpkg toolchain before project()
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

project(DiscordClient VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.1/msvc2022_64")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebSockets Multimedia)

# Find vcpkg libraries for voice chat
find_package(Opus CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Include directories
include_directories(
    src
    src/core
    src/ui
    src/network
    src/models
    src/utils
)

# Source files
set(SOURCES
    src/main.cpp
    src/network/DiscordClient.cpp
    src/network/GatewayClient.cpp
    src/ui/LoginDialog.cpp
    src/ui/MainWindow.cpp
    src/utils/TokenStorage.cpp
)

set(HEADERS
    src/network/DiscordClient.h
    src/network/GatewayClient.h
    src/ui/LoginDialog.h
    src/ui/MainWindow.h
    src/models/User.h
    src/models/Snowflake.h
    src/models/Guild.h
    src/models/Channel.h
    src/models/Message.h
    src/utils/TokenStorage.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::Multimedia
    Opus::opus
    unofficial-sodium::sodium
)

# Platform-specific libraries for secure storage
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Advapi32)
elseif(APPLE)
    find_library(SECURITY_FRAMEWORK Security)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SECURITY_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
elseif(UNIX AND NOT APPLE)
    # Linux: Try to find libsecret for secure storage
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSECRET libsecret-1)
        if(LIBSECRET_FOUND)
            add_definitions(-DHAVE_LIBSECRET)
            include_directories(${LIBSECRET_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSECRET_LIBRARIES})
            message(STATUS "libsecret found - secure storage enabled for Linux")
        else()
            message(WARNING "libsecret not found - Linux token storage will be INSECURE (plaintext)")
            message(WARNING "Install libsecret: sudo apt install libsecret-1-dev")
        endif()
    endif()
endif()
