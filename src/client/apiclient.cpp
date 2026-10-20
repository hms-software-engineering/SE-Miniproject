
#include "apiclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariant>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
{
}

QString ApiClient::token() const
{
    return authToken;
}

QString ApiClient::role() const
{
    return userRole;
}

void ApiClient::login(const QString &username,
                      const QString &password)
{
    QUrl url("http://127.0.0.1:8080/api/v1/auth/login");

    QNetworkRequest request(url);
    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/json"
    );

    QJsonObject body;
    body["username"] = username;
    body["password"] = password;

    QNetworkReply *reply = networkManager.post(
        request,
        QJsonDocument(body).toJson(QJsonDocument::Compact)
    );

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
        ).toInt();

        const QByteArray response = reply->readAll();
        const QJsonObject json =
            QJsonDocument::fromJson(response).object();

        if (status == 200 && reply->error() == QNetworkReply::NoError) {
            authToken = json.value("token").toString();
            userRole = json.value("role").toString();

            if (authToken.isEmpty() || userRole.isEmpty()) {
                emit loginFailed(
                    "The server response is missing the token or role."
                );
            } else {
                emit loginSucceeded(userRole);
            }
        } else if (status == 401) {
            emit loginFailed("Invalid username or password.");
        } else if (status == 423) {
            emit loginFailed("Your account is locked. Contact the administrator.");
        } else if (status == 0) {
            emit loginFailed(
                "Cannot connect to the HMS server. Check that it is running."
            );
        } else {
            QString message = json.value("error").toObject()
                                  .value("message").toString();

            if (message.isEmpty()) {
                message = QString("Login failed (HTTP %1).").arg(status);
            }

            emit loginFailed(message);
        }

        reply->deleteLater();
    });
}
