cmake_minimum_required(VERSION 3.16)
project(DiscordClient VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt
if(WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.10.1/msvc2022_64")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebSockets Multimedia)

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
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::Multimedia
)
