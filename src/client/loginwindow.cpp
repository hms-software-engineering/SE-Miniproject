#include "loginwindow.h"

#include "apiclient.h"
#include "patientmanagementwindow.h"
#include "workflowwindow.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int SESSION_IDLE_TIMEOUT_MS = 10 * 60 * 1000;

}

LoginWindow::LoginWindow(QWidget *parent)
    : QMainWindow(parent),
      apiClient(new ApiClient(this)),
      pages(new QStackedWidget(this)),
      loginPage(new QWidget(pages)),
      dashboardPage(new QWidget(pages)),
      usernameInput(new QLineEdit(loginPage)),
      passwordInput(new QLineEdit(loginPage)),
      messageLabel(new QLabel(loginPage)),
      loginButton(new QPushButton("Login", loginPage)),
      workAreaLabel(new QLabel(dashboardPage)),
      sessionStatusLabel(new QLabel(this)),
      navigation(new QListWidget(dashboardPage)),
      sessionTimer(new QTimer(this))
{
    setWindowTitle("Hospital Management System");
    resize(1000, 660);
    setMinimumSize(760, 500);
    setStyleSheet("QWidget { font-size: 11pt; }");
    setCentralWidget(pages);
    statusBar()->addPermanentWidget(sessionStatusLabel);

    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(80, 55, 80, 55);
    loginLayout->addStretch();
    auto *heading = new QLabel("Hospital Management System", loginPage);
    heading->setStyleSheet("font-size: 22pt; font-weight: bold;");
    heading->setAlignment(Qt::AlignCenter);
    auto *loginTitle = new QLabel("Sign in", loginPage);
    loginTitle->setAlignment(Qt::AlignCenter);
    usernameInput->setObjectName("usernameInput");
    usernameInput->setAccessibleName("Username");
    usernameInput->setPlaceholderText("Username *");
    passwordInput->setObjectName("passwordInput");
    passwordInput->setAccessibleName("Password");
    passwordInput->setPlaceholderText("Password *");
    passwordInput->setEchoMode(QLineEdit::Password);
    messageLabel->setWordWrap(true);
    messageLabel->setStyleSheet("color: #a61b1b;");
    auto *helpButton = new QPushButton("Help", loginPage);
    loginButton->setDefault(true);
    loginLayout->addWidget(heading);
    loginLayout->addSpacing(12);
    loginLayout->addWidget(loginTitle);
    loginLayout->addSpacing(18);
    loginLayout->addWidget(new QLabel("Username *", loginPage));
    loginLayout->addWidget(usernameInput);
    loginLayout->addWidget(new QLabel("Password *", loginPage));
    loginLayout->addWidget(passwordInput);
    loginLayout->addWidget(messageLabel);
    loginLayout->addWidget(loginButton);
    loginLayout->addWidget(helpButton, 0, Qt::AlignRight);
    loginLayout->addStretch();

    auto *dashboardLayout = new QHBoxLayout(dashboardPage);
    dashboardLayout->setContentsMargins(0, 0, 0, 0);
    auto *sidebar = new QWidget(dashboardPage);
    sidebar->setMinimumWidth(220);
    sidebar->setMaximumWidth(280);
    sidebar->setStyleSheet("QWidget { background-color: #edf2f7; }");
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    auto *brand = new QLabel("HMS", sidebar);
    brand->setStyleSheet("font-size: 18pt; font-weight: bold; color: #12345a;");
    navigation->setStyleSheet(
        "QListWidget { background: #edf2f7; border: none; }"
        "QListWidget::item { padding: 10px; color: #14263d; }"
        "QListWidget::item:selected { background: #c9dcef; }");
    auto *dashboardHelpButton = new QPushButton("Help", sidebar);
    sidebarLayout->addWidget(brand);
    sidebarLayout->addWidget(navigation, 1);
    sidebarLayout->addWidget(dashboardHelpButton);

    auto *workArea = new QWidget(dashboardPage);
    auto *workLayout = new QVBoxLayout(workArea);
    auto *welcome = new QLabel("Hospital Management System", workArea);
    welcome->setStyleSheet(
        "font-size: 20pt; font-weight: bold; color: #12345a;");
    auto *dashboardLogoutButton = new QPushButton("Logout", workArea);
    auto *dashboardHeader = new QHBoxLayout;
    dashboardHeader->addWidget(welcome);
    dashboardHeader->addStretch();
    dashboardHeader->addWidget(dashboardLogoutButton);
    workAreaLabel->setWordWrap(true);
    workAreaLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    workAreaLabel->setStyleSheet(
        "QLabel { background: #ffffff; color: #17212f; padding: 20px; }");
    workLayout->addLayout(dashboardHeader);
    workLayout->addWidget(workAreaLabel, 1);
    dashboardLayout->addWidget(sidebar);
    dashboardLayout->addWidget(workArea, 1);

    pages->addWidget(loginPage);
    pages->addWidget(dashboardPage);
    pages->setCurrentWidget(loginPage);
    sessionTimer->setSingleShot(true);
    qApp->installEventFilter(this);

    connect(loginButton, &QPushButton::clicked,
            this, &LoginWindow::handleLogin);
    connect(passwordInput, &QLineEdit::returnPressed,
            this, &LoginWindow::handleLogin);
    connect(helpButton, &QPushButton::clicked,
            this, &LoginWindow::showHelp);
    connect(dashboardHelpButton, &QPushButton::clicked,
            this, &LoginWindow::showHelp);
    connect(dashboardLogoutButton, &QPushButton::clicked,
            this, &LoginWindow::logout);
    connect(navigation, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) {
        handleMenuSelection(navigation->row(item));
    });
    connect(apiClient, &ApiClient::loginSucceeded,
            this, &LoginWindow::showRoleMenu);
    connect(apiClient, &ApiClient::loginFailed,
            this, &LoginWindow::showLoginError);
    connect(apiClient, &ApiClient::logoutFinished, this,
            [this](bool success, const QString &message) {
        if (!success) {
            statusBar()->showMessage(message, 10000);
        }
    });
    connect(apiClient, &ApiClient::sessionEnded,
            this, &LoginWindow::expireSession);
    connect(sessionTimer, &QTimer::timeout,
            this, &LoginWindow::logout);

    auto *patientSearchShortcut = new QShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_F), this);
    connect(patientSearchShortcut, &QShortcut::activated, this, [this]() {
        if (activeRole.compare("Receptionist", Qt::CaseInsensitive) == 0) {
            showPatientWindow(true);
        } else if (activeRole.compare("Doctor", Qt::CaseInsensitive) == 0) {
            showPatientWindow(false);
        }
    });
}

LoginWindow::~LoginWindow()
{
    qApp->removeEventFilter(this);
}

bool LoginWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    if (!apiClient->token().isEmpty() &&
        (event->type() == QEvent::MouseButtonPress ||
         event->type() == QEvent::KeyPress ||
         event->type() == QEvent::TouchBegin)) {
        sessionTimer->start(SESSION_IDLE_TIMEOUT_MS);
    }
    return QMainWindow::eventFilter(watched, event);
}

void LoginWindow::handleLogin()
{
    const QString username = usernameInput->text().trimmed();
    const QString password = passwordInput->text();
    if (username.isEmpty() || password.isEmpty()) {
        showLoginError("Enter both username and password.");
        return;
    }
    activeUsername = username;
    messageLabel->setStyleSheet("color: #334155;");
    messageLabel->setText("Signing in...");
    loginButton->setEnabled(false);
    usernameInput->setEnabled(false);
    passwordInput->setEnabled(false);
    apiClient->login(username, password);
}

void LoginWindow::showRoleMenu(const QString &role)
{
    activeRole = role;
    passwordInput->clear();
    sessionTimer->start(SESSION_IDLE_TIMEOUT_MS);
    navigation->clear();
    QStringList options;
    if (role.compare("Patient", Qt::CaseInsensitive) == 0) {
        options << "My Profile" << "Appointments";
    } else if (role.compare("Receptionist", Qt::CaseInsensitive) == 0) {
        options << "Patient Management" << "Appointments";
    } else if (role.compare("Doctor", Qt::CaseInsensitive) == 0) {
        options << "Patient Search" << "Consultations";
    } else if (role.compare("Administrator", Qt::CaseInsensitive) == 0 ||
               role.compare("Admin", Qt::CaseInsensitive) == 0) {
        options << "User Management";
    } else if (role.compare("Billing Officer", Qt::CaseInsensitive) == 0 ||
               role.compare("BillingOfficer", Qt::CaseInsensitive) == 0) {
        options.clear();
    } else {
        showLoginError("The server returned an unrecognized user role.");
        apiClient->logout();
        return;
    }
    for (const QString &option : options) {
        navigation->addItem(option);
    }
    sessionStatusLabel->setText(
        QString("%1 | %2").arg(activeUsername, activeRole));
    workAreaLabel->setText(
        options.isEmpty()
            ? "No Sprint 1 desktop workflows are assigned to this role."
            : "Choose a Sprint 1 workflow from the menu. "
              "This build includes working patient, appointment, "
              "consultation, and account-management screens.");
    pages->setCurrentWidget(dashboardPage);
}

void LoginWindow::showLoginError(const QString &message)
{
    messageLabel->setStyleSheet("color: #a61b1b;");
    messageLabel->setText(message);
    loginButton->setEnabled(true);
    usernameInput->setEnabled(true);
    passwordInput->setEnabled(true);
    passwordInput->clear();
    passwordInput->setFocus();
}

void LoginWindow::handleMenuSelection(int row)
{
    if (row < 0 || row >= navigation->count()) {
        return;
    }
    const QString option = navigation->item(row)->text();
    if (option == "Patient Management") {
        showPatientWindow(true);
        return;
    }
    if (option == "Patient Search") {
        showPatientWindow(false);
        return;
    }
    if (option == "Appointments") {
        showWorkflow(WorkflowWindow::Workflow::AppointmentBooking);
        return;
    }
    if (option == "Consultations") {
        showWorkflow(WorkflowWindow::Workflow::Consultation);
        return;
    }
    if (option == "User Management") {
        showWorkflow(WorkflowWindow::Workflow::UserManagement);
        return;
    }
    if (option == "My Profile") {
        showWorkflow(WorkflowWindow::Workflow::PatientProfile);
        return;
    }
    statusBar()->showMessage(
        QString("No Sprint 1 workflow is available for \"%1\".").arg(option),
        5000);
}

void LoginWindow::showPatientWindow(bool canManagePatients)
{
    auto *patients = new PatientManagementWindow(apiClient,
                                                  canManagePatients, this);
    patients->setAttribute(Qt::WA_DeleteOnClose);
    patients->show();
    patients->raise();
    patients->activateWindow();
}

void LoginWindow::showWorkflow(WorkflowWindow::Workflow workflow)
{
    auto *window = new WorkflowWindow(apiClient, workflow, this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
    window->raise();
    window->activateWindow();
}

void LoginWindow::logout()
{
    if (apiClient->token().isEmpty()) {
        showLoginPage();
        return;
    }
    apiClient->logout();
}

void LoginWindow::expireSession()
{
    sessionTimer->stop();
    for (auto *dialog : findChildren<QDialog*>()) {
        dialog->close();
    }
    showLoginPage();
    showLoginError("You have been signed out. Please log in again.");
}

void LoginWindow::showLoginPage(const QString &message)
{
    sessionTimer->stop();
    activeUsername.clear();
    activeRole.clear();
    passwordInput->clear();
    usernameInput->setEnabled(true);
    passwordInput->setEnabled(true);
    sessionStatusLabel->clear();
    navigation->clear();
    pages->setCurrentWidget(loginPage);
    if (!message.isEmpty()) {
        showLoginError(message);
    }
}

void LoginWindow::showHelp()
{
    QMessageBox::information(
        this, "HMS help",
        "Enter your assigned username and password to sign in. "
        "Use Tab to move between fields and Enter to submit. "
        "After signing in, choose an available workflow from the left menu. "
        "Press Ctrl+F to open patient search when your role permits it. "
        "Use Logout to end your session.");
}
