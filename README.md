<div align="center">

<img src=".github/assets/cppcord_icon.png" alt="CPPCord Logo" width="200"/>

# CPPCord

![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![C++](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6.10.1-41CD52?logo=qt)
![Status](https://img.shields.io/badge/status-active%20development-orange)

A native, high-performance Discord client built with modern C++ and Qt framework.

[Features](#features) • [Installation](#installation) • [Building](#building-from-source) • [Contributing](#contributing)

</div>

---

## Overview

CPPCord is a native Discord client designed as a lightweight, high-performance alternative to the official Electron-based client. Built entirely with C++23 and Qt 6, it provides a complete Discord experience with significantly lower resource usage and better system integration.

### Why CPPCord?

The official Discord client is built with Electron, bundling an entire Chromium browser instance. CPPCord aims to deliver the same functionality with:

- **Lower Memory Footprint** - Native code with minimal overhead
- **Better Performance** - Direct system API access and optimized rendering
- **Native OS Integration** - Secure credential storage and system notifications
- **Faster Startup** - No JavaScript engine initialization required

## Features

### Authentication & Security
- User account authentication (non-bot)
- Secure token storage using platform-native credential managers:
  - **Windows**: Windows Credential Manager
  - **macOS**: Keychain Services
  - **Linux**: libsecret (GNOME Keyring/KWallet)

### Messaging
- Real-time message sending and receiving via Discord Gateway (WebSocket)
- Discord-like message display with HTML/CSS rendering
- Message grouping (same author within 5-minute windows)
- User avatar caching and display
- Date separators with visual dividers
- Permission-based message input (SEND_MESSAGES check)
- Empty channel state indicators

### Guilds & Channels
- Guild navigation with icon display (circular, 40x40px)
- Guild sorting by join timestamp
- Channel categorization and hierarchical display
- Channel sorting by category and position
- Direct message support with recent activity sorting
- Guild icon downloading from Discord CDN

### Voice Chat
- Full voice chat implementation with real-time audio streaming
- Opus audio codec integration (v1.5.2) for encoding/decoding
- libsodium encryption (v1.0.20) with AEAD AES256-GCM support
- Discord voice gateway connection and UDP RTP transport
- Bidirectional audio: microphone input and speaker output
- Support for Discord's rtpsize encryption modes
- Proper RTP header parsing and extension handling

### User Interface
- Native Qt Widgets-based interface
- Fixed-position message layout matching Discord's design
- Responsive avatar and username positioning
- Timestamp formatting with locale support
- Permission-aware UI states

## Technology Stack

| Component | Technology |
|-----------|-----------|
| **Language** | C++23 |
| **GUI Framework** | Qt 6.10.1 (Widgets) |
| **Build System** | CMake 4.2.1 |
| **Package Manager** | vcpkg |
| **Networking** | Qt Network (REST), Qt WebSockets (Gateway) |
| **Audio Codec** | Opus 1.5.2 |
| **Encryption** | libsodium 1.0.20 |
| **Compiler** | MSVC 2026 (Windows) / GCC 15.2 (Linux) / Clang 21.1.0 (macOS) |

### Qt Modules
- **Qt Core** - Base framework and utilities
- **Qt Widgets** - Native UI components
- **Qt Network** - HTTP/HTTPS REST API communication
- **Qt WebSockets** - Discord Gateway real-time connection
- **Qt Multimedia** - Audio processing for voice chat

## Current Status

### Implemented
- [x] Project infrastructure and build system
- [x] Qt application framework
- [x] User authentication flow
- [x] Secure token storage (platform-native)
- [x] Discord REST API integration
- [x] WebSocket Gateway connection
- [x] Guild and channel fetching
- [x] Channel categorization and sorting
- [x] Real-time message receiving
- [x] Message sending with permission validation
- [x] Discord-like message display (avatars, grouping, separators)
- [x] User avatar downloading and caching
- [x] Guild icon display
- [x] Direct message support
- [x] DM sorting by recent activity
- [x] Voice library integration (Opus, libsodium)
- [x] Voice channel connection and audio streaming
- [x] Full bidirectional voice chat (input/output)
- [x] AEAD AES256-GCM encryption for voice

### In Progress
- [ ] Voice UI (mute/deafen controls, speaking indicators)

### Planned Features
- [ ] Rich message content (embeds, attachments, reactions)
- [ ] Markdown rendering
- [ ] User presence and status updates
- [ ] Typing indicators
- [ ] System notifications
- [ ] User settings and preferences
- [ ] Theme customization
- [ ] Message history pagination
- [ ] File uploads
- [ ] Emoji picker


## Installation

### Prerequisites

Ensure you have the following installed on your system:

- **Qt 6.10.1+**
  - Download from [qt.io](https://www.qt.io/download-qt-installer)
  - Required modules: Core, Widgets, Network, WebSockets, Multimedia
- **CMake 4.2.1+**
- **vcpkg** (package manager for C++ libraries)
- **C++23 compatible compiler**:
  - Windows: Visual Studio 2022 (v17.x) or 2026 (v18.x) with MSVC
  - Linux: GCC 15.2+ or Clang 21.1.O+
  - macOS: Xcode 26.2+ (includes Clang 21.1.0+)

### vcpkg Setup

CPPCord uses vcpkg for dependency management. Set up vcpkg if not already installed:

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# Bootstrap (Windows)
.\bootstrap-vcpkg.bat

# Bootstrap (Linux/macOS)
./bootstrap-vcpkg.sh

# Set environment variable
# Windows (PowerShell)
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\path\to\vcpkg', 'User')

# Linux/macOS (add to .bashrc or .zshrc)
export VCPKG_ROOT=/path/to/vcpkg
```

## Building from Source

### Windows

```powershell
# Clone the repository
git clone https://github.com/doshibadev/cppcord.git
cd cppcord

# Configure CMake (vcpkg will auto-install dependencies)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

# Build
cmake --build build --config Release

# Deploy Qt dependencies
C:\Qt\6.10.1\msvc2022_64\bin\windeployqt.exe build\Release\DiscordClient.exe

# Run
.\build\Release\DiscordClient.exe
```

### Linux

```bash
# Install system dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install build-essential cmake git
sudo apt install qt6-base-dev qt6-websockets-dev qt6-multimedia-dev
sudo apt install libsecret-1-dev  # For secure token storage

# Clone the repository
git clone https://github.com/doshibadev/cppcord.git
cd cppcord

# Configure and build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release -j$(nproc)

# Run
./build/DiscordClient
```

### macOS

```bash
# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install Qt
brew install qt@6

# Clone the repository
git clone https://github.com/doshibadev/cppcord.git
cd cppcord

# Configure and build
cmake -B build -S . \
  -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release -j$(sysctl -n hw.ncpu)

# Run
./build/DiscordClient
```

## Project Structure

```
cppcord/
├── CMakeLists.txt              # Build configuration
├── vcpkg.json                  # Dependency manifest
├── README.md                   # Documentation
├── LICENSE                     # MIT License
├── src/
│   ├── main.cpp               # Application entry point
│   ├── core/
│   │   └── (Core application logic)
│   ├── ui/
│   │   ├── LoginDialog.cpp    # Authentication UI
│   │   └── MainWindow.cpp     # Main application window
│   ├── network/
│   │   ├── DiscordClient.cpp  # Discord REST API client
│   │   └── GatewayClient.cpp  # WebSocket Gateway handler
│   ├── models/
│   │   ├── Guild.h            # Guild data structures
│   │   ├── Channel.h          # Channel data structures
│   │   ├── Message.h          # Message data structures
│   │   └── User.h             # User data structures
│   └── utils/
│       └── TokenStorage.cpp   # Secure credential storage
└── build/                      # Build output (generated)
```

## Development

### Development Environment

**Recommended IDEs:**
- **Qt Creator** - Best integration with Qt framework
- **Visual Studio** (Windows) - Excellent C++ debugging
- **Visual Studio Code** - Lightweight with CMake extension
- **CLion** - Full-featured C++ IDE

### VS Code Setup

1. Install required extensions:
   ```
   - C/C++ (Microsoft)
   - CMake Tools (Microsoft)
   - Qt tools (tonka3000)
   ```

2. Open workspace:
   ```bash
   code cppcord
   ```

3. Configure CMake:
   - Press `Ctrl+Shift+P`
   - Select "CMake: Configure"
   - Choose your kit (e.g., "Visual Studio Community 2022 Release - amd64")

4. Build and run:
   - Build: `F7`
   - Run: `Ctrl+F5`

### Code Style

- Follow [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- Use Qt naming conventions for Qt-related code
- 4-space indentation
- Descriptive variable and function names
- Document public APIs with comments

### Testing

```bash
# Run unit tests (when implemented)
cd build
ctest --output-on-failure
```


## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                         UI Layer                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ LoginDialog  │  │  MainWindow  │  │ Message View │       │
│  └──────┬───────┘  └───────┬──────┘  └────────┬─────┘       │
└─────────┼──────────────────┼──────────────────┼─────────────┘
          │                  │                  │
┌─────────┼──────────────────┼──────────────────┼─────────────┐
│         │            Network Layer            │             │
│  ┌──────▼───────┐  ┌───────▼──────┐  ┌────────▼─────┐       │
│  │DiscordClient │  │GatewayClient │  │   CDN (HTTP) │       │
│  │  (REST API)  │  │ (WebSocket)  │  │              │       │
│  └──────┬───────┘  └───────┬──────┘  └────────┬─────┘       │
└─────────┼──────────────────┼──────────────────┼─────────────┘
          │                  │                  │
┌─────────┼──────────────────┼──────────────────┼─────────────┐
│         │             Data Models             │             │
│  ┌──────▼───────┬──────────▼──────┬───────────▼──────┐      │
│  │    Guild     │    Channel      │    Message       │      │
│  │    User      │    Role         │    Permissions   │      │
│  └──────────────┴─────────────────┴──────────────────┘      │
└─────────────────────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────────┐
│                    Utilities & Storage                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │TokenStorage  │  │ Avatar Cache │  │  Icon Cache  │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

### Key Components

**DiscordClient (REST API)**
- Handles HTTP requests to Discord's REST API
- Manages authentication and token refresh
- Fetches guilds, channels, messages, and user data
- Implements rate limiting and error handling

**GatewayClient (WebSocket)**
- Maintains persistent WebSocket connection to Discord Gateway
- Handles real-time events (MESSAGE_CREATE, GUILD_UPDATE, etc.)
- Implements heartbeat and reconnection logic
- Processes and dispatches gateway events

**TokenStorage**
- Platform-native secure credential storage
- Windows: Credential Manager API
- macOS: Keychain Services API
- Linux: libsecret (GNOME Keyring/KWallet)

**MainWindow**
- Primary application interface
- Guild and channel navigation
- Message display with Discord-like formatting
- Input handling and permission validation

## API Integration

### Discord API Version
CPPCord uses Discord API v9 with the following endpoints:

**Authentication**
- `GET /users/@me` - Current user information
- Token-based authentication (user accounts, not bots)

**Guilds**
- `GET /users/@me/guilds` - List user's guilds
- `GET /guilds/{guild.id}/channels` - Guild channels

**Channels**
- `GET /channels/{channel.id}/messages` - Message history
- `POST /channels/{channel.id}/messages` - Send messages
- Permission validation before message operations

**Gateway (WebSocket)**
- Real-time event streaming
- Opcode handling (HELLO, HEARTBEAT, IDENTIFY, DISPATCH)
- Event types: MESSAGE_CREATE, GUILD_CREATE, READY, etc.

**CDN**
- User avatars: `https://cdn.discordapp.com/avatars/{user.id}/{avatar.hash}.png`
- Guild icons: `https://cdn.discordapp.com/icons/{guild.id}/{icon.hash}.png`

## Distribution

### Creating Release Builds

**Windows**
```powershell
# Build Release
cmake --build build --config Release

# Deploy Qt dependencies
C:\Qt\6.10.1\msvc2022_64\bin\windeployqt.exe build\Release\DiscordClient.exe

# Create installer (using NSIS or WiX)
# Package the Release folder into an installer
```

**Linux**
```bash
# Build Release
cmake --build build --config Release

# Create AppImage (recommended)
# Or use linuxdeployqt for bundling Qt libraries
```

**macOS**
```bash
# Build Release
cmake --build build --config Release

# Create .app bundle
macdeployqt build/DiscordClient.app -dmg
```

## Contributing

Contributions are welcome and appreciated! Whether you're fixing bugs, adding features, or improving documentation, your help makes CPPCord better.

### How to Contribute

1. **Fork the repository**
   ```bash
   # Click "Fork" on GitHub, then clone your fork
   git clone https://github.com/doshibadev/cppcord.git
   ```

2. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Make your changes**
   - Write clean, documented code
   - Follow existing code style
   - Test your changes thoroughly

4. **Commit and push**
   ```bash
   git add .
   git commit -m "Add: description of your changes"
   git push origin feature/your-feature-name
   ```

5. **Open a Pull Request**
   - Describe your changes
   - Reference any related issues
   - Wait for review

### Development Priorities

Current focus areas for contributions:

1. **Voice Chat** - Complete voice channel connection and audio streaming
2. **Rich Messages** - Embed rendering, markdown support, reactions
3. **Notifications** - System notifications for new messages
4. **Settings** - User preferences and customization
5. **Performance** - Memory optimization and rendering improvements
6. **Testing** - Unit tests and integration tests

### Reporting Issues

Found a bug? Have a feature request?

- Check existing issues first
- Use the GitHub issue tracker
- Provide detailed information:
  - Operating system and version
  - Qt version
  - Steps to reproduce
  - Expected vs actual behavior
  - Screenshots if applicable

## Security

### Responsible Disclosure

If you discover a security vulnerability, please report it privately:

- **Do not** open a public issue
- Email: security@cppcord.dev (or contact via GitHub)
- Provide details about the vulnerability
- Allow time for a fix before public disclosure

### Security Features

- Token encryption using platform-native credential stores
- No plaintext password storage
- Secure HTTPS connections for all API requests
- WebSocket TLS encryption (wss://)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

```
MIT License

Copyright (c) 2025 doshibadev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Disclaimer

**Important Legal Notice**

- CPPCord is an **unofficial** Discord client
- Not affiliated with, endorsed by, or sponsored by Discord Inc.
- Use at your own risk
- Discord's [Terms of Service](https://discord.com/terms) and [Community Guidelines](https://discord.com/guidelines) apply
- Self-botting and automation may violate Discord's ToS
- The developers are not responsible for any account actions taken by Discord

## Acknowledgments

This project stands on the shoulders of giants:

- **Discord** - For providing the API and platform
- **Qt Project** - For the excellent cross-platform framework
- **vcpkg** - For seamless C++ dependency management
- **Opus** - For high-quality audio codec
- **libsodium** - For modern cryptography
- **Open Source Community** - For tools, libraries, and inspiration

## Roadmap

### Version 0.3.0 (Next Release)
- [ ] Complete voice chat implementation
- [ ] Voice UI controls (mute, deafen, volume)
- [ ] Speaking indicators
- [ ] Rich embed rendering

### Version 0.4.0
- [ ] Markdown message formatting
- [ ] Reaction support
- [ ] Message editing and deletion
- [ ] File upload support

### Version 0.5.0
- [ ] System notifications
- [ ] User settings panel
- [ ] Theme customization
- [ ] Typing indicators

### Version 1.0.0 (Stable)
- [ ] Feature parity with essential Discord functionality
- [ ] Comprehensive testing suite
- [ ] Performance optimization
- [ ] Documentation completion

## Support

### Getting Help

- **Documentation**: Check this README and code comments
- **Issues**: Browse or create [GitHub Issues](https://github.com/doshibadev/cppcord/issues)
- **Discussions**: Join [GitHub Discussions](https://github.com/doshibadev/cppcord/discussions)

### Contact

- **GitHub**: [@doshibadev](https://github.com/doshibadev)
- **Discord**: doshibadev

---

<div align="center">

**[⬆ Back to Top](#cppcord)**

Made with passion for native applications and efficient code.

*Star this repository if you find it useful!*

</div>
