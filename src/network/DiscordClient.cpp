#include "DiscordClient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QRandomGenerator>
#include <QSysInfo>

DiscordClient::DiscordClient(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_fingerprint(generateFingerprint())
{
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
        emit loginSuccess();
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
        emit loginSuccess();
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
    user.username = obj["username"].toString();
    user.discriminator = obj["discriminator"].toString();
    user.id = obj["id"].toString().toULongLong();
    user.avatar = obj["avatar"].toString();
    user.email = obj["email"].toString();
    user.mfaEnabled = obj["mfa_enabled"].toBool();
    user.verified = obj["verified"].toBool();

    emit userInfoReceived(user);
}
