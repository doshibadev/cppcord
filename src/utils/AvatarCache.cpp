#include "AvatarCache.h"
#include <QPainter>
#include <QPainterPath>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QBuffer>
#include <QDebug>

AvatarCache::AvatarCache(int maxCacheSize, QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this)),
      m_maxCacheSize(maxCacheSize),
      m_maxConcurrentDownloads(10) // Download max 10 avatars at once
{
    qDebug() << "AvatarCache initialized with LRU max size:" << m_maxCacheSize
             << "Max concurrent downloads:" << m_maxConcurrentDownloads;
}

AvatarCache::~AvatarCache()
{
}

QString AvatarCache::getAvatarUrl(Snowflake userId, const QString &avatarHash) const
{
    if (avatarHash.isEmpty())
        return QString();

    // Use size=64 for balance between quality and bandwidth
    QString extension = avatarHash.startsWith("a_") ? ".gif" : ".png";
    return QString("https://cdn.discordapp.com/avatars/%1/%2%3?size=64")
        .arg(userId)
        .arg(avatarHash)
        .arg(extension);
}

void AvatarCache::updateLRU(Snowflake userId)
{
    // Remove from current position if exists
    m_lruList.removeAll(userId);

    // Add to front (most recently used)
    m_lruList.prepend(userId);
}

void AvatarCache::evictOldest()
{
    while (m_cache.size() >= m_maxCacheSize && !m_lruList.isEmpty())
    {
        Snowflake oldestUserId = m_lruList.takeLast();
        m_cache.remove(oldestUserId);
        qDebug() << "LRU: Evicted avatar for user" << oldestUserId;
    }
}

QPixmap AvatarCache::decodeAvatar(const QByteArray &compressedData) const
{
    if (compressedData.isEmpty())
        return QPixmap();

    // Load from compressed data
    QPixmap source;
    if (!source.loadFromData(compressedData))
    {
        qWarning() << "Failed to decode compressed avatar data";
        return QPixmap();
    }

    // Create circular version (40x40 for message display)
    const int size = 40;
    QPixmap rounded(size, size);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Scale source to fit
    QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    // Center the scaled image
    int x = (scaled.width() - size) / 2;
    int y = (scaled.height() - size) / 2;

    // Create circular clip path
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    painter.setClipPath(path);

    // Draw the scaled image
    painter.drawPixmap(-x, -y, scaled);

    return rounded;
}

QPixmap AvatarCache::getAvatar(Snowflake userId, const QString &avatarHash)
{
    if (avatarHash.isEmpty())
        return QPixmap();

    // Check if in cache
    if (m_cache.contains(userId))
    {
        CachedAvatar &cached = m_cache[userId];

        // Update LRU (mark as recently used)
        updateLRU(userId);

        // If not decoded yet, decode now
        if (!cached.hasDecoded)
        {
            cached.decodedPixmap = decodeAvatar(cached.compressedData);
            cached.hasDecoded = true;
        }

        return cached.decodedPixmap;
    }

    // Not cached - add to queue or start download
    if (!m_pendingDownloads.contains(userId))
    {
        // Check if already in queue
        bool inQueue = false;
        for (const auto &pair : m_downloadQueue)
        {
            if (pair.first == userId)
            {
                inQueue = true;
                break;
            }
        }

        if (!inQueue)
        {
            // Add to queue and process
            m_downloadQueue.append(qMakePair(userId, avatarHash));
            processDownloadQueue();
        }
    }

    return QPixmap(); // Return null, will signal when ready
}

bool AvatarCache::hasAvatar(Snowflake userId) const
{
    return m_cache.contains(userId);
}

void AvatarCache::preloadAvatars(const QMap<Snowflake, QString> &userAvatars)
{
    for (auto it = userAvatars.constBegin(); it != userAvatars.constEnd(); ++it)
    {
        Snowflake userId = it.key();
        QString avatarHash = it.value();

        if (avatarHash.isEmpty())
            continue;

        // Only add to queue if not cached and not already pending/queued
        if (!m_cache.contains(userId) && !m_pendingDownloads.contains(userId))
        {
            // Check if already in queue
            bool inQueue = false;
            for (const auto &pair : m_downloadQueue)
            {
                if (pair.first == userId)
                {
                    inQueue = true;
                    break;
                }
            }

            if (!inQueue)
            {
                m_downloadQueue.append(qMakePair(userId, avatarHash));
            }
        }
    }

    // Process the queue
    processDownloadQueue();
}

void AvatarCache::processDownloadQueue()
{
    // Start downloads up to the max concurrent limit
    while (!m_downloadQueue.isEmpty() && m_pendingDownloads.size() < m_maxConcurrentDownloads)
    {
        auto pair = m_downloadQueue.takeFirst();
        startDownload(pair.first, pair.second);
    }
}

void AvatarCache::clearCache()
{
    m_cache.clear();
    m_lruList.clear();
    qDebug() << "Avatar cache cleared";
}

void AvatarCache::startDownload(Snowflake userId, const QString &avatarHash)
{
    QString url = getAvatarUrl(userId, avatarHash);
    if (url.isEmpty())
        return;

    m_pendingDownloads.insert(userId);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    // Set priority to low for avatars to not block other requests
    request.setPriority(QNetworkRequest::LowPriority);

    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, userId]()
            {
        m_pendingDownloads.remove(userId);

        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray compressedData = reply->readAll();

            if (!compressedData.isEmpty())
            {
                // Evict oldest if needed before adding
                if (m_cache.size() >= m_maxCacheSize)
                {
                    evictOldest();
                }

                // Store compressed data
                CachedAvatar cached;
                cached.compressedData = compressedData;
                cached.hasDecoded = false;  // Decode lazily when needed

                m_cache[userId] = cached;
                updateLRU(userId);

                // Notify that avatar is ready
                emit avatarReady(userId);
            }
        }
        else
        {
            qWarning() << "Avatar download failed for user:" << userId << "-" << reply->errorString();
        }

        reply->deleteLater();

        // Process next items in queue
        processDownloadQueue(); });
}
