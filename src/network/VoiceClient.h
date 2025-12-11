#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>
#include <QUdpSocket>
#include "Types.h"

class AudioManager;

class VoiceClient : public QObject
{
    Q_OBJECT

public:
    explicit VoiceClient(QObject *parent = nullptr);
    ~VoiceClient();

    // Connect to voice server
    void connectToVoice(const QString &endpoint, const QString &token,
                        const QString &sessionId, Snowflake guildId, Snowflake userId);

    void disconnectFromVoice();
    void setSelfMute(bool mute);
    void setSelfDeaf(bool deaf);

    // Send Opus-encoded audio data
    void sendAudio(const QByteArray &opusData);

signals:
    void connected();
    void disconnected();
    void error(const QString &error);
    void ready(const QString &ip, quint16 port, quint32 ssrc);
    void audioDataReceived(const QByteArray &opusData); // Decrypted Opus data from other users

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketTextMessageReceived(const QString &message);
    void onWebSocketBinaryMessageReceived(const QByteArray &message);
    void onWebSocketError(QAbstractSocket::SocketError error);
    void sendHeartbeat();
    void onUdpReadyRead();

private:
    // WebSocket operations
    void sendIdentify();
    void sendSelectProtocol(const QString &ip, quint16 port, const QString &mode);
    void sendSpeaking(int flags);
    void sendResume();

    // Voice opcodes
    void handleOpcode(int opcode, const QJsonObject &data);
    void handleReady(const QJsonObject &data);
    void handleSessionDescription(const QJsonObject &data);
    void handleHello(const QJsonObject &data);
    void handleResumed();
    void handleHeartbeatAck(const QJsonObject &data);

    // UDP operations
    void performIpDiscovery();
    QByteArray encryptAudio(const QByteArray &opus, quint16 sequence, quint32 timestamp);
    QByteArray decryptAudio(const QByteArray &encrypted);

    // Voice gateway version 8 (recommended)
    static constexpr int VOICE_GATEWAY_VERSION = 8;

    // Connection info
    QString m_endpoint;
    QString m_token;
    QString m_sessionId;
    Snowflake m_guildId;
    Snowflake m_userId;

    // Voice session
    quint32 m_ssrc = 0;
    QString m_ip;
    quint16 m_port = 0;
    QStringList m_encryptionModes;
    QString m_selectedMode;
    QByteArray m_secretKey; // 32 bytes for encryption
    int m_daveProtocolVersion = 0;

    // State
    bool m_selfMute = false;
    bool m_selfDeaf = false;
    bool m_speaking = false;

    // Heartbeat
    QTimer *m_heartbeatTimer = nullptr;
    int m_heartbeatInterval = 0;
    qint64 m_lastHeartbeatNonce = 0;
    int m_lastSequence = -1; // For voice gateway v8 buffered resume

    // Sockets
    QWebSocket *m_webSocket = nullptr;
    QUdpSocket *m_udpSocket = nullptr;

    // Audio sequence
    quint16 m_audioSequence = 0;
    quint32 m_audioTimestamp = 0;
    quint32 m_audioNonce = 0; // Incremental nonce for encryption
};
