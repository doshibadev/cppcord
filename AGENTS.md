# Agent Guide: CPPCord

## Build & Test
- **Build**: `cmake -B build -S . && cmake --build build`
- **Run**: `./build/DiscordClient` (Linux/Mac) or `build\Debug\DiscordClient.exe` (Windows)
- **Tests**: currently no test suite. Validate changes by running the app.

## Code Style & Conventions
- **Stack**: C++17, Qt 6.10.1, CMake.
- **Naming**:
  - Classes: PascalCase (`DiscordAPI`)
  - Methods: camelCase (`submitMFA`)
  - Members: `m_` prefix + camelCase (`m_token`)
- **Memory**: Use `QObject` parent-child trees for automatic cleanup.
- **Async**: Use Signals & Slots. Do not block the main thread.
- **Strings**: Always use `QString`, never `std::string` unless interacting with non-Qt libs.
- **Error Handling**: Emit error signals (e.g., `apiError(QString)`). Do not use exceptions.
