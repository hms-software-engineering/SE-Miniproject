
#include "apiclient.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <QStringList>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
{
    const QString configuredUrl = qEnvironmentVariable(
        "HMS_API_BASE_URL", "https://127.0.0.1:8443");
    apiBaseUrl = QUrl(configuredUrl);
    if (!apiBaseUrl.isValid() ||
        apiBaseUrl.scheme().compare("https", Qt::CaseInsensitive) != 0 ||
        apiBaseUrl.host().isEmpty() ||
        apiBaseUrl.hasQuery() ||
        apiBaseUrl.hasFragment() ||
        !apiBaseUrl.userName().isEmpty() ||
        !apiBaseUrl.password().isEmpty()) {
        apiBaseUrl = QUrl();
    }
}

QUrl ApiClient::endpoint(const QString &path) const
{
    if (apiBaseUrl.isEmpty()) {
        return {};
    }
    QUrl url = apiBaseUrl;
    QString basePath = url.path();
    if (basePath.endsWith('/')) {
        basePath.chop(1);
    }
    url.setPath(basePath + path);
    return url;
}

QString ApiClient::token() const
{
    return authToken;
}

QString ApiClient::role() const
{
    return userRole;
}

QString ApiClient::username() const
{
    return authUsername;
}

void ApiClient::login(const QString &username,
                      const QString &password)
{
    if (apiBaseUrl.isEmpty()) {
        emit loginFailed(
            "HMS_API_BASE_URL must be a valid HTTPS URL. Plain HTTP is not allowed.");
        return;
    }
    const QUrl url = endpoint("/api/v1/auth/login");

    QNetworkRequest request(url);
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    request.setSslConfiguration(ssl);
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

    connect(reply, &QNetworkReply::finished, this, [this, reply, username]() {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
        ).toInt();

        const QByteArray response = reply->readAll();
        const QJsonObject json =
            QJsonDocument::fromJson(response).object();

        if (status == 200 && reply->error() == QNetworkReply::NoError) {
            authToken = json.value("token").toString();
            userRole = json.value("role").toString();
            authUsername = username;

            if (authToken.isEmpty() || userRole.isEmpty()) {
                authToken.clear();
                userRole.clear();
                authUsername.clear();
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
                "Cannot establish a secure connection to HMS. Check the server and its trusted TLS certificate."
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

void ApiClient::logout()
{
    if (authToken.isEmpty()) {
        emit logoutFinished(true, {});
        return;
    }
    const QString previousToken = authToken;
    authToken.clear();
    userRole.clear();
    authUsername.clear();
    emit sessionEnded();

    if (apiBaseUrl.isEmpty()) {
        emit logoutFinished(
            false, "The local session was cleared, but the server URL is invalid.");
        return;
    }

    QNetworkRequest request(endpoint("/api/v1/auth/logout"));
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    request.setSslConfiguration(ssl);
    request.setRawHeader("Authorization",
                         "Bearer " + previousToken.toUtf8());
    QNetworkReply *reply = networkManager.post(request, QByteArray{});
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = status >= 200 && status < 300 &&
                             reply->error() == QNetworkReply::NoError;
        const QString message = success
            ? QString()
            : "The local session was cleared, but the server could not confirm logout.";
        emit logoutFinished(success, message);
        reply->deleteLater();
    });
}

void ApiClient::registerPatient(const QJsonObject &patient,
                                bool confirmDuplicate)
{
    QUrl url = endpoint("/api/v1/patients");
    if (confirmDuplicate) {
        QUrlQuery parameters;
        parameters.addQueryItem("confirmDuplicate", "true");
        url.setQuery(parameters);
    }
    sendPatientRequest(
        QNetworkAccessManager::PostOperation,
        url, patient, "register");
}

void ApiClient::updatePatient(const QString &patientId,
                              const QJsonObject &patient)
{
    QUrl url = endpoint("/api/v1/patients/" + patientId);
    sendPatientRequest(QNetworkAccessManager::PutOperation, url,
                       patient, "update");
}

void ApiClient::searchPatients(const QString &query)
{
    QUrl url = endpoint("/api/v1/patients/search");
    QUrlQuery parameters;
    parameters.addQueryItem("query", query);
    url.setQuery(parameters);
    sendPatientRequest(QNetworkAccessManager::GetOperation, url, {},
                       "search");
}

void ApiClient::getFreeSlots(const QString &doctorId, const QString &date)
{
    QUrl url = endpoint("/api/v1/appointments/free-slots");
    QUrlQuery parameters;
    parameters.addQueryItem("doctorId", doctorId);
    parameters.addQueryItem("date", date);
    url.setQuery(parameters);
    sendWorkflowRequest(QNetworkAccessManager::GetOperation, url, {},
                        "freeSlots");
}

void ApiClient::bookAppointment(const QString &patientId,
                                const QString &doctorId,
                                const QString &date,
                                const QString &time)
{
    sendWorkflowRequest(
        QNetworkAccessManager::PostOperation,
        endpoint("/api/v1/appointments"),
        {{"patientId", patientId}, {"doctorId", doctorId},
         {"date", date}, {"time", time}},
        "bookAppointment");
}

void ApiClient::recordVisit(const QString &appointmentId,
                            const QString &symptoms,
                            const QString &diagnosis,
                            const QString &notes)
{
    sendWorkflowRequest(
        QNetworkAccessManager::PostOperation, endpoint("/api/v1/visits"),
        {{"appointmentId", appointmentId}, {"symptoms", symptoms},
         {"diagnosis", diagnosis}, {"notes", notes}},
        "recordVisit");
}

void ApiClient::addPrescription(const QString &visitId,
                                const QString &item,
                                const QString &dosage,
                                const QString &frequency,
                                const QString &days)
{
    sendWorkflowRequest(
        QNetworkAccessManager::PostOperation,
        endpoint("/api/v1/visits/" + visitId + "/prescriptions"),
        {{"item", item}, {"dosage", dosage}, {"frequency", frequency},
         {"days", days}},
        "addPrescription");
}

void ApiClient::createUser(const QString &username,
                           const QString &password,
                           const QString &role)
{
    sendWorkflowRequest(
        QNetworkAccessManager::PostOperation, endpoint("/api/v1/users"),
        {{"username", username}, {"password", password}, {"role", role}},
        "createUser");
}

void ApiClient::createMyPatientProfile(const QJsonObject &profile)
{
    sendWorkflowRequest(
        QNetworkAccessManager::PostOperation,
        endpoint("/api/v1/patients/me/profile"), profile,
        "createMyProfile");
}

void ApiClient::sendWorkflowRequest(
    QNetworkAccessManager::Operation operation,
    const QUrl &url,
    const QJsonObject &body,
    const QString &action)
{
    if (url.isEmpty()) {
        emit workflowOperationFailed(
            action, "HMS_API_BASE_URL must be a valid HTTPS URL.");
        return;
    }
    QNetworkRequest request(url);
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    request.setSslConfiguration(ssl);
    request.setRawHeader("Authorization",
                         "Bearer " + authToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");

    QNetworkReply *reply = nullptr;
    if (operation == QNetworkAccessManager::GetOperation) {
        reply = networkManager.get(request);
    } else {
        reply = networkManager.post(
            request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    }
    connect(reply, &QNetworkReply::finished, this, [this, reply, action]() {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject response =
            QJsonDocument::fromJson(reply->readAll()).object();
        if (status >= 200 && status < 300 &&
            reply->error() == QNetworkReply::NoError) {
            emit workflowOperationSucceeded(action, response);
        } else {
            QString message = response.value("error").toObject()
                                  .value("message").toString();
            const QJsonArray suggestions =
                response.value("suggestedSlots").toArray();
            if (!suggestions.isEmpty()) {
                QStringList values;
                for (const QJsonValue &value : suggestions) {
                    values.append(value.toString());
                }
                message += "\nSuggested alternatives: " +
                           values.join(", ");
            }
            if (message.isEmpty()) {
                message = status == 0
                    ? "Cannot connect securely to the HMS server."
                    : QString("Request failed (HTTP %1).").arg(status);
            }
            emit workflowOperationFailed(action, message);
        }
        reply->deleteLater();
    });
}

void ApiClient::sendPatientRequest(
    QNetworkAccessManager::Operation operation,
    const QUrl &url,
    const QJsonObject &body,
    const QString &action)
{
    if (url.isEmpty()) {
        emit patientOperationFailed(
            action, "HMS_API_BASE_URL must be a valid HTTPS URL.");
        return;
    }
    QNetworkRequest request(url);
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    request.setSslConfiguration(ssl);
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
            const QString errorCode = response.value("error").toObject()
                                         .value("code").toString();
            if (action == "register" &&
                errorCode == "DUPLICATE_PATIENT") {
                emit patientDuplicateDetected(message);
                reply->deleteLater();
                return;
            }
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
