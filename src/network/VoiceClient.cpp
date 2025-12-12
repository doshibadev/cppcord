#include "VoiceClient.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkRequest>
#include <QDebug>
#include <sodium.h>
#include <cstring>
#include "../audio/OpusCodec.h"

// Helper function to get RTP header size for AAD (Additional Authenticated Data)
// For rtpsize modes: includes base header + CSRCs + extension header (NOT extension data)
static int getRtpHeaderSizeForAAD(const QByteArray &packet)
{
    if (packet.size() < 12)
    {
        return 12; // Minimum RTP header size
    }

    // Parse RTP header
    quint8 byte0 = static_cast<quint8>(packet[0]);
    quint8 cc = byte0 & 0x0F;                // CSRC count (bits 0-3)
    bool hasExtension = (byte0 & 0x10) != 0; // Extension bit (bit 4)

    int headerSize = 12;  // Basic RTP header
    headerSize += cc * 4; // CSRC identifiers (4 bytes each)

    // For rtpsize modes: extension HEADER (4 bytes) is part of AAD, but extension DATA is encrypted
    if (hasExtension && packet.size() >= headerSize + 4)
    {
        headerSize += 4; // Extension header only (profile + length)
    }

    return headerSize;
}

VoiceClient::VoiceClient(QObject *parent)
    : QObject(parent)
{
    // Initialize libsodium
    if (sodium_init() < 0)
    {
        qCritical() << "Failed to initialize libsodium!";
    }

    m_webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_udpSocket = new QUdpSocket(this);
    m_heartbeatTimer = new QTimer(this);

    connect(m_webSocket, &QWebSocket::connected, this, &VoiceClient::onWebSocketConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &VoiceClient::onWebSocketDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &VoiceClient::onWebSocketTextMessageReceived);
    connect(m_webSocket, &QWebSocket::binaryMessageReceived, this, &VoiceClient::onWebSocketBinaryMessageReceived);
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &VoiceClient::onWebSocketError);

    connect(m_heartbeatTimer, &QTimer::timeout, this, &VoiceClient::sendHeartbeat);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &VoiceClient::onUdpReadyRead);
}

VoiceClient::~VoiceClient()
{
    disconnectFromVoice();
}

void VoiceClient::connectToVoice(const QString &endpoint, const QString &token,
                                 const QString &sessionId, Snowflake guildId, Snowflake userId)
{
    m_endpoint = endpoint;
    m_token = token;
    m_sessionId = sessionId;
    m_guildId = guildId;
    m_userId = userId;

    // Voice endpoint doesn't include protocol, prepend wss://
    QString url = QString("wss://%1?v=%2").arg(m_endpoint).arg(VOICE_GATEWAY_VERSION);

    qDebug() << "Connecting to voice gateway:" << url;
    m_webSocket->open(QUrl(url));
}

void VoiceClient::disconnectFromVoice()
{
    if (m_heartbeatTimer->isActive())
    {
        m_heartbeatTimer->stop();
    }

    if (m_webSocket->state() == QAbstractSocket::ConnectedState)
    {
        m_webSocket->close();
    }

    if (m_udpSocket->state() == QAbstractSocket::BoundState)
    {
        m_udpSocket->close();
    }

    m_ssrc = 0;
    m_secretKey.clear();
    m_lastSequence = -1;
}

void VoiceClient::sendAudio(const QByteArray &opusData)
{
    qDebug() << "sendAudio called with" << opusData.size() << "bytes";

    if (!m_udpSocket || m_udpSocket->state() != QAbstractSocket::BoundState)
    {
        qWarning() << "Cannot send audio: UDP socket not ready";
        return;
    }

    if (m_ssrc == 0 || m_secretKey.isEmpty())
    {
        qWarning() << "Cannot send audio: not ready (no SSRC or secret key)";
        return;
    }

    // Increment sequence and timestamp
    m_audioSequence++;
    m_audioTimestamp += OPUS_FRAME_SIZE; // 960 samples per 20ms frame

    // Encrypt audio
    QByteArray encrypted = encryptAudio(opusData, m_audioSequence, m_audioTimestamp);
    if (encrypted.isEmpty())
    {
        qWarning() << "Failed to encrypt audio";
        return;
    }

    qDebug() << "Sending encrypted audio:" << encrypted.size() << "bytes, seq:" << m_audioSequence;

    // Send over UDP
    qint64 sent = m_udpSocket->writeDatagram(encrypted, QHostAddress(m_ip), m_port);
    if (sent < 0)
    {
        qWarning() << "Failed to send audio datagram:" << m_udpSocket->errorString();
    }
}

void VoiceClient::setSelfMute(bool mute)
{
    m_selfMute = mute;
    // Update speaking state
    if (m_ssrc != 0)
    {
        sendSpeaking(m_selfMute ? 0 : 1);
    }
}

void VoiceClient::setSelfDeaf(bool deaf)
{
    m_selfDeaf = deaf;
    // Deafening also mutes
    if (deaf)
    {
        setSelfMute(true);
    }
}

void VoiceClient::onWebSocketConnected()
{
    qDebug() << "Voice WebSocket connected";
    // Wait for Hello (Opcode 8) before sending Identify
}

void VoiceClient::onWebSocketDisconnected()
{
    QAbstractSocket::SocketState state = m_webSocket->state();
    QAbstractSocket::SocketError error = m_webSocket->error();
    QString errorString = m_webSocket->errorString();
    QWebSocketProtocol::CloseCode closeCode = m_webSocket->closeCode();
    QString closeReason = m_webSocket->closeReason();

    qWarning() << "Voice WebSocket disconnected";
    qWarning() << "State:" << state << "Error:" << error;
    qWarning() << "Close Code:" << closeCode << "Close Reason:" << closeReason;
    if (!errorString.isEmpty())
    {
        qWarning() << "Error string:" << errorString;
    }

    m_heartbeatTimer->stop();
    emit disconnected();
}

void VoiceClient::onWebSocketTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject json = doc.object();

    int opcode = json["op"].toInt();
    QJsonObject data = json["d"].toObject();

    // Handle sequence numbers for voice gateway v8
    if (json.contains("seq"))
    {
        m_lastSequence = json["seq"].toInt();
    }

    handleOpcode(opcode, data);
}

void VoiceClient::onWebSocketBinaryMessageReceived(const QByteArray &message)
{
    // Binary messages are used for DAVE protocol (E2EE)
    // For now, we'll skip E2EE implementation and focus on basic voice
    if (message.size() < 3)
    {
        qWarning() << "Received invalid binary voice message";
        return;
    }

    // Check if sequence number is present (first 2 bytes for server->client)
    quint16 sequence = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(message.constData()));
    m_lastSequence = sequence;

    quint8 opcode = static_cast<quint8>(message.at(2));
    QByteArray payload = message.mid(3);

    qDebug() << "Received binary voice opcode:" << opcode << "payload size:" << payload.size();
}

void VoiceClient::onWebSocketError(QAbstractSocket::SocketError error)
{
    qWarning() << "Voice WebSocket error:" << error << m_webSocket->errorString();
    emit this->error(m_webSocket->errorString());
}

void VoiceClient::sendHeartbeat()
{
    qint64 nonce = QDateTime::currentMSecsSinceEpoch();
    m_lastHeartbeatNonce = nonce;

    QJsonObject payload;
    payload["op"] = 3; // Heartbeat

    // Voice gateway v8 requires seq_ack
    QJsonObject data;
    data["t"] = nonce;
    data["seq_ack"] = m_lastSequence;
    payload["d"] = data;

    m_webSocket->sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void VoiceClient::sendIdentify()
{
    QJsonObject payload;
    payload["op"] = 0; // Identify

    QJsonObject data;
    data["server_id"] = QString::number(m_guildId);
    data["user_id"] = QString::number(m_userId);
    data["session_id"] = m_sessionId;
    data["token"] = m_token;
    data["max_dave_protocol_version"] = 0; // Required field - 0 indicates no E2EE support
    payload["d"] = data;

    qDebug() << "Sending voice Identify - Guild:" << m_guildId << "User:" << m_userId << "Session:" << m_sessionId;
    qDebug() << "Identify payload:" << QJsonDocument(payload).toJson(QJsonDocument::Compact);
    m_webSocket->sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void VoiceClient::sendSelectProtocol(const QString &ip, quint16 port, const QString &mode)
{
    QJsonObject payload;
    payload["op"] = 1; // Select Protocol

    QJsonObject protocolData;
    protocolData["address"] = ip;
    protocolData["port"] = port;
    protocolData["mode"] = mode;

    QJsonObject data;
    data["protocol"] = "udp";
    data["data"] = protocolData;
    payload["d"] = data;

    qDebug() << "Selecting protocol:" << mode << "IP:" << ip << "Port:" << port;
    m_webSocket->sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void VoiceClient::sendSpeaking(int flags)
{
    QJsonObject payload;
    payload["op"] = 5; // Speaking

    QJsonObject data;
    data["speaking"] = flags; // 0 = not speaking, 1 = microphone, 2 = soundshare, 4 = priority
    data["delay"] = 0;
    data["ssrc"] = static_cast<qint64>(m_ssrc);
    payload["d"] = data;

    m_webSocket->sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void VoiceClient::sendResume()
{
    QJsonObject payload;
    payload["op"] = 7; // Resume

    QJsonObject data;
    data["server_id"] = QString::number(m_guildId);
    data["session_id"] = m_sessionId;
    data["token"] = m_token;
    data["seq_ack"] = m_lastSequence;
    payload["d"] = data;

    qDebug() << "Sending voice Resume";
    m_webSocket->sendTextMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void VoiceClient::handleOpcode(int opcode, const QJsonObject &data)
{
    switch (opcode)
    {
    case 2: // Ready
        handleReady(data);
        break;
    case 4: // Session Description
        handleSessionDescription(data);
        break;
    case 6: // Heartbeat ACK
        handleHeartbeatAck(data);
        break;
    case 8: // Hello
        handleHello(data);
        break;
    case 9: // Resumed
        handleResumed();
        break;
    default:
        qDebug() << "Unhandled voice opcode:" << opcode;
        break;
    }
}

void VoiceClient::handleReady(const QJsonObject &data)
{
    m_ssrc = data["ssrc"].toInt();
    m_ip = data["ip"].toString();
    m_port = data["port"].toInt();

    // Get supported encryption modes
    QJsonArray modesArray = data["modes"].toArray();
    m_encryptionModes.clear();
    for (const QJsonValue &mode : modesArray)
    {
        m_encryptionModes.append(mode.toString());
    }

    qDebug() << "Voice Ready - SSRC:" << m_ssrc << "IP:" << m_ip << "Port:" << m_port;
    qDebug() << "Supported encryption modes:" << m_encryptionModes;

    // Bind UDP socket to any available port
    if (!m_udpSocket->bind(QHostAddress::Any, 0))
    {
        qWarning() << "Failed to bind UDP socket:" << m_udpSocket->errorString();
        emit error("Failed to bind UDP socket");
        return;
    }

    // Perform IP discovery
    performIpDiscovery();
}

void VoiceClient::handleSessionDescription(const QJsonObject &data)
{
    m_selectedMode = data["mode"].toString();

    QJsonArray keyArray = data["secret_key"].toArray();
    m_secretKey.clear();
    for (const QJsonValue &byte : keyArray)
    {
        m_secretKey.append(static_cast<char>(byte.toInt()));
    }

    m_daveProtocolVersion = data["dave_protocol_version"].toInt(0);

    qDebug() << "Session Description - Mode:" << m_selectedMode
             << "Key size:" << m_secretKey.size()
             << "DAVE version:" << m_daveProtocolVersion;

    // Initialize nonce counter
    m_audioNonce = 0;

    // Send initial speaking state
    sendSpeaking(m_selfMute ? 0 : 1);

    emit connected();
    emit ready(m_ip, m_port, m_ssrc);
}

void VoiceClient::handleHello(const QJsonObject &data)
{
    m_heartbeatInterval = data["heartbeat_interval"].toInt();

    qDebug() << "Voice Hello - Heartbeat interval:" << m_heartbeatInterval << "ms";

    // Send Identify FIRST before heartbeating
    sendIdentify();

    // Start heartbeat timer AFTER sending Identify
    m_heartbeatTimer->start(m_heartbeatInterval);
}

void VoiceClient::handleResumed()
{
    qDebug() << "Voice connection resumed";
    emit connected();
}

void VoiceClient::handleHeartbeatAck(const QJsonObject &data)
{
    qint64 nonce = data["t"].toVariant().toLongLong();
    if (nonce == m_lastHeartbeatNonce)
    {
        qDebug() << "Voice heartbeat acknowledged";
    }
}

void VoiceClient::performIpDiscovery()
{
    // IP Discovery packet structure:
    // Type (2 bytes) = 0x0001 for request
    // Length (2 bytes) = 70
    // SSRC (4 bytes)
    // Address (64 bytes) - null terminated
    // Port (2 bytes)

    QByteArray packet(74, 0);

    // Type = 0x0001 (request)
    packet[0] = 0x00;
    packet[1] = 0x01;

    // Length = 70
    packet[2] = 0x00;
    packet[3] = 0x46;

    // SSRC (big endian)
    qToBigEndian(m_ssrc, reinterpret_cast<uchar *>(packet.data() + 4));

    // Address and port are filled by the voice server

    qint64 sent = m_udpSocket->writeDatagram(packet, QHostAddress(m_ip), m_port);
    if (sent == -1)
    {
        qWarning() << "Failed to send IP discovery packet:" << m_udpSocket->errorString();
        emit error("Failed to send IP discovery packet");
    }
    else
    {
        qDebug() << "IP discovery packet sent to" << m_ip << ":" << m_port;
    }
}

void VoiceClient::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        // Check if this is an IP discovery response
        if (datagram.size() == 74 && datagram[0] == 0x00 && datagram[1] == 0x02)
        {
            // Extract our external IP
            QByteArray addressBytes = datagram.mid(8, 64);
            int nullIndex = addressBytes.indexOf('\0');
            QString externalIp = QString::fromUtf8(addressBytes.left(nullIndex));

            // Extract our external port (big endian)
            quint16 externalPort = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(datagram.constData() + 72));

            qDebug() << "IP Discovery complete - External IP:" << externalIp << "Port:" << externalPort;

            // Select encryption mode (prefer aead_aes256_gcm_rtpsize, fallback to aead_xchacha20_poly1305_rtpsize)
            QString selectedMode;
            if (m_encryptionModes.contains("aead_aes256_gcm_rtpsize"))
            {
                selectedMode = "aead_aes256_gcm_rtpsize";
            }
            else if (m_encryptionModes.contains("aead_xchacha20_poly1305_rtpsize"))
            {
                selectedMode = "aead_xchacha20_poly1305_rtpsize";
            }
            else
            {
                qWarning() << "No supported encryption mode available!";
                emit error("No supported encryption mode");
                return;
            }

            // Send Select Protocol
            sendSelectProtocol(externalIp, externalPort, selectedMode);
        }
        else
        {
            // This is encrypted voice data - process all packets
            qDebug() << "Received voice packet of size:" << datagram.size();

            // Decrypt the audio
            QByteArray decrypted = decryptAudio(datagram);
            if (!decrypted.isEmpty())
            {
                emit audioDataReceived(decrypted);
            }
            else
            {
                qDebug() << "Failed to decrypt audio packet";
            }
        }
    }
}

QByteArray VoiceClient::encryptAudio(const QByteArray &opus, quint16 sequence, quint32 timestamp)
{
    if (m_secretKey.isEmpty() || m_selectedMode.isEmpty())
    {
        qWarning() << "Cannot encrypt: secret key or mode not set";
        return QByteArray();
    }

    // Build RTP header (12 bytes)
    QByteArray rtpHeader(12, 0);
    rtpHeader[0] = 0x80; // Version 2, no padding, no extension, no CSRC
    rtpHeader[1] = 0x78; // Payload type 120 (Opus)

    // Sequence number (big endian)
    rtpHeader[2] = (sequence >> 8) & 0xFF;
    rtpHeader[3] = sequence & 0xFF;

    // Timestamp (big endian)
    rtpHeader[4] = (timestamp >> 24) & 0xFF;
    rtpHeader[5] = (timestamp >> 16) & 0xFF;
    rtpHeader[6] = (timestamp >> 8) & 0xFF;
    rtpHeader[7] = timestamp & 0xFF;

    // SSRC (big endian)
    rtpHeader[8] = (m_ssrc >> 24) & 0xFF;
    rtpHeader[9] = (m_ssrc >> 16) & 0xFF;
    rtpHeader[10] = (m_ssrc >> 8) & 0xFF;
    rtpHeader[11] = m_ssrc & 0xFF;

    QByteArray result;

    if (m_selectedMode == "aead_aes256_gcm_rtpsize")
    {
        // Increment nonce counter (must match discord.js implementation)
        m_audioNonce++;
        if (m_audioNonce > 0xFFFFFFFF) // MAX_NONCE_SIZE = 2^32 - 1
        {
            m_audioNonce = 0;
        }

        // Create 12-byte nonce buffer: [4-byte counter (big-endian)][8 zero bytes]
        unsigned char nonce[12];
        std::memset(nonce, 0, 12);
        qToBigEndian(m_audioNonce, nonce); // Write counter at beginning

        // Encrypt: output = ciphertext + 16 byte tag
        QByteArray ciphertext(opus.size() + crypto_aead_aes256gcm_ABYTES, 0);
        unsigned long long ciphertext_len;

        if (crypto_aead_aes256gcm_encrypt(
                reinterpret_cast<unsigned char *>(ciphertext.data()),
                &ciphertext_len,
                reinterpret_cast<const unsigned char *>(opus.constData()),
                opus.size(),
                reinterpret_cast<const unsigned char *>(rtpHeader.constData()),
                rtpHeader.size(),
                nullptr,
                nonce,
                reinterpret_cast<const unsigned char *>(m_secretKey.constData())) != 0)
        {
            qWarning() << "AES-GCM encryption failed";
            return QByteArray();
        }

        ciphertext.resize(static_cast<int>(ciphertext_len));

        // Append the 4-byte nonce padding (first 4 bytes of nonce buffer)
        QByteArray nonceBytes(4, 0);
        qToBigEndian(m_audioNonce, reinterpret_cast<uchar *>(nonceBytes.data()));

        result = rtpHeader + ciphertext + nonceBytes;
    }
    else if (m_selectedMode == "aead_xchacha20_poly1305_rtpsize")
    {
        // AEAD XChaCha20-Poly1305 with RTP size nonce
        // Nonce is 24 bytes: RTP header + 12 zero bytes
        unsigned char nonce[24];
        std::memcpy(nonce, rtpHeader.constData(), 12);
        std::memset(nonce + 12, 0, 12);

        // Encrypt: output = ciphertext + 16 byte tag
        QByteArray ciphertext(opus.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES, 0);
        unsigned long long ciphertext_len;

        if (crypto_aead_xchacha20poly1305_ietf_encrypt(
                reinterpret_cast<unsigned char *>(ciphertext.data()),
                &ciphertext_len,
                reinterpret_cast<const unsigned char *>(opus.constData()),
                opus.size(),
                reinterpret_cast<const unsigned char *>(rtpHeader.constData()),
                rtpHeader.size(),
                nullptr,
                nonce,
                reinterpret_cast<const unsigned char *>(m_secretKey.constData())) != 0)
        {
            qWarning() << "XChaCha20-Poly1305 encryption failed";
            return QByteArray();
        }

        ciphertext.resize(static_cast<int>(ciphertext_len));
        result = rtpHeader + ciphertext;
    }
    else
    {
        qWarning() << "Unsupported encryption mode:" << m_selectedMode;
        return QByteArray();
    }

    return result;
}

QByteArray VoiceClient::decryptAudio(const QByteArray &encrypted)
{
    if (m_secretKey.isEmpty() || m_selectedMode.isEmpty())
    {
        qWarning() << "Cannot decrypt: secret key or mode not set";
        return QByteArray();
    }

    if (encrypted.size() < 12)
    {
        qWarning() << "Encrypted data too small (missing RTP header)";
        return QByteArray();
    }

    // Calculate RTP header size for AAD (excludes extension data)
    int rtpHeaderSize = getRtpHeaderSizeForAAD(encrypted);

    // For rtpsize modes: the RTP header (base + CSRCs + extension header) is AAD
    // Extension DATA and Opus payload are encrypted together
    QByteArray rtpHeader = encrypted.left(rtpHeaderSize);

    // Packet structure: [RTP header with extensions][encrypted data + tag][4-byte nonce]
    if (encrypted.size() < rtpHeaderSize + 16 + 4)
    {
        qWarning() << "Packet too small for rtpsize mode";
        return QByteArray();
    }

    // Extract 4-byte nonce from end of packet
    QByteArray nonceBytes = encrypted.right(4);

    // Ciphertext + tag (everything between header and nonce suffix)
    QByteArray ciphertext = encrypted.mid(rtpHeaderSize, encrypted.size() - rtpHeaderSize - 4);

    qDebug() << "Decrypting - Header size:" << rtpHeaderSize
             << "Ciphertext+tag size:" << ciphertext.size()
             << "Total packet:" << encrypted.size();

    QByteArray decrypted;

    if (m_selectedMode == "aead_aes256_gcm_rtpsize")
    {
        // AES-GCM nonce: 12 bytes with nonce counter copied as-is
        unsigned char nonce[12];
        std::memset(nonce, 0, 12);
        std::memcpy(nonce, nonceBytes.constData(), 4);

        if (ciphertext.size() < crypto_aead_aes256gcm_ABYTES)
        {
            qWarning() << "Ciphertext too small for AES-GCM";
            return QByteArray();
        }

        QByteArray plaintext(ciphertext.size() - crypto_aead_aes256gcm_ABYTES, 0);
        unsigned long long plaintext_len;

        if (crypto_aead_aes256gcm_decrypt(
                reinterpret_cast<unsigned char *>(plaintext.data()),
                &plaintext_len,
                nullptr,
                reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                ciphertext.size(),
                reinterpret_cast<const unsigned char *>(rtpHeader.constData()),
                rtpHeader.size(),
                nonce,
                reinterpret_cast<const unsigned char *>(m_secretKey.constData())) != 0)
        {
            qWarning() << "AES-GCM decryption failed";
            return QByteArray();
        }

        plaintext.resize(static_cast<int>(plaintext_len));
        decrypted = plaintext;
    }
    else if (m_selectedMode == "aead_xchacha20_poly1305_rtpsize")
    {
        // XChaCha20 nonce: 24 bytes with nonce counter copied as-is
        unsigned char nonce[24];
        std::memset(nonce, 0, 24);
        std::memcpy(nonce, nonceBytes.constData(), 4);

        if (ciphertext.size() < crypto_aead_xchacha20poly1305_ietf_ABYTES)
        {
            qWarning() << "Ciphertext too small for XChaCha20-Poly1305";
            return QByteArray();
        }

        QByteArray plaintext(ciphertext.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES, 0);
        unsigned long long plaintext_len;

        if (crypto_aead_xchacha20poly1305_ietf_decrypt(
                reinterpret_cast<unsigned char *>(plaintext.data()),
                &plaintext_len,
                nullptr,
                reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                ciphertext.size(),
                reinterpret_cast<const unsigned char *>(rtpHeader.constData()),
                rtpHeader.size(),
                nonce,
                reinterpret_cast<const unsigned char *>(m_secretKey.constData())) != 0)
        {
            qWarning() << "XChaCha20-Poly1305 decryption failed";
            return QByteArray();
        }

        plaintext.resize(static_cast<int>(plaintext_len));
        decrypted = plaintext;
    }
    else
    {
        qWarning() << "Unsupported encryption mode:" << m_selectedMode;
        return QByteArray();
    }

    qDebug() << "Successfully decrypted" << decrypted.size() << "bytes";

    // Strip RTP extension DATA from the decrypted payload if present
    // The extension HEADER is in AAD, but extension DATA is encrypted
    if (encrypted.size() >= 12)
    {
        quint8 byte0 = static_cast<quint8>(encrypted[0]);
        quint8 cc = byte0 & 0x0F;
        bool hasExtension = (byte0 & 0x10) != 0;

        if (hasExtension)
        {
            int baseHeaderSize = 12 + (cc * 4);

            // Read extension length from the original RTP header (in AAD)
            if (encrypted.size() >= baseHeaderSize + 4)
            {
                quint16 extensionLength = qFromBigEndian<quint16>(
                    reinterpret_cast<const uchar *>(encrypted.constData() + baseHeaderSize + 2));

                int extensionDataSize = extensionLength * 4; // Length is in 32-bit words

                if (decrypted.size() >= extensionDataSize)
                {
                    // Skip the extension data, return only Opus payload
                    decrypted = decrypted.mid(extensionDataSize);
                    qDebug() << "Stripped" << extensionDataSize << "bytes of RTP extension data, Opus payload:" << decrypted.size() << "bytes";
                }
            }
        }
    }

    return decrypted;
}
