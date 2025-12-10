# CPPCord

A native Discord client built with C++ and Qt, designed as a lightweight alternative to the Electron-based official client.

## Project Goals

- **Native Performance**: Built with C++ and Qt for maximum efficiency and low resource usage
- **Cross-Platform**: Runs on Windows, macOS, and Linux
- **Feature Complete**: Full Discord API integration including messaging, voice chat, and real-time updates via WebSocket
- **No Electron**: A truly native application without the bloat of web technologies

## Why CPPCord?

The official Discord client is built with Electron, which bundles an entire Chromium browser. This project aims to provide the same Discord experience with:
- Lower memory footprint
- Better performance
- Native OS integration
- Faster startup times

## Technology Stack

- **Language**: C++17
- **GUI Framework**: Qt 6.10.1 (Widgets)
- **Build System**: CMake
- **Compiler**: MSVC 2022 (Windows) / GCC (Linux) / Clang (macOS)
- **Key Qt Modules**:
  - Qt Core - Base framework
  - Qt Widgets - UI components
  - Qt Network - HTTP/HTTPS REST API calls
  - Qt WebSockets - Discord Gateway connection
  - Qt Multimedia - Voice chat audio

## Current Status

🚧 **In Early Development** 🚧

### Implemented
- [x] Project setup and build system
- [x] Basic Qt application window

### Planned Features
- [ ] Discord OAuth2 authentication
- [ ] REST API integration (user info, guilds, channels)
- [ ] WebSocket gateway connection
- [ ] Real-time message receiving
- [ ] Message sending
- [ ] Server and channel navigation
- [ ] Rich message display (embeds, markdown, reactions)
- [ ] Voice chat support
- [ ] Direct messages
- [ ] User presence and status
- [ ] Notifications
- [ ] Settings and customization

## Building from Source

### Prerequisites

- **Qt 6.10.1** (MSVC 2022 64-bit on Windows)
  - Required modules: Core, Widgets, Network, WebSockets, Multimedia, SVG
- **CMake 3.16+**
- **C++17 compatible compiler**:
  - Windows: MSVC 2022
  - Linux: GCC 9+
  - macOS: Clang (Xcode)

### Windows Build Instructions

1. **Install Qt**
   - Download from [qt.io](https://www.qt.io/download-qt-installer)
   - Install Qt 6.10.1 with MSVC 2022 64-bit
   - Select components: Core, Widgets, Network, WebSockets, Multimedia, SVG

2. **Clone the repository**
   ```bash
   git clone https://github.com/doshibadev/cppcord.git
   cd cppcord
   ```

3. **Configure with CMake**
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64"
   ```

4. **Build**
   ```bash
   cmake --build . --config Debug
   ```

5. **Run**
   ```bash
   # Option 1: Add Qt bin to PATH
   set PATH=%PATH%;C:\Qt\6.10.1\msvc2022_64\bin
   Debug\DiscordClient.exe

   # Option 2: Use windeployqt to bundle DLLs
   C:\Qt\6.10.1\msvc2022_64\bin\windeployqt.exe Debug\DiscordClient.exe
   Debug\DiscordClient.exe
   ```

### Linux Build Instructions

```bash
# Install Qt (Ubuntu/Debian)
sudo apt install qt6-base-dev qt6-websockets-dev qt6-multimedia-dev

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
./DiscordClient
```

### macOS Build Instructions

```bash
# Install Qt via Homebrew
brew install qt@6

# Build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
make -j$(sysctl -n hw.ncpu)
./DiscordClient
```

## Project Structure

```
cppcord/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── src/
│   └── main.cpp           # Application entry point
└── build/                 # Build output (generated)
```

## Development

This project uses:
- **CMake** for cross-platform builds
- **Qt Creator** or **Visual Studio Code** with CMake extension for development
- **Git** for version control

### VSCode Setup

1. Install extensions:
   - C/C++ (Microsoft)
   - CMake Tools (Microsoft)

2. Open the project folder in VSCode

3. Configure CMake (Ctrl+Shift+P → "CMake: Configure")

4. Build (F7) and Run (Ctrl+F5)

## Distribution

When distributing the application:

### Windows
```bash
cd build/Release
C:\Qt\6.10.1\msvc2022_64\bin\windeployqt.exe DiscordClient.exe
# Zip the entire Release folder
```

### Linux
```bash
# Use linuxdeployqt or bundle Qt libraries manually
```

### macOS
```bash
# Use macdeployqt to create .app bundle
macdeployqt DiscordClient.app -dmg
```

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

### Development Priorities
1. Authentication flow
2. Basic messaging (send/receive)
3. Channel navigation
4. Voice chat
5. Polish and optimization

## License

MIT

## Disclaimer

This is an unofficial Discord client. Use at your own risk. The Discord API terms of service apply.

## Acknowledgments

- Discord for their API
- Qt Project for the excellent framework
- The open source community

## Contact

Discord Username: doshibadev

---

**Note**: This project is in active development. Features and APIs may change.
