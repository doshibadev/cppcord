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
    void getChannelMessages(Snowflake channelId, int limit = 50);
    void getChannelMessagesBefore(Snowflake channelId, Snowflake beforeId, int limit = 50);
    void sendMessage(Snowflake channelId, const QString &content);

    // Token management
    void setToken(const QString &token);
    QString getToken() const;
    bool isLoggedIn() const;

    // Data Access
    const QList<Guild> &getGuilds() const { return m_guilds; }
    const QList<Channel> &getPrivateChannels() const { return m_privateChannels; }
    const QList<Channel> &getChannels(Snowflake guildId) const;
    const User *currentUser() const { return m_user.id != 0 ? &m_user : nullptr; }
    Snowflake getUserId() const { return m_user.id; }

    // Voice
    class VoiceClient *getVoiceClient() const { return m_gateway->getVoiceClient(); }
    void joinVoiceChannel(Snowflake guildId, Snowflake channelId, bool mute = false, bool deaf = false);
    void leaveVoiceChannel(Snowflake guildId);

    // DM Calls
    void startCall(Snowflake channelId);
    void ringCall(Snowflake channelId, const QList<Snowflake> &recipients);
    void stopRinging(Snowflake channelId);

    // Icon management
    void downloadGuildIcon(Snowflake guildId, const QString &iconHash);
    QString getGuildIconUrl(Snowflake guildId, const QString &iconHash) const;

    // Permission checking
    bool canViewChannel(const Guild &guild, const Channel &channel) const;
    bool canSendMessages(const Guild &guild, const Channel &channel) const;

signals:
    void loginSuccess();
    void mfaRequired(const QString &ticket);
    void loginError(const QString &error);
    void tokenInvalidated();
    void userInfoReceived(const User &user);
    void apiError(const QString &error);
    void messagesLoaded(Snowflake channelId, const QList<Message> &messages);
    void newMessage(const Message &message); // Live message from gateway

    // Data signals
    void guildCreated(const Guild &guild);
    void channelCreated(const Channel &channel);
    void guildIconLoaded(Snowflake guildId, const QPixmap &icon);

    // Call signals
    void callCreated(Snowflake channelId, const QList<Snowflake> &ringing);
    void callUpdated(Snowflake channelId, const QList<Snowflake> &ringing);
    void callDeleted(Snowflake channelId);

private:
    QNetworkAccessManager *m_networkManager;
    GatewayClient *m_gateway;
    QString m_token;
    QString m_fingerprint;
    TokenStorage m_tokenStorage;

    // State
    QList<Guild> m_guilds;
    QList<Channel> m_privateChannels;
    User m_user;
    QMap<Snowflake, QPixmap> m_guildIcons; // Cache for guild icons // Current user info

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
