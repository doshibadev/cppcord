#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include "User.h"

class DiscordClient : public QObject
{
    Q_OBJECT

public:
    explicit DiscordClient(QObject *parent = nullptr);

    // Auth methods
    void login(const QString &email, const QString &password);
    void submitMFA(const QString &code, const QString &ticket);

    // API methods
    void getCurrentUser();

    // Token management
    void setToken(const QString &token);
    QString getToken() const;
    bool isLoggedIn() const;

signals:
    void loginSuccess();
    void mfaRequired(const QString &ticket);
    void loginError(const QString &error);
    void userInfoReceived(const User &user);
    void apiError(const QString &error);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_token;
    QString m_fingerprint;

    // Helper methods
    QNetworkRequest createRequest(const QString &endpoint);
    QString generateFingerprint();
    QString generateSuperProperties();
    void handleLoginResponse(QNetworkReply *reply);
    void handleMFAResponse(QNetworkReply *reply);
    void handleUserInfoResponse(QNetworkReply *reply);
};
