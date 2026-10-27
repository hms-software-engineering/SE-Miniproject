#pragma once

#include "workflowwindow.h"

#include <QMainWindow>

class ApiClient;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;
class QWidget;

class LoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void handleLogin();
    void showRoleMenu(const QString &role);
    void showLoginError(const QString &message);
    void handleMenuSelection(int row);
    void logout();
    void showHelp();

private:
    void showLoginPage(const QString &message = {});
    void showPatientWindow(bool canManagePatients);
    void showWorkflow(WorkflowWindow::Workflow workflow);
    void expireSession();

    ApiClient *apiClient;
    QStackedWidget *pages;
    QWidget *loginPage;
    QWidget *dashboardPage;
    QLineEdit *usernameInput;
    QLineEdit *passwordInput;
    QLabel *messageLabel;
    QPushButton *loginButton;
    QLabel *workAreaLabel;
    QLabel *sessionStatusLabel;
    QListWidget *navigation;
    QTimer *sessionTimer;
    QString activeUsername;
    QString activeRole;
};
