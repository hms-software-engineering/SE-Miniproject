
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QString>
#include <QUrl>

class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    void login(const QString &username, const QString &password);
    void registerPatient(const QJsonObject &patient);
    void updatePatient(const QString &patientId, const QJsonObject &patient);
    void searchPatients(const QString &query);

    QString token() const;
    QString role() const;

signals:
    void loginSucceeded(const QString &role);
    void loginFailed(const QString &message);
    void patientOperationSucceeded(const QString &operation,
                                   const QJsonObject &response);
    void patientOperationFailed(const QString &operation,
                                const QString &message);

private:
    void sendPatientRequest(QNetworkAccessManager::Operation operation,
                            const QUrl &url,
                            const QJsonObject &body,
                            const QString &action);

    QNetworkAccessManager networkManager;
    QString authToken;
    QString userRole;
};
