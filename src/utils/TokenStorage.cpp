#include "TokenStorage.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#elif defined(Q_OS_MACOS)
#include <Security/Security.h>
#else
// Linux
#ifdef HAVE_LIBSECRET
#include <libsecret/secret.h>
#endif
#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#endif

TokenStorage::TokenStorage()
{
}

bool TokenStorage::saveToken(const QString &token)
{
    if (token.isEmpty())
    {
        return clearToken();
    }

#ifdef Q_OS_WIN
    return saveTokenWindows(token);
#elif defined(Q_OS_MACOS)
    return saveTokenMacOS(token);
#else
    return saveTokenLinux(token);
#endif
}

QString TokenStorage::loadToken() const
{
#ifdef Q_OS_WIN
    return loadTokenWindows();
#elif defined(Q_OS_MACOS)
    return loadTokenMacOS();
#else
    return loadTokenLinux();
#endif
}

bool TokenStorage::hasToken() const
{
    QString token = loadToken();
    return !token.isEmpty();
}

bool TokenStorage::clearToken()
{
#ifdef Q_OS_WIN
    return deleteTokenWindows();
#elif defined(Q_OS_MACOS)
    return deleteTokenMacOS();
#else
    return deleteTokenLinux();
#endif
}

// ============================================================================
// Windows Implementation - Uses Windows Credential Manager
// ============================================================================
#ifdef Q_OS_WIN

bool TokenStorage::saveTokenWindows(const QString &token)
{
    QByteArray tokenData = token.toUtf8();
    QString serviceName = QString::fromUtf8(SERVICE_NAME);
    QString accountName = QString::fromUtf8(ACCOUNT_NAME);

    CREDENTIALW cred = {0};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = (LPWSTR)serviceName.utf16();
    cred.CredentialBlobSize = tokenData.size();
    cred.CredentialBlob = (LPBYTE)tokenData.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = (LPWSTR)accountName.utf16();

    if (CredWriteW(&cred, 0))
    {
        qDebug() << "Token saved to Windows Credential Manager";
        return true;
    }

    qWarning() << "Failed to save token to Windows Credential Manager:" << GetLastError();
    return false;
}

QString TokenStorage::loadTokenWindows() const
{
    PCREDENTIALW cred = nullptr;
    QString serviceName = QString::fromUtf8(SERVICE_NAME);

    if (CredReadW((LPWSTR)serviceName.utf16(), CRED_TYPE_GENERIC, 0, &cred))
    {
        QString token = QString::fromUtf8((char *)cred->CredentialBlob, cred->CredentialBlobSize);
        CredFree(cred);
        qDebug() << "Token loaded from Windows Credential Manager";
        return token;
    }

    DWORD error = GetLastError();
    if (error != ERROR_NOT_FOUND)
    {
        qWarning() << "Failed to read token from Windows Credential Manager:" << error;
    }

    return QString();
}

bool TokenStorage::deleteTokenWindows()
{
    QString serviceName = QString::fromUtf8(SERVICE_NAME);

    if (CredDeleteW((LPWSTR)serviceName.utf16(), CRED_TYPE_GENERIC, 0))
    {
        qDebug() << "Token deleted from Windows Credential Manager";
        return true;
    }

    DWORD error = GetLastError();
    if (error == ERROR_NOT_FOUND)
    {
        return true; // Already deleted
    }

    qWarning() << "Failed to delete token from Windows Credential Manager:" << error;
    return false;
}

#endif

// ============================================================================
// macOS Implementation - Uses Keychain Services
// ============================================================================
#ifdef Q_OS_MACOS

bool TokenStorage::saveTokenMacOS(const QString &token)
{
    QByteArray tokenData = token.toUtf8();
    QByteArray serviceName = QByteArray(SERVICE_NAME);
    QByteArray accountName = QByteArray(ACCOUNT_NAME);

    // Try to update existing item first
    SecKeychainItemRef itemRef = nullptr;
    OSStatus status = SecKeychainFindGenericPassword(
        nullptr,
        serviceName.size(), serviceName.data(),
        accountName.size(), accountName.data(),
        nullptr, nullptr,
        &itemRef);

    if (status == errSecSuccess && itemRef != nullptr)
    {
        // Update existing item
        status = SecKeychainItemModifyAttributesAndData(
            itemRef,
            nullptr,
            tokenData.size(),
            tokenData.data());
        CFRelease(itemRef);

        if (status == errSecSuccess)
        {
            qDebug() << "Token updated in macOS Keychain";
            return true;
        }
    }
    else
    {
        // Create new item
        status = SecKeychainAddGenericPassword(
            nullptr,
            serviceName.size(), serviceName.data(),
            accountName.size(), accountName.data(),
            tokenData.size(), tokenData.data(),
            nullptr);

        if (status == errSecSuccess)
        {
            qDebug() << "Token saved to macOS Keychain";
            return true;
        }
    }

    qWarning() << "Failed to save token to macOS Keychain:" << status;
    return false;
}

QString TokenStorage::loadTokenMacOS() const
{
    QByteArray serviceName = QByteArray(SERVICE_NAME);
    QByteArray accountName = QByteArray(ACCOUNT_NAME);

    void *passwordData = nullptr;
    UInt32 passwordLength = 0;

    OSStatus status = SecKeychainFindGenericPassword(
        nullptr,
        serviceName.size(), serviceName.data(),
        accountName.size(), accountName.data(),
        &passwordLength, &passwordData,
        nullptr);

    if (status == errSecSuccess)
    {
        QString token = QString::fromUtf8((char *)passwordData, passwordLength);
        SecKeychainItemFreeContent(nullptr, passwordData);
        qDebug() << "Token loaded from macOS Keychain";
        return token;
    }

    if (status != errSecItemNotFound)
    {
        qWarning() << "Failed to read token from macOS Keychain:" << status;
    }

    return QString();
}

bool TokenStorage::deleteTokenMacOS()
{
    QByteArray serviceName = QByteArray(SERVICE_NAME);
    QByteArray accountName = QByteArray(ACCOUNT_NAME);

    SecKeychainItemRef itemRef = nullptr;
    OSStatus status = SecKeychainFindGenericPassword(
        nullptr,
        serviceName.size(), serviceName.data(),
        accountName.size(), accountName.data(),
        nullptr, nullptr,
        &itemRef);

    if (status == errSecSuccess && itemRef != nullptr)
    {
        status = SecKeychainItemDelete(itemRef);
        CFRelease(itemRef);

        if (status == errSecSuccess)
        {
            qDebug() << "Token deleted from macOS Keychain";
            return true;
        }
    }
    else if (status == errSecItemNotFound)
    {
        return true; // Already deleted
    }

    qWarning() << "Failed to delete token from macOS Keychain:" << status;
    return false;
}

#endif

// ============================================================================
// Linux Implementation - Uses libsecret (GNOME Keyring/KWallet)
// ============================================================================
#ifndef Q_OS_WIN
#ifndef Q_OS_MACOS

#ifdef HAVE_LIBSECRET

// Schema for libsecret
static const SecretSchema cppcord_schema = {
    "org.cppcord.DiscordClient",
    SECRET_SCHEMA_NONE,
    {{"service", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"account", SECRET_SCHEMA_ATTRIBUTE_STRING},
     {"NULL", (SecretSchemaAttributeType)0}}};

bool TokenStorage::saveTokenLinux(const QString &token)
{
    GError *error = nullptr;
    QByteArray tokenData = token.toUtf8();

    bool success = secret_password_store_sync(
        &cppcord_schema,
        SECRET_COLLECTION_DEFAULT,
        "CPPCord Discord Token",
        tokenData.constData(),
        nullptr,
        &error,
        "service", SERVICE_NAME,
        "account", ACCOUNT_NAME,
        nullptr);

    if (error != nullptr)
    {
        qWarning() << "Failed to save token to libsecret:" << error->message;
        g_error_free(error);
        return false;
    }

    if (success)
    {
        qDebug() << "Token saved to GNOME Keyring/KWallet via libsecret (SECURE)";
    }

    return success;
}

QString TokenStorage::loadTokenLinux() const
{
    GError *error = nullptr;

    gchar *password = secret_password_lookup_sync(
        &cppcord_schema,
        nullptr,
        &error,
        "service", SERVICE_NAME,
        "account", ACCOUNT_NAME,
        nullptr);

    if (error != nullptr)
    {
        if (error->code != SECRET_ERROR_NO_SUCH_OBJECT)
        {
            qWarning() << "Failed to load token from libsecret:" << error->message;
        }
        g_error_free(error);
        return QString();
    }

    if (password == nullptr)
    {
        return QString();
    }

    QString token = QString::fromUtf8(password);
    secret_password_free(password);

    qDebug() << "Token loaded from GNOME Keyring/KWallet via libsecret";
    return token;
}

bool TokenStorage::deleteTokenLinux()
{
    GError *error = nullptr;

    bool success = secret_password_clear_sync(
        &cppcord_schema,
        nullptr,
        &error,
        "service", SERVICE_NAME,
        "account", ACCOUNT_NAME,
        nullptr);

    if (error != nullptr)
    {
        qWarning() << "Failed to delete token from libsecret:" << error->message;
        g_error_free(error);
        return false;
    }

    qDebug() << "Token deleted from GNOME Keyring/KWallet via libsecret";
    return success;
}

#else
// Fallback for Linux without libsecret - INSECURE

bool TokenStorage::saveTokenLinux(const QString &token)
{
    // Store in user-only readable config directory
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configPath);

    QString filePath = configPath + "/credentials.conf";
    QSettings settings(filePath, QSettings::IniFormat);

    settings.setValue("auth/token", token);
    settings.sync();

    // Set file permissions to user-only read/write
    QFile file(filePath);
    if (file.exists())
    {
        file.setPermissions(QFile::ReadUser | QFile::WriteUser);
    }

    qWarning() << "=================================================================";
    qWarning() << "WARNING: Token stored in PLAINTEXT on Linux (INSECURE!)";
    qWarning() << "Install libsecret for secure storage: sudo apt install libsecret-1-dev";
    qWarning() << "Then rebuild with: cmake -B build -S . && cmake --build build";
    qWarning() << "=================================================================";

    return true;
}

QString TokenStorage::loadTokenLinux() const
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString filePath = configPath + "/credentials.conf";

    if (!QFile::exists(filePath))
    {
        return QString();
    }

    QSettings settings(filePath, QSettings::IniFormat);
    QString token = settings.value("auth/token").toString();

    if (!token.isEmpty())
    {
        qDebug() << "Token loaded from plaintext config file (INSECURE)";
    }

    return token;
}

bool TokenStorage::deleteTokenLinux()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString filePath = configPath + "/credentials.conf";

    QSettings settings(filePath, QSettings::IniFormat);
    settings.remove("auth/token");
    settings.sync();

    qDebug() << "Token cleared from config file";
    return true;
}

#endif // HAVE_LIBSECRET

#endif
#endif
