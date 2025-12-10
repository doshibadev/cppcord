#pragma once

#include <QString>
#include <QByteArray>

/**
 * @brief Secure token storage using OS-native credential managers
 *
 * Platform-specific implementations:
 * - Windows: Windows Credential Manager (CredRead/CredWrite) - SECURE ✓
 * - macOS: Keychain Services - SECURE ✓
 * - Linux: libsecret (GNOME Keyring/KWallet) - SECURE ✓
 *   Fallback: Encrypted file with user-only permissions - INSECURE ⚠️
 *
 * Security Notes:
 * - Windows & macOS: Uses OS-provided secure storage with proper encryption
 * - Linux with libsecret: Integrates with system keyring (GNOME Keyring, KWallet)
 * - Linux without libsecret: Falls back to plaintext with file permissions
 *   (Install libsecret-1-dev for secure storage on Linux)
 */
class TokenStorage
{
public:
    TokenStorage();

    // Save token securely to OS credential store
    bool saveToken(const QString &token);

    // Load token from OS credential store
    QString loadToken() const;

    // Check if token exists
    bool hasToken() const;

    // Clear stored token
    bool clearToken();

private:
    static constexpr char SERVICE_NAME[] = "CPPCord";
    static constexpr char ACCOUNT_NAME[] = "discord_token";

#ifdef Q_OS_WIN
    bool saveTokenWindows(const QString &token);
    QString loadTokenWindows() const;
    bool deleteTokenWindows();
#elif defined(Q_OS_MACOS)
    bool saveTokenMacOS(const QString &token);
    QString loadTokenMacOS() const;
    bool deleteTokenMacOS();
#else
    // Linux fallback - uses QSettings with system encryption
    bool saveTokenLinux(const QString &token);
    QString loadTokenLinux() const;
    bool deleteTokenLinux();
#endif
};
