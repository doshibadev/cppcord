#include "DiscordClient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRandomGenerator>
#include <QSysInfo>
#include <algorithm> // for std::sort

DiscordClient::DiscordClient(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_gateway(new GatewayClient(this)), m_fingerprint(generateFingerprint())
{
    connect(m_gateway, &GatewayClient::eventReceived, this, &DiscordClient::handleGatewayEvent);
    connect(m_gateway, &GatewayClient::messageReceived, this, &DiscordClient::messageReceived);
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

    emit userInfoReceived(user);
}

void DiscordClient::getChannelMessages(Snowflake channelId)
{
    if (!isLoggedIn())
        return;

    QNetworkRequest request = createRequest(QString("/api/v9/channels/%1/messages?limit=50").arg(channelId));
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

void DiscordClient::handleGatewayEvent(const QString &eventName, const QJsonObject &data)
{
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

    // Guilds in READY are unavailable initially, we wait for GUILD_CREATE
    m_guilds.clear();
}

void DiscordClient::handleGuildCreate(const QJsonObject &data)
{
    Guild guild;
    guild.id = data["id"].toString().toULongLong();
    guild.name = data["name"].toString();
    guild.icon = data["icon"].toString();
    guild.ownerId = data["owner_id"].toString().toULongLong();

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
        channel.lastMessageId = channelObj["last_message_id"].toString().toULongLong();

        guild.channels.append(channel);
    }

    // Sort channels by position
    std::sort(guild.channels.begin(), guild.channels.end(), [](const Channel &a, const Channel &b)
              { return a.position < b.position; });

    m_guilds.append(guild);
    emit guildCreated(guild);
    qDebug() << "Guild created:" << guild.name << "with" << guild.channels.size() << "channels";
}

void DiscordClient::handleMessageCreate(const QJsonObject &data)
{
    // For now we just use the existing messageReceived signal for the string log
    // In future we parse Message object
}
