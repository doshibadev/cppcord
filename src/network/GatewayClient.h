#pragma once
#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

class GatewayClient : public QObject
{
    Q_OBJECT
public:
    explicit GatewayClient(QObject *parent = nullptr);

    void connectToGateway(const QString &token);
    void disconnectFromGateway();

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &message); // Kept for debug log
    void logMessage(const QString &msg);
    void eventReceived(const QString &eventName, const QJsonObject &data);

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

    void handlePayload(const QJsonObject &payload);
    void handleHello(const QJsonObject &data);
    void handleReady(const QJsonObject &data);
    void handleDispatch(const QJsonObject &payload);

    void sendIdentify();
};
