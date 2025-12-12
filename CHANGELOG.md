# Changelog

All notable changes to CPPCord will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- Voice UI controls (mute/deafen buttons in chat)
- Speaking indicators for voice channels
- Rich embed rendering
- Markdown message formatting

---

## [0.1.0-alpha.1] - 2025-12-12

### Added
- **Authentication System**
  - User authentication via Discord REST API
  - Secure token storage using Windows Credential Manager
  - Auto-login on startup with saved credentials
  - Login/logout functionality

- **Guild & Channel Management**
  - Guild list display with icons (40x40px circular)
  - Guild icon downloading and caching from Discord CDN
  - Channel categorization and hierarchical display
  - Channel sorting by category and position
  - Guild sorting by join timestamp
  - Direct message (DM) support with recent activity sorting

- **Messaging**
  - Real-time message receiving via Discord Gateway (WebSocket)
  - Message sending with permission validation (SEND_MESSAGES)
  - Message display with Discord-like formatting
  - Message grouping (same author within 5-minute windows)
  - Date separators with visual dividers
  - Empty channel state indicators
  - Message history loading on channel switch
  - Scroll to bottom button for navigation

- **User Interface**
  - Discord-inspired native Qt Widgets interface
  - Three-panel layout (guilds, channels, messages)
  - User avatar downloading and caching
  - Avatar display in messages (40x40px circular)
  - Table-based message layout for proper alignment
  - Username and timestamp display
  - Timestamp formatting with locale support
  - Permission-aware UI states
  - Visual feedback for selected guilds/channels

- **Voice Chat (Experimental)**
  - Voice channel connection and WebSocket management
  - Real-time audio streaming (bidirectional)
  - Opus audio codec integration (v1.5.2)
  - Audio encoding/decoding (48kHz, stereo, 64kbps)
  - libsodium encryption (v1.0.20) with AEAD AES256-GCM
  - UDP RTP transport for voice data
  - Discord voice gateway protocol implementation
  - Microphone input capture
  - Speaker output playback
  - Support for rtpsize encryption mode
  - RTP header parsing and extension handling

- **Audio System**
  - AudioManager for input/output handling
  - Qt Multimedia integration
  - Proper audio device initialization
  - 20ms frame duration for voice packets

- **Architecture**
  - DiscordClient for REST API and main logic
  - GatewayClient for WebSocket real-time events
  - VoiceClient for voice channel handling
  - AudioManager for audio I/O
  - OpusCodec for audio encoding/decoding
  - Model classes (User, Guild, Channel, Message, etc.)
  - Snowflake ID handling

- **Developer Experience**
  - Cross-platform CMake build system
  - vcpkg dependency management
  - Qt 6.10.1 framework integration
  - C++23 modern features
  - Comprehensive README documentation
  - MIT License

### Fixed
- Message layout using table-based HTML for better QTextDocument support
- Avatar positioning fixed to stay at the left of username
- Proper vertical alignment of message components

### Known Issues
- Voice UI controls incomplete (no mute/deafen buttons in UI)
- No speaking indicators in voice channels
- No markdown rendering in messages
- No embed/attachment support
- No message reactions
- No typing indicators
- No system notifications
- No message editing/deletion
- No file uploads
- Limited error handling
- Memory usage not fully optimized
- Some edge cases in message grouping
- Voice chat may have occasional audio artifacts

### Technical Details
- **Platform**: Windows (primary), Linux, macOS (experimental)
- **Qt Version**: 6.10.1
- **C++ Standard**: C++23
- **Build System**: CMake 4.2.1
- **Dependencies**: Opus 1.5.2, libsodium 1.0.20
- **Compiler**: MSVC 2026 (Windows)

### Security Notes
- Token storage uses Windows Credential Manager (secure)
- Voice encryption uses AEAD AES256-GCM
- No plaintext credential storage
- TLS/HTTPS for all API requests

### Performance
- Native Qt rendering (no Electron/Chromium overhead)
- Efficient avatar/icon caching
- Low memory footprint compared to official client
- Fast startup time

---

## [0.0.1] - 2025-12-01

### Added
- Initial project setup
- Qt application framework
- Basic UI layout with three panels
- Project structure and build configuration
- vcpkg integration for dependency management
- Basic CMakeLists.txt configuration
- README with project overview

---

## Development Notes

### Versioning Strategy
- **v0.x.x**: Initial development, pre-production
- **v1.0.0**: First production-ready release (target: Q1 2026)
- Alpha releases: Feature-incomplete, expect bugs
- Beta releases: Feature-complete, stabilization phase
- Stable releases: Production-ready

### Contributing
See [RELEASE_GUIDE.md](RELEASE_GUIDE.md) for release process details.

---

[Unreleased]: https://github.com/doshibadev/cppcord/compare/v0.1.0-alpha.1...HEAD
[0.1.0-alpha.1]: https://github.com/doshibadev/cppcord/releases/tag/v0.1.0-alpha.1
[0.0.1]: https://github.com/doshibadev/cppcord/releases/tag/v0.0.1
