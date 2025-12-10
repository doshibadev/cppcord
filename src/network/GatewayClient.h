#pragma once
#include <QObject>
#include <QWebSocket>

class GatewayClient : public QObject
{
    Q_OBJECT
public:
    explicit GatewayClient(QObject *parent = nullptr);

    void connectToGateway(const QString &token);
    void disconnect();

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &message);

private:
    QWebSocket *m_socket;
};
