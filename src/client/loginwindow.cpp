
#include "loginwindow.h"
#include "apiclient.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QStringList>

LoginWindow::LoginWindow(QWidget *parent)
    : QMainWindow(parent),
      apiClient(new ApiClient(this)),
      usernameInput(new QLineEdit(this)),
      passwordInput(new QLineEdit(this)),
      messageLabel(new QLabel(this)),
      loginButton(new QPushButton("Login", this)),
      layout(new QVBoxLayout)
{
    setWindowTitle("Hospital Management System");
    resize(420, 320);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *heading = new QLabel("HMS Login", central);
    heading->setStyleSheet(
        "font-size: 24px; font-weight: bold;"
    );

    usernameInput->setPlaceholderText("Username");
    passwordInput->setPlaceholderText("Password");
    passwordInput->setEchoMode(QLineEdit::Password);

    messageLabel->setWordWrap(true);
    loginButton->setDefault(true);

    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(new QLabel("Username", central));
    layout->addWidget(usernameInput);
    layout->addWidget(new QLabel("Password", central));
    layout->addWidget(passwordInput);
    layout->addWidget(messageLabel);
    layout->addWidget(loginButton);
    layout->addStretch();

    central->setLayout(layout);

    // Connect signals once in the constructor.
    connect(loginButton, &QPushButton::clicked,
            this, &LoginWindow::handleLogin);

    connect(passwordInput, &QLineEdit::returnPressed,
            this, &LoginWindow::handleLogin);

    connect(apiClient, &ApiClient::loginSucceeded,
            this, &LoginWindow::showRoleMenu);

    connect(apiClient, &ApiClient::loginFailed,
            this, &LoginWindow::showLoginError);
}

void LoginWindow::handleLogin()
{
    const QString username = usernameInput->text().trimmed();
    const QString password = passwordInput->text();

    if (username.isEmpty() || password.isEmpty()) {
        showLoginError("Enter both username and password.");
        return;
    }

    messageLabel->setText("Signing in...");
    loginButton->setEnabled(false);

    apiClient->login(username, password);
}

void LoginWindow::showRoleMenu(const QString &role)
{
    auto *dashboard = new QWidget;
    dashboard->setWindowTitle("HMS Dashboard - " + role);
    dashboard->resize(600, 400);

    auto *dashboardLayout = new QVBoxLayout(dashboard);

    auto *heading = new QLabel(
        "Hospital Management System", dashboard
    );
    heading->setStyleSheet(
        "font-size: 22px; font-weight: bold;"
    );

    auto *roleLabel = new QLabel(
        "Logged in as: " + role, dashboard
    );

    dashboardLayout->addWidget(heading);
    dashboardLayout->addWidget(roleLabel);

    QStringList options;

    if (role.compare("Patient", Qt::CaseInsensitive) == 0) {
        options << "Appointments" << "Medical History";
    } else if (role.compare("Receptionist", Qt::CaseInsensitive) == 0) {
        options << "Patient Management" << "Appointments";
    } else if (role.compare("Doctor", Qt::CaseInsensitive) == 0) {
        options << "Appointments"
                << "Patient History"
                << "Prescriptions";
    } else if (
        role.compare("Billing Officer", Qt::CaseInsensitive) == 0 ||
        role.compare("BillingOfficer", Qt::CaseInsensitive) == 0
    ) {
        options << "Bills" << "Payments";
    } else if (
        role.compare("Administrator", Qt::CaseInsensitive) == 0 ||
        role.compare("Admin", Qt::CaseInsensitive) == 0
    ) {
        options << "User Management"
                << "Reports"
                << "Audit Records";
    } else {
        options << "Dashboard";
    }

    for (const QString &option : options) {
        auto *button = new QPushButton(option, dashboard);
        button->setMinimumHeight(40);
        dashboardLayout->addWidget(button);
    }

    dashboardLayout->addStretch();

    dashboard->show();
    hide();
}

void LoginWindow::showLoginError(const QString &message)
{
    messageLabel->setText(message);
    loginButton->setEnabled(true);
    passwordInput->clear();
    passwordInput->setFocus();
}
