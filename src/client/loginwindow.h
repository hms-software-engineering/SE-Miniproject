
#pragma once

#include <QMainWindow>

class ApiClient;
class QLineEdit;
class QLabel;
class QPushButton;
class QVBoxLayout;

class LoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);

private slots:
    void handleLogin();
    void showRoleMenu(const QString &role);
    void showLoginError(const QString &message);

private:
    ApiClient *apiClient;
    QLineEdit *usernameInput;
    QLineEdit *passwordInput;
    QLabel *messageLabel;
    QPushButton *loginButton;
    QVBoxLayout *layout;
};
