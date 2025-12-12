#pragma once
#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include "Types.h"
class VoiceClient;
class DiscordClient;
class GatewayClient : public QObject
{
    Q_OBJECT
public:
    explicit GatewayClient(QObject *parent = nullptr);

    void connectToGateway(const QString &token);
    void disconnectFromGateway();

    // Voice operations
    void joinVoiceChannel(Snowflake guildId, Snowflake channelId, bool mute = false, bool deaf = false);
    void leaveVoiceChannel(Snowflake guildId);
    class VoiceClient *getVoiceClient() const { return m_voiceClient; }
    void setDiscordClient(DiscordClient *client) { m_discordClient = client; }

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &message); // Kept for debug log
    void logMessage(const QString &msg);
    void eventReceived(const QString &eventName, const QJsonObject &data);

    // Voice signals
    void voiceStateUpdate(const QJsonObject &data);
    void voiceServerUpdate(const QString &token, Snowflake guildId, const QString &endpoint, const QString &sessionId);

    // Call signals (for DM voice)
    void callCreated(const QJsonObject &data);
    void callUpdated(const QJsonObject &data);
    void callDeleted(const QJsonObject &data);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void sendHeartbeat();

private:
    QWebSocket *m_socket;
    QTimer *m_heartbeatTimer;
    QString m_token;
    int m_sequenceNumber;
    int m_heartbeatInterval;
    QString m_sessionId;
    QString m_voiceSessionId; // Session ID from VOICE_STATE_UPDATE
    VoiceClient *m_voiceClient;
    DiscordClient *m_discordClient;

    // Track current voice connection for DM support
    Snowflake m_pendingVoiceGuildId;
    Snowflake m_pendingVoiceChannelId;

    void handlePayload(const QJsonObject &payload);
    void handleHello(const QJsonObject &data);
    void handleReady(const QJsonObject &data);
    void handleDispatch(const QJsonObject &payload);

    // Voice state helpers
    void sendVoiceStateUpdate(Snowflake guildId, Snowflake channelId, bool mute, bool deaf);

    void sendIdentify();
};
