
#include "apiclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
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

void ApiClient::registerPatient(const QJsonObject &patient)
{
    sendPatientRequest(
        QNetworkAccessManager::PostOperation,
        QUrl("http://127.0.0.1:8080/api/v1/patients"),
        patient, "register");
}

void ApiClient::updatePatient(const QString &patientId,
                              const QJsonObject &patient)
{
    QUrl url("http://127.0.0.1:8080/api/v1/patients/" + patientId);
    sendPatientRequest(QNetworkAccessManager::PutOperation, url,
                       patient, "update");
}

void ApiClient::searchPatients(const QString &query)
{
    QUrl url("http://127.0.0.1:8080/api/v1/patients/search");
    QUrlQuery parameters;
    parameters.addQueryItem("query", query);
    url.setQuery(parameters);
    sendPatientRequest(QNetworkAccessManager::GetOperation, url, {},
                       "search");
}

void ApiClient::sendPatientRequest(
    QNetworkAccessManager::Operation operation,
    const QUrl &url,
    const QJsonObject &body,
    const QString &action)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         "Bearer " + authToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    const QByteArray payload =
        QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = nullptr;
    if (operation == QNetworkAccessManager::GetOperation) {
        reply = networkManager.get(request);
    } else if (operation == QNetworkAccessManager::PostOperation) {
        reply = networkManager.post(request, payload);
    } else {
        reply = networkManager.sendCustomRequest(
            request, "PUT", payload);
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, action]() {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject response =
            QJsonDocument::fromJson(reply->readAll()).object();
        if (status >= 200 && status < 300 &&
            reply->error() == QNetworkReply::NoError) {
            emit patientOperationSucceeded(action, response);
        } else {
            QString message = response.value("error").toObject()
                                  .value("message").toString();
            if (message.isEmpty()) {
                message = status == 0
                    ? "Cannot connect to the HMS server."
                    : QString("Patient request failed (HTTP %1).").arg(status);
            }
            emit patientOperationFailed(action, message);
        }
        reply->deleteLater();
    });
}
