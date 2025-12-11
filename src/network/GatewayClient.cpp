#include "GatewayClient.h"
#include "VoiceClient.h"
#include "DiscordClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>

GatewayClient::GatewayClient(QObject *parent)
    : QObject(parent),
      m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this)),
      m_heartbeatTimer(new QTimer(this)),
      m_sequenceNumber(0),
      m_heartbeatInterval(0),
      m_voiceClient(new VoiceClient(this)),
      m_discordClient(nullptr)
{
    connect(m_socket, &QWebSocket::connected, this, &GatewayClient::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &GatewayClient::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &GatewayClient::onTextMessageReceived);

    // SSL configuration if needed (usually QWebSocket handles this automatically for wss://)

    connect(m_heartbeatTimer, &QTimer::timeout, this, &GatewayClient::sendHeartbeat);

    // Connect voice server update to voice client
    connect(this, &GatewayClient::voiceServerUpdate, this, [this](const QString &token, Snowflake guildId, const QString &endpoint, const QString &sessionId)
            {
                qDebug() << "Voice server update received, connecting to voice gateway";
                qDebug() << "Endpoint:" << endpoint << "Guild:" << guildId << "Session:" << sessionId;

                Snowflake userId = m_discordClient ? m_discordClient->getUserId() : 0;
                qDebug() << "User ID:" << userId;

                m_voiceClient->connectToVoice(endpoint, token, sessionId, guildId, userId); });
}

void GatewayClient::connectToGateway(const QString &token)
{
    m_token = token;
    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        m_socket->close();
    }

    qDebug() << "Connecting to Discord Gateway...";
    // Using v9 gateway
    m_socket->open(QUrl("wss://gateway.discord.gg/?v=9&encoding=json"));
}

void GatewayClient::disconnectFromGateway()
{
    m_heartbeatTimer->stop();
    m_socket->close();
}

void GatewayClient::onConnected()
{
    qDebug() << "Gateway WebSocket connected!";
    emit connected();
}

void GatewayClient::onDisconnected()
{
    qDebug() << "Gateway WebSocket disconnected!";
    m_heartbeatTimer->stop();
    emit disconnected();
}

void GatewayClient::onTextMessageReceived(const QString &message)
{
    // emit messageReceived(message); // Optional: raw message logging

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject())
    {
        qDebug() << "Received invalid JSON from gateway";
        return;
    }

    handlePayload(doc.object());
}

void GatewayClient::handlePayload(const QJsonObject &payload)
{
    int op = payload["op"].toInt();

    // Update sequence number if present
    if (payload.contains("s") && !payload["s"].isNull())
    {
        m_sequenceNumber = payload["s"].toInt();
    }

    switch (op)
    {
    case 0: // Dispatch
        handleDispatch(payload);
        break;
    case 10: // Hello
        handleHello(payload["d"].toObject());
        break;
    case 11: // Heartbeat ACK
        // qDebug() << "Heartbeat acknowledged";
        break;
    case 1: // Heartbeat requested
        sendHeartbeat();
        break;
    case 7: // Reconnect
        qDebug() << "Received Reconnect request";
        m_socket->close(); // Simplistic reconnection strategy: close and let user/logic reconnect
        break;
    case 9: // Invalid Session
        qDebug() << "Invalid Session";
        // Should verify if resumable, but for now just close
        m_socket->close();
        break;
    default:
        // qDebug() << "Unhandled OpCode:" << op;
        break;
    }
}

void GatewayClient::handleHello(const QJsonObject &data)
{
    m_heartbeatInterval = data["heartbeat_interval"].toInt();
    qDebug() << "Received Hello. Heartbeat interval:" << m_heartbeatInterval << "ms";

    // Start heartbeat
    m_heartbeatTimer->start(m_heartbeatInterval);

    // Send initial heartbeat immediately? Usually we wait or send Identify immediately.
    // Spec says: "Upon receiving Opcode 10 Hello... you should send an Opcode 1 Heartbeat..."
    // Wait, usually libraries identify after hello.
    // Actually, you can identify immediately.
    // But you MUST heartbeat periodically.

    // Let's send Identify now.
    sendIdentify();
}

void GatewayClient::sendIdentify()
{
    qDebug() << "Sending Identify...";

    QJsonObject properties;
    properties["$os"] = "linux"; // Or detect OS
    properties["$browser"] = "cppcord";
    properties["$device"] = "cppcord";

    QJsonObject data;
    data["token"] = m_token;
    data["properties"] = properties;
    data["compress"] = false;

    // Gateway Intents:
    // GUILDS = 1 << 0 = 1
    // GUILD_MEMBERS = 1 << 1 = 2 (privileged)
    // GUILD_VOICE_STATES = 1 << 7 = 128
    // GUILD_MESSAGES = 1 << 9 = 512
    // GUILD_MESSAGE_REACTIONS = 1 << 10 = 1024
    // DIRECT_MESSAGES = 1 << 12 = 4096
    // MESSAGE_CONTENT = 1 << 15 = 32768 (privileged)
    // Non-privileged intents: 1 + 128 + 512 + 1024 + 4096 = 5761
    // We try to request 37505 (standard + content + voice states)
    data["intents"] = 37505;

    QJsonObject payload;
    payload["op"] = 2; // Identify
    payload["d"] = data;

    m_socket->sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void GatewayClient::sendHeartbeat()
{
    QJsonObject payload;
    payload["op"] = 1;
    if (m_sequenceNumber == 0)
        payload["d"] = QJsonValue::Null;
    else
        payload["d"] = m_sequenceNumber;

    m_socket->sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void GatewayClient::handleDispatch(const QJsonObject &payload)
{
    QString eventName = payload["t"].toString();
    QJsonObject data = payload["d"].toObject();

    // Always emit the raw event for DiscordClient to process
    emit eventReceived(eventName, data);

    if (eventName == "READY")
    {
        handleReady(data);
    }
    else if (eventName == "VOICE_STATE_UPDATE")
    {
        qDebug() << "VOICE_STATE_UPDATE raw data:" << QJsonDocument(data).toJson(QJsonDocument::Compact);

        // Check if this is our user's voice state
        Snowflake userId = data["user_id"].toString().toULongLong();
        Snowflake currentUserId = m_discordClient ? m_discordClient->getUserId() : 0;
        
        qDebug() << "Voice State Update for User:" << userId << "Current User:" << currentUserId;

        if (userId == currentUserId)
        {
            QString voiceSessionId = data["session_id"].toString();
            qDebug() << "Our VOICE_STATE_UPDATE - Voice Session ID:" << voiceSessionId;
            qDebug() << "Gateway Session ID (m_sessionId):" << m_sessionId;

            // Store the voice session ID for later use
            m_voiceSessionId = voiceSessionId;
        }

        emit voiceStateUpdate(data);
    }
    else if (eventName == "VOICE_SERVER_UPDATE")
    {
        qDebug() << "VOICE_SERVER_UPDATE raw data:" << QJsonDocument(data).toJson(QJsonDocument::Compact);
        QString token = data["token"].toString();
        Snowflake guildId = data["guild_id"].toString().toULongLong();
        QString endpoint = data["endpoint"].toString();
        qDebug() << "VOICE_SERVER_UPDATE - Token length:" << token.length() << "Token:" << token;
        qDebug() << "VOICE_SERVER_UPDATE - Token type:" << data["token"].type();

        // Use voice-specific session ID if available, otherwise fall back to gateway session
        QString sessionToUse = m_voiceSessionId.isEmpty() ? m_sessionId : m_voiceSessionId;
        qDebug() << "Using session ID for voice:" << sessionToUse;

        emit voiceServerUpdate(token, guildId, endpoint, sessionToUse);
    }
    // Note: MESSAGE_CREATE handling for debug log moved to DiscordClient or kept here?
    // Let's keep the legacy messageReceived for now to not break the UI until we refactor MainWindow
    else if (eventName == "MESSAGE_CREATE")
    {
        QString content = data["content"].toString();
        QString author = data["author"].toObject()["username"].toString();
        // emit messageReceived(author + ": " + content); // We can keep this if needed or rely on DiscordClient
        qDebug() << "Message:" << author << ":" << content;
    }
}

void GatewayClient::handleReady(const QJsonObject &data)
{
    m_sessionId = data["session_id"].toString();
    QString username = data["user"].toObject()["username"].toString();
    qDebug() << "Gateway READY! Session ID:" << m_sessionId << "Logged in as:" << username;
}

void GatewayClient::joinVoiceChannel(Snowflake guildId, Snowflake channelId, bool mute, bool deaf)
{
    qDebug() << "Joining voice channel:" << channelId << "in guild:" << guildId;
    sendVoiceStateUpdate(guildId, channelId, mute, deaf);
}

void GatewayClient::leaveVoiceChannel(Snowflake guildId)
{
    qDebug() << "Leaving voice channel in guild:" << guildId;
    // Sending null channel_id disconnects from voice
    sendVoiceStateUpdate(guildId, Snowflake(), false, false);
}

void GatewayClient::sendVoiceStateUpdate(Snowflake guildId, Snowflake channelId, bool mute, bool deaf)
{
    QJsonObject payload;
    payload["op"] = 4; // Voice State Update

    QJsonObject data;
    data["guild_id"] = QString::number(guildId);

    if (channelId == 0)
    {
        data["channel_id"] = QJsonValue::Null; // Leave voice
    }
    else
    {
        data["channel_id"] = QString::number(channelId);
    }

    data["self_mute"] = mute;
    data["self_deaf"] = deaf;
    payload["d"] = data;

    m_socket->sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}
