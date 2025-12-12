#pragma once
#include <QObject>
#include <QPixmap>
#include <QByteArray>
#include <QList>
#include <QHash>
#include <QSet>
#include <QString>
#include <QNetworkAccessManager>
#include "models/Snowflake.h"

/**
 * @brief Discord-style avatar cache with LRU eviction and compressed storage
 *
 * Features:
 * - LRU cache with max 300 avatars (~3-8 MB RAM)
 * - Stores compressed PNG data (~5-20KB each)
 * - Lazy decoding only when rendering
 * - Async downloads never block UI
 */
class AvatarCache : public QObject
{
    Q_OBJECT

public:
    explicit AvatarCache(int maxCacheSize = 300, QObject *parent = nullptr);
    ~AvatarCache();

    /**
     * @brief Get avatar if cached, otherwise trigger async download
     * @return Decoded circular avatar, or null if not ready
     */
    QPixmap getAvatar(Snowflake userId, const QString &avatarHash);

    /**
     * @brief Check if avatar is cached
     */
    bool hasAvatar(Snowflake userId) const;

    /**
     * @brief Preload avatars asynchronously
     */
    void preloadAvatars(const QMap<Snowflake, QString> &userAvatars);

    /**
     * @brief Clear all cache
     */
    void clearCache();

signals:
    void avatarReady(Snowflake userId);

private:
    struct CachedAvatar
    {
        QByteArray compressedData; // ~5-20 KB compressed
        QPixmap decodedPixmap;     // Cached decoded version
        bool hasDecoded = false;
    };

    QNetworkAccessManager *m_networkManager;
    QHash<Snowflake, CachedAvatar> m_cache;
    QList<Snowflake> m_lruList; // Most recent at front
    QSet<Snowflake> m_pendingDownloads;
    QList<QPair<Snowflake, QString>> m_downloadQueue; // Queue for batched downloads
    int m_maxCacheSize;
    int m_maxConcurrentDownloads;

    QString getAvatarUrl(Snowflake userId, const QString &avatarHash) const;
    void updateLRU(Snowflake userId);
    void evictOldest();
    QPixmap decodeAvatar(const QByteArray &compressedData) const;
    void startDownload(Snowflake userId, const QString &avatarHash);
    void processDownloadQueue();
};
