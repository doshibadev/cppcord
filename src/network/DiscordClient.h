#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include "User.h"
#include "Guild.h"
#include "Channel.h"
#include "Message.h"
#include "GatewayClient.h"
#include "utils/TokenStorage.h"

class DiscordClient : public QObject
{
    Q_OBJECT

public:
    explicit DiscordClient(QObject *parent = nullptr);

    // Auth methods
    void login(const QString &email, const QString &password);
    void submitMFA(const QString &code, const QString &ticket);
    void loginWithToken(const QString &token);
    void logout();

    // API methods
    void getCurrentUser();
    void getChannelMessages(Snowflake channelId);

    // Token management
    void setToken(const QString &token);
    QString getToken() const;
    bool isLoggedIn() const;

    // Data Access
    const QList<Guild> &getGuilds() const { return m_guilds; }
    const QList<Channel> &getPrivateChannels() const { return m_privateChannels; }

    // Permission checking
    bool canViewChannel(const Guild &guild, const Channel &channel) const;

signals:
    void loginSuccess();
    void mfaRequired(const QString &ticket);
    void loginError(const QString &error);
    void tokenInvalidated();
    void userInfoReceived(const User &user);
    void apiError(const QString &error);
    void messageReceived(const QString &message);
    void messagesLoaded(Snowflake channelId, const QList<Message> &messages);

    // Data signals
    void guildCreated(const Guild &guild);
    void channelCreated(const Channel &channel);

private:
    QNetworkAccessManager *m_networkManager;
    GatewayClient *m_gateway;
    QString m_token;
    QString m_fingerprint;
    TokenStorage m_tokenStorage;

    // State
    QList<Guild> m_guilds;
    QList<Channel> m_privateChannels;
    User m_user; // Current user info

    // Event handlers
    void handleGatewayEvent(const QString &eventName, const QJsonObject &data);
    void handleReady(const QJsonObject &data);
    void handleGuildCreate(const QJsonObject &data);
    void handleMessageCreate(const QJsonObject &data);

    // Helper methods
    QNetworkRequest createRequest(const QString &endpoint);
    QString generateFingerprint();
    QString generateSuperProperties();
    void handleLoginResponse(QNetworkReply *reply);
    void handleMFAResponse(QNetworkReply *reply);
    void handleUserInfoResponse(QNetworkReply *reply);
};
