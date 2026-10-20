
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    void login(const QString &username, const QString &password);

    QString token() const;
    QString role() const;

signals:
    void loginSucceeded(const QString &role);
    void loginFailed(const QString &message);

private:
    QNetworkAccessManager networkManager;
    QString authToken;
    QString userRole;
};
