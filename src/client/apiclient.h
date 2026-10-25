
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
    void logout();
    void registerPatient(const QJsonObject &patient);
    void updatePatient(const QString &patientId, const QJsonObject &patient);
    void searchPatients(const QString &query);
    void getFreeSlots(const QString &doctorId, const QString &date);
    void bookAppointment(const QString &patientId, const QString &doctorId,
                         const QString &date, const QString &time);
    void recordVisit(const QString &appointmentId, const QString &symptoms,
                     const QString &diagnosis, const QString &notes);
    void addPrescription(const QString &visitId, const QString &item,
                        const QString &dosage, const QString &frequency,
                        const QString &days);
    void createUser(const QString &username, const QString &password,
                    const QString &role);
    void createMyPatientProfile(const QJsonObject &profile);

    QString token() const;
    QString role() const;
    QString username() const;

signals:
    void loginSucceeded(const QString &role);
    void loginFailed(const QString &message);
    void logoutFinished(bool success, const QString &message);
    void sessionEnded();
    void patientOperationSucceeded(const QString &operation,
                                   const QJsonObject &response);
    void patientOperationFailed(const QString &operation,
                                const QString &message);
    void workflowOperationSucceeded(const QString &operation,
                                    const QJsonObject &response);
    void workflowOperationFailed(const QString &operation,
                                 const QString &message);

private:
    void sendPatientRequest(QNetworkAccessManager::Operation operation,
                            const QUrl &url,
                            const QJsonObject &body,
                            const QString &action);
    void sendWorkflowRequest(QNetworkAccessManager::Operation operation,
                             const QUrl &url,
                             const QJsonObject &body,
                             const QString &action);
    QUrl endpoint(const QString &path) const;

    QNetworkAccessManager networkManager;
    QUrl apiBaseUrl;
    QString authToken;
    QString userRole;
    QString authUsername;
};
