cmake_minimum_required(VERSION 3.16)
project(DiscordClient VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt
set(CMAKE_PREFIX_PATH "C:\\Qt\\6.10.1\\msvc2022_64") # Adjust to your Qt install path
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebSockets)

# Auto-generate Qt meta-object code
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Source files
set(SOURCES
    src/main.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link Qt libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)
