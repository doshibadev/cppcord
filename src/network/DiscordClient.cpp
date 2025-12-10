#include "DiscordClient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QPixmap>
#include <algorithm> // for std::sort

DiscordClient::DiscordClient(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_gateway(new GatewayClient(this)), m_fingerprint(generateFingerprint())
{
    connect(m_gateway, &GatewayClient::eventReceived, this, &DiscordClient::handleGatewayEvent);
}

void DiscordClient::loginWithToken(const QString &token)
{
    if (token.isEmpty())
    {
        qWarning() << "loginWithToken called with empty token";
        emit loginError("Invalid token");
        return;
    }

    qDebug() << "loginWithToken: Setting token and connecting to gateway";
    m_token = token;
    // Don't save again - token is already saved
    emit loginSuccess();
    m_gateway->connectToGateway(m_token);
}

void DiscordClient::logout()
{
    m_tokenStorage.clearToken();
    m_token.clear();
    m_gateway->disconnectFromGateway();
    m_guilds.clear();
    m_privateChannels.clear();
}

void DiscordClient::login(const QString &email, const QString &password)
{
    QNetworkRequest request = createRequest("/api/v9/auth/login");

    QJsonObject loginData;
    loginData["login"] = email;
    loginData["password"] = password;
    loginData["undelete"] = false;
    loginData["login_source"] = QJsonValue::Null;
    loginData["gift_code_sku_id"] = QJsonValue::Null;

    QJsonDocument doc(loginData);
    QByteArray data = doc.toJson();

    QNetworkReply *reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        handleLoginResponse(reply);
        reply->deleteLater(); });
}

void DiscordClient::submitMFA(const QString &code, const QString &ticket)
{
    QNetworkRequest request = createRequest("/api/v9/auth/mfa/totp");

    QJsonObject mfaData;
    mfaData["code"] = code;
    mfaData["ticket"] = ticket;
    mfaData["login_source"] = QJsonValue::Null;
    mfaData["gift_code_sku_id"] = QJsonValue::Null;

    QJsonDocument doc(mfaData);
    QByteArray data = doc.toJson();

    QNetworkReply *reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        handleMFAResponse(reply);
        reply->deleteLater(); });
}

void DiscordClient::getCurrentUser()
{
    if (!isLoggedIn())
    {
        emit apiError("Not logged in");
        return;
    }

    QNetworkRequest request = createRequest("/api/v9/users/@me");
    request.setRawHeader("Authorization", m_token.toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        handleUserInfoResponse(reply);
        reply->deleteLater(); });
}

void DiscordClient::setToken(const QString &token)
{
    m_token = token;
    if (!m_token.isEmpty())
        m_gateway->connectToGateway(m_token);
}

QString DiscordClient::getToken() const
{
    return m_token;
}

bool DiscordClient::isLoggedIn() const
{
    return !m_token.isEmpty();
}

QNetworkRequest DiscordClient::createRequest(const QString &endpoint)
{
    QUrl url("https://discord.com" + endpoint);
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:147.0) Gecko/20100101 Firefox/147.0");
    request.setRawHeader("X-Fingerprint", m_fingerprint.toUtf8());
    request.setRawHeader("X-Super-Properties", generateSuperProperties().toUtf8());
    request.setRawHeader("X-Discord-Locale", "en-US");
    request.setRawHeader("X-Discord-Timezone", "Europe/Paris");
    request.setRawHeader("X-Debug-Options", "bugReporterEnabled");
    request.setRawHeader("Origin", "https://discord.com");
    request.setRawHeader("Referer", "https://discord.com/login");

    return request;
}

QString DiscordClient::generateFingerprint()
{
    quint64 random = QRandomGenerator::global()->generate64();
    return QString::number(random) + ".xYQaaQJGcUSinxb9Lbtw9PIOzeo";
}

QString DiscordClient::generateSuperProperties()
{
    QJsonObject props;
    props["os"] = "Windows";
    props["browser"] = "Firefox";
    props["device"] = "";
    props["system_locale"] = "en-US";
    props["browser_user_agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:147.0) Gecko/20100101 Firefox/147.0";
    props["browser_version"] = "147.0";
    props["os_version"] = "10";
    props["referrer"] = "";
    props["referring_domain"] = "";
    props["referrer_current"] = "";
    props["referring_domain_current"] = "";
    props["release_channel"] = "stable";
    props["client_build_number"] = 477811;
    props["client_event_source"] = QJsonValue::Null;

    QJsonDocument doc(props);
    return doc.toJson(QJsonDocument::Compact).toBase64();
}

void DiscordClient::handleLoginResponse(QNetworkReply *reply)
{
    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject obj = doc.object();

    if (reply->error() == QNetworkReply::ProtocolInvalidOperationError ||
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 400)
    {
        emit loginError("Wrong email or password");
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        emit loginError("Network error: " + reply->errorString());
        return;
    }

    qDebug() << "Login response:" << doc.toJson(QJsonDocument::Indented);

    if (obj.contains("token"))
    {
        m_token = obj["token"].toString();
        m_tokenStorage.saveToken(m_token);
        emit loginSuccess();
        m_gateway->connectToGateway(m_token);
    }
    else if (obj.contains("ticket") && obj["mfa"].toBool())
    {
        QString ticket = obj["ticket"].toString();
        emit mfaRequired(ticket);
    }
    else if (obj.contains("message"))
    {
        emit loginError(obj["message"].toString());
    }
    else
    {
        emit loginError("Unknown response from server");
    }
}

void DiscordClient::handleMFAResponse(QNetworkReply *reply)
{
    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject obj = doc.object();

    if (reply->error() == QNetworkReply::ProtocolInvalidOperationError ||
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 400)
    {
        emit loginError("Wrong code");
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        emit loginError("Network error: " + reply->errorString());
        return;
    }

    qDebug() << "MFA response:" << doc.toJson(QJsonDocument::Indented);

    if (obj.contains("token"))
    {
        m_token = obj["token"].toString();
        m_tokenStorage.saveToken(m_token);
        emit loginSuccess();
        m_gateway->connectToGateway(m_token);
    }
    else if (obj.contains("message"))
    {
        emit loginError(obj["message"].toString());
    }
    else
    {
        emit loginError("Unknown MFA response");
    }
}

void DiscordClient::handleUserInfoResponse(QNetworkReply *reply)
{
    // Check for 401 Unauthorized - token is invalid
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401)
    {
        qDebug() << "Token is invalid or expired";
        m_tokenStorage.clearToken();
        m_token.clear();
        emit tokenInvalidated();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        emit apiError("Failed to get user info: " + reply->errorString());
        return;
    }

    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    QJsonObject obj = doc.object();

    qDebug() << "User info:" << doc.toJson(QJsonDocument::Indented);

    User user;
    user.id = obj["id"].toString().toULongLong();
    user.username = obj["username"].toString();
    user.discriminator = obj["discriminator"].toString();
    user.avatar = obj["avatar"].toString();
    user.bot = obj["bot"].toBool(false);

    m_user = user; // Store current user
    emit userInfoReceived(user);
}

void DiscordClient::getChannelMessages(Snowflake channelId, int limit)
{
    if (!isLoggedIn())
        return;

    QNetworkRequest request = createRequest(QString("/api/v9/channels/%1/messages?limit=%2").arg(channelId).arg(limit));
    request.setRawHeader("Authorization", m_token.toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelId]()
            {
        if (reply->error() != QNetworkReply::NoError) {
            emit apiError("Failed to fetch messages: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray messagesArray = doc.array();
        QList<Message> messages;

        for (const QJsonValue &val : messagesArray) {
            QJsonObject obj = val.toObject();
            Message msg;
            msg.id = obj["id"].toString().toULongLong();
            msg.channelId = obj["channel_id"].toString().toULongLong();
            msg.content = obj["content"].toString();
            msg.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);

            QJsonObject authorObj = obj["author"].toObject();
            msg.author.id = authorObj["id"].toString().toULongLong();
            msg.author.username = authorObj["username"].toString();
            msg.author.discriminator = authorObj["discriminator"].toString();
            msg.author.avatar = authorObj["avatar"].toString();

            messages.append(msg);
        }

        // API returns newest first, so reverse to show oldest at top in chat log
        std::reverse(messages.begin(), messages.end());

        emit messagesLoaded(channelId, messages);
        reply->deleteLater(); });
}

void DiscordClient::getChannelMessagesBefore(Snowflake channelId, Snowflake beforeId, int limit)
{
    if (!isLoggedIn())
        return;

    QNetworkRequest request = createRequest(QString("/api/v9/channels/%1/messages?before=%2&limit=%3")
                                                .arg(channelId)
                                                .arg(beforeId)
                                                .arg(limit));
    request.setRawHeader("Authorization", m_token.toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channelId]()
            {
        if (reply->error() != QNetworkReply::NoError) {
            emit apiError("Failed to fetch older messages: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray messagesArray = doc.array();
        QList<Message> messages;

        for (const QJsonValue &val : messagesArray) {
            QJsonObject obj = val.toObject();
            Message msg;
            msg.id = obj["id"].toString().toULongLong();
            msg.channelId = obj["channel_id"].toString().toULongLong();
            msg.content = obj["content"].toString();
            msg.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);

            QJsonObject authorObj = obj["author"].toObject();
            msg.author.id = authorObj["id"].toString().toULongLong();
            msg.author.username = authorObj["username"].toString();
            msg.author.discriminator = authorObj["discriminator"].toString();
            msg.author.avatar = authorObj["avatar"].toString();

            messages.append(msg);
        }

        // API returns newest first, so reverse to show oldest at top in chat log
        std::reverse(messages.begin(), messages.end());

        emit messagesLoaded(channelId, messages);
        reply->deleteLater(); });
}

void DiscordClient::sendMessage(Snowflake channelId, const QString &content)
{
    if (!isLoggedIn() || content.isEmpty())
        return;

    QNetworkRequest request = createRequest(QString("/api/v9/channels/%1/messages").arg(channelId));
    request.setRawHeader("Authorization", m_token.toUtf8());

    QJsonObject messageData;
    messageData["content"] = content;
    // nonce helps identify the message echo
    messageData["nonce"] = QString::number(QRandomGenerator::global()->generate64());

    QJsonDocument doc(messageData);
    QByteArray data = doc.toJson();

    QNetworkReply *reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        if (reply->error() != QNetworkReply::NoError) {
            emit apiError("Failed to send message: " + reply->errorString());
        }
        // Success is handled via Gateway MESSAGE_CREATE usually, or we can handle it here
        reply->deleteLater(); });
}

void DiscordClient::handleGatewayEvent(const QString &eventName, const QJsonObject &data)
{
    qDebug() << "Gateway Event:" << eventName;

    if (eventName == "READY")
    {
        handleReady(data);
    }
    else if (eventName == "GUILD_CREATE")
    {
        handleGuildCreate(data);
    }
    else if (eventName == "MESSAGE_CREATE")
    {
        handleMessageCreate(data);
    }
}

void DiscordClient::handleReady(const QJsonObject &data)
{
    // Store current user info
    QJsonObject userObj = data["user"].toObject();
    m_user.id = userObj["id"].toString().toULongLong();
    m_user.username = userObj["username"].toString();
    m_user.discriminator = userObj["discriminator"].toString();
    m_user.avatar = userObj["avatar"].toString();
    m_user.bot = userObj["bot"].toBool(false);

    // Handle private channels (DMs)
    m_privateChannels.clear();
    QJsonArray privateChannels = data["private_channels"].toArray();

    for (const QJsonValue &val : privateChannels)
    {
        QJsonObject channelObj = val.toObject();
        Channel channel;
        channel.id = channelObj["id"].toString().toULongLong();
        channel.type = channelObj["type"].toInt();
        channel.lastMessageId = channelObj["last_message_id"].toString().toULongLong();
        channel.name = channelObj["name"].toString();

        // Parse recipients for DMs
        QJsonArray recipients = channelObj["recipients"].toArray();
        for (const QJsonValue &recipVal : recipients)
        {
            QJsonObject recipObj = recipVal.toObject();
            User user;
            user.id = recipObj["id"].toString().toULongLong();
            user.username = recipObj["username"].toString();
            user.discriminator = recipObj["discriminator"].toString();
            user.avatar = recipObj["avatar"].toString();
            user.bot = recipObj["bot"].toBool(false);
            channel.recipients.append(user);
        }

        m_privateChannels.append(channel);
        emit channelCreated(channel);
    }

    // For user accounts, guilds are included in READY
    // For bot accounts, guilds come later via GUILD_CREATE events
    m_guilds.clear();
    QJsonArray guilds = data["guilds"].toArray();

    for (const QJsonValue &val : guilds)
    {
        QJsonObject guildObj = val.toObject();

        // Check if guild is unavailable (outage scenario)
        if (guildObj["unavailable"].toBool(false))
        {
            qDebug() << "Guild unavailable:" << guildObj["id"].toString();
            continue;
        }

        // Parse guild data (user accounts get full guild objects in READY)
        handleGuildCreate(guildObj);
    }
}

void DiscordClient::handleGuildCreate(const QJsonObject &data)
{
    Guild guild;
    guild.id = data["id"].toString().toULongLong();
    guild.name = data["name"].toString();
    guild.icon = data["icon"].toString();
    guild.ownerId = data["owner_id"].toString().toULongLong();
    guild.joinedAt = data["joined_at"].toString();

    // Parse roles and their permissions
    QJsonArray rolesArray = data["roles"].toArray();
    for (const QJsonValue &roleVal : rolesArray)
    {
        QJsonObject roleObj = roleVal.toObject();
        Role role;
        role.id = roleObj["id"].toString().toULongLong();
        role.name = roleObj["name"].toString();
        role.permissions = roleObj["permissions"].toString().toULongLong();
        role.position = roleObj["position"].toInt();
        guild.roles[role.id] = role;
    }

    // Parse current user's roles in this guild
    QJsonArray members = data["members"].toArray();
    for (const QJsonValue &memberVal : members)
    {
        QJsonObject memberObj = memberVal.toObject();
        QJsonObject userObj = memberObj["user"].toObject();

        // Check if this is the current user
        if (userObj["id"].toString().toULongLong() == m_user.id)
        {
            QJsonArray roles = memberObj["roles"].toArray();
            for (const QJsonValue &roleVal : roles)
            {
                guild.memberRoles.append(roleVal.toString().toULongLong());
            }
            break;
        }
    }

    QJsonArray channels = data["channels"].toArray();
    for (const QJsonValue &val : channels)
    {
        QJsonObject channelObj = val.toObject();
        Channel channel;
        channel.id = channelObj["id"].toString().toULongLong();
        channel.type = channelObj["type"].toInt();
        channel.guildId = guild.id;
        channel.name = channelObj["name"].toString();
        channel.topic = channelObj["topic"].toString();
        channel.position = channelObj["position"].toInt();
        channel.parentId = channelObj["parent_id"].toString().toULongLong();
        channel.lastMessageId = channelObj["last_message_id"].toString().toULongLong();

        // Parse permission overwrites
        QJsonArray overwrites = channelObj["permission_overwrites"].toArray();
        for (const QJsonValue &overwriteVal : overwrites)
        {
            QJsonObject overwriteObj = overwriteVal.toObject();
            PermissionOverwrite overwrite;
            overwrite.id = overwriteObj["id"].toString().toULongLong();
            overwrite.type = overwriteObj["type"].toInt();
            overwrite.allow = overwriteObj["allow"].toString().toULongLong();
            overwrite.deny = overwriteObj["deny"].toString().toULongLong();
            channel.permissionOverwrites.append(overwrite);
        }

        guild.channels.append(channel);
    }

    // Sort channels properly by category hierarchy
    // 1. Build a map of category positions
    QMap<Snowflake, int> categoryPositions;
    for (const Channel &ch : guild.channels)
    {
        if (ch.isCategory())
        {
            categoryPositions[ch.id] = ch.position;
        }
    }

    // 2. Sort: categories and their children grouped together, ordered by category position
    std::sort(guild.channels.begin(), guild.channels.end(), [&categoryPositions](const Channel &a, const Channel &b)
              {
        // Determine effective category position for each channel
        int aCategoryPos = a.isCategory() ? a.position : (a.parentId != 0 ? categoryPositions.value(a.parentId, 9999) : -1);
        int bCategoryPos = b.isCategory() ? b.position : (b.parentId != 0 ? categoryPositions.value(b.parentId, 9999) : -1);

        // Uncategorized channels (parentId == 0, not categories) come first
        bool aUncategorized = !a.isCategory() && a.parentId == 0;
        bool bUncategorized = !b.isCategory() && b.parentId == 0;

        if (aUncategorized && !bUncategorized)
            return true;
        if (!aUncategorized && bUncategorized)
            return false;

        // If in different categories, sort by category position
        if (aCategoryPos != bCategoryPos)
            return aCategoryPos < bCategoryPos;

        // Same category: category header comes first, then channels by position
        if (a.isCategory() && !b.isCategory())
            return true;
        if (!a.isCategory() && b.isCategory())
            return false;

        // Both channels in same category, sort by position
        return a.position < b.position; });

    m_guilds.append(guild);
    emit guildCreated(guild);
    qDebug() << "Guild created:" << guild.name << "with" << guild.channels.size() << "channels";

    // Download guild icon if available
    if (!guild.icon.isEmpty())
    {
        downloadGuildIcon(guild.id, guild.icon);
    }
}

QString DiscordClient::getGuildIconUrl(Snowflake guildId, const QString &iconHash) const
{
    if (iconHash.isEmpty())
        return QString();

    // Discord CDN URL format: https://cdn.discordapp.com/icons/{guild_id}/{icon_hash}.png
    QString extension = iconHash.startsWith("a_") ? ".gif" : ".png";
    return QString("https://cdn.discordapp.com/icons/%1/%2%3")
        .arg(guildId)
        .arg(iconHash)
        .arg(extension);
}

void DiscordClient::downloadGuildIcon(Snowflake guildId, const QString &iconHash)
{
    if (iconHash.isEmpty())
        return;

    QString iconUrl = getGuildIconUrl(guildId, iconHash);
    QNetworkRequest request(iconUrl);

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, guildId]()
            {
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray imageData = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(imageData))
            {
                m_guildIcons[guildId] = pixmap;
                emit guildIconLoaded(guildId, pixmap);
                qDebug() << "Guild icon loaded for guild" << guildId;
            }
        }
        else
        {
            qDebug() << "Failed to download guild icon:" << reply->errorString();
        }
        reply->deleteLater(); });
}

void DiscordClient::handleMessageCreate(const QJsonObject &data)
{
    Message message;
    message.id = data["id"].toString().toULongLong();
    message.channelId = data["channel_id"].toString().toULongLong();

    if (data.contains("guild_id"))
        message.guildId = data["guild_id"].toString().toULongLong();
    else
        message.guildId = 0;

    QJsonObject authorObj = data["author"].toObject();
    message.author.id = authorObj["id"].toString().toULongLong();
    message.author.username = authorObj["username"].toString();
    message.author.discriminator = authorObj["discriminator"].toString();
    message.author.avatar = authorObj["avatar"].toString();
    message.author.bot = authorObj["bot"].toBool(false);

    message.content = data["content"].toString();
    message.timestamp = QDateTime::fromString(data["timestamp"].toString(), Qt::ISODate);

    emit newMessage(message);
}

bool DiscordClient::canViewChannel(const Guild &guild, const Channel &channel) const
{
    // DM channels are always viewable
    if (channel.isDm())
        return true;

    // If user is guild owner, they can view all channels
    if (guild.ownerId == m_user.id)
        return true;

    // Start with @everyone role base permissions
    quint64 basePermissions = 0;
    if (guild.roles.contains(guild.id))
    {
        basePermissions = guild.roles[guild.id].permissions;
    }

    // Apply user's role permissions (highest role position wins for base permissions)
    for (Snowflake roleId : guild.memberRoles)
    {
        if (guild.roles.contains(roleId))
        {
            const Role &role = guild.roles[roleId];

            // If any role has ADMINISTRATOR, user can see everything
            if (role.permissions & Permissions::ADMINISTRATOR)
                return true;

            basePermissions |= role.permissions;
        }
    }

    // Start with base permissions
    quint64 permissions = basePermissions;

    // Apply @everyone channel overwrites
    for (const PermissionOverwrite &overwrite : channel.permissionOverwrites)
    {
        if (overwrite.id == guild.id && overwrite.type == 0)
        {
            permissions &= ~overwrite.deny;
            permissions |= overwrite.allow;
        }
    }

    // Apply user's role channel overwrites
    for (Snowflake roleId : guild.memberRoles)
    {
        for (const PermissionOverwrite &overwrite : channel.permissionOverwrites)
        {
            if (overwrite.id == roleId && overwrite.type == 0)
            {
                permissions &= ~overwrite.deny;
                permissions |= overwrite.allow;
            }
        }
    }

    // Apply user-specific overwrites (highest priority)
    for (const PermissionOverwrite &overwrite : channel.permissionOverwrites)
    {
        if (overwrite.id == m_user.id && overwrite.type == 1) // Member overwrite
        {
            permissions &= ~overwrite.deny;
            permissions |= overwrite.allow;
        }
    }

    // Check if user has VIEW_CHANNEL permission
    return (permissions & Permissions::VIEW_CHANNEL) != 0;
}

bool DiscordClient::canSendMessages(const Guild &guild, const Channel &channel) const
{
    // DM channels - always can send
    if (channel.isDm())
        return true;

    // Voice channels - can't send text messages
    if (channel.isVoice())
        return false;

    // If user is guild owner, they can send in all channels
    if (guild.ownerId == m_user.id)
        return true;

    // Start with @everyone role base permissions
    quint64 basePermissions = 0;
    if (guild.roles.contains(guild.id))
    {
        basePermissions = guild.roles[guild.id].permissions;
    }

    // Apply user's role permissions
    for (Snowflake roleId : guild.memberRoles)
    {
        if (guild.roles.contains(roleId))
        {
            const Role &role = guild.roles[roleId];

            // If any role has ADMINISTRATOR, user can send everywhere
            if (role.permissions & Permissions::ADMINISTRATOR)
                return true;

            basePermissions |= role.permissions;
        }
    }

    // Start with base permissions
    quint64 permissions = basePermissions;

    // Apply @everyone channel overwrites
    for (const PermissionOverwrite &overwrite : channel.permissionOverwrites)
    {
        if (overwrite.id == guild.id && overwrite.type == 0)
        {
            permissions &= ~overwrite.deny;
            permissions |= overwrite.allow;
        }
    }

    // Apply user's role channel overwrites
    for (Snowflake roleId : guild.memberRoles)
    {
        for (const PermissionOverwrite &overwrite : channel.permissionOverwrites)
        {
            if (overwrite.id == roleId && overwrite.type == 0)
            {
                permissions &= ~overwrite.deny;
                permissions |= overwrite.allow;
            }
        }
    }

    // Apply user-specific overwrites (highest priority)
    for (const PermissionOverwrite &overwrite : channel.permissionOverwrites)
    {
        if (overwrite.id == m_user.id && overwrite.type == 1) // Member overwrite
        {
            permissions &= ~overwrite.deny;
            permissions |= overwrite.allow;
        }
    }

    // Check if user has SEND_MESSAGES permission
    return (permissions & Permissions::SEND_MESSAGES) != 0;
}
