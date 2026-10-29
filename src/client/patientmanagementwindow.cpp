#include "patientmanagementwindow.h"

#include "apiclient.h"

#include <QComboBox>
#include <QDate>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QKeySequence>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

PatientManagementWindow::PatientManagementWindow(
    ApiClient *client,
    bool canManagePatients,
    QWidget *parent)
    : QDialog(parent),
      apiClient(client),
      searchInput(new QLineEdit(this)),
      results(new QTableWidget(this)),
      patientIdLabel(new QLabel("New patient", this)),
      nameInput(new QLineEdit(this)),
      dateOfBirthInput(new QLineEdit(this)),
      genderInput(new QComboBox(this)),
      phoneInput(new QLineEdit(this)),
      addressInput(new QLineEdit(this)),
      statusLabel(new QLabel(this)),
      submitButton(new QPushButton("Register patient", this)),
      newPatientButton(new QPushButton("New patient", this)),
      historyButton(new QPushButton("View history", this))
{
    setWindowTitle("Patient management");
    resize(850, 600);

    auto *root = new QVBoxLayout(this);
    auto *toolbar = new QHBoxLayout;
    auto *sessionLabel = new QLabel(
        QString("Logged in as %1 | %2")
            .arg(client->username(), client->role()), this);
    auto *helpButton = new QPushButton("Help", this);
    auto *logoutButton = new QPushButton("Logout", this);
    toolbar->addWidget(sessionLabel);
    toolbar->addStretch();
    toolbar->addWidget(helpButton);
    toolbar->addWidget(logoutButton);
    root->addLayout(toolbar);
    auto *searchBox = new QGroupBox("Search patients", this);
    auto *searchLayout = new QVBoxLayout(searchBox);
    auto *searchRow = new QHBoxLayout;
    searchInput->setPlaceholderText("Patient ID, name prefix, or phone");
    searchInput->setAccessibleName("Patient search");
    auto *searchButton = new QPushButton("Search", searchBox);
    searchRow->addWidget(searchInput);
    searchRow->addWidget(searchButton);
    searchLayout->addLayout(searchRow);
    results->setColumnCount(4);
    results->setHorizontalHeaderLabels(
        {"Patient ID", "Name", "Date of birth", "Phone"});
    results->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    results->setSelectionBehavior(QAbstractItemView::SelectRows);
    results->setSelectionMode(QAbstractItemView::SingleSelection);
    results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results->setMaximumHeight(180);
    searchLayout->addWidget(results);
    root->addWidget(searchBox);

    auto *formBox = new QGroupBox("Patient details", this);
    auto *form = new QFormLayout(formBox);
    genderInput->addItems({"Female", "Male", "Other"});
    dateOfBirthInput->setInputMask("00-00-0000;_");
    dateOfBirthInput->setToolTip("Enter the date as DD-MM-YYYY.");
    nameInput->setAccessibleName("Patient name");
    dateOfBirthInput->setAccessibleName("Date of birth");
    genderInput->setAccessibleName("Gender");
    phoneInput->setAccessibleName("Phone");
    addressInput->setAccessibleName("Address");
    nameInput->setMaxLength(60);
    phoneInput->setMaxLength(10);
    phoneInput->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[0-9]{10}"), phoneInput));
    addressInput->setMaxLength(120);
    form->addRow("Patient ID", patientIdLabel);
    form->addRow("Name *", nameInput);
    form->addRow("Date of birth *", dateOfBirthInput);
    form->addRow("Gender *", genderInput);
    form->addRow("Phone *", phoneInput);
    form->addRow("Address *", addressInput);
    QWidget::setTabOrder(nameInput, dateOfBirthInput);
    QWidget::setTabOrder(dateOfBirthInput, genderInput);
    QWidget::setTabOrder(genderInput, phoneInput);
    QWidget::setTabOrder(phoneInput, addressInput);
    QWidget::setTabOrder(addressInput, submitButton);
    submitButton->setDefault(true);
    if (!canManagePatients) {
        formBox->hide();
    }
    root->addWidget(formBox);

    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet("color: #a61b1b;");
    root->addWidget(statusLabel);
    const bool canViewPatientHistory =
        canManagePatients ||
        client->role().compare("Doctor", Qt::CaseInsensitive) == 0;
    if (canManagePatients || canViewPatientHistory) {
        auto *patientActions = new QHBoxLayout;
        if (canManagePatients) {
            patientActions->addWidget(newPatientButton);
            patientActions->addWidget(submitButton, 1);
        } else {
            newPatientButton->hide();
            submitButton->hide();
        }
        if (canManagePatients) {
            registrationTimer.start();
            auto *registerShortcut =
                new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
            connect(registerShortcut, &QShortcut::activated,
                    this, &PatientManagementWindow::submitPatient);
            QTimer::singleShot(0, this, [this]() { nameInput->setFocus(); });
        }
        historyButton->setEnabled(false);
        patientActions->addWidget(historyButton);
        root->addLayout(patientActions);
    } else {
        newPatientButton->hide();
        submitButton->hide();
        historyButton->hide();
    }

    connect(searchButton, &QPushButton::clicked,
            this, &PatientManagementWindow::searchPatients);
    connect(searchInput, &QLineEdit::returnPressed,
            this, &PatientManagementWindow::searchPatients);
    connect(results, &QTableWidget::cellClicked,
            this, &PatientManagementWindow::showSearchResult);
    connect(submitButton, &QPushButton::clicked,
            this, &PatientManagementWindow::submitPatient);
    connect(newPatientButton, &QPushButton::clicked,
            this, &PatientManagementWindow::clearForm);
    connect(historyButton, &QPushButton::clicked, this, [this]() {
        const QString patientId = patientIdLabel->text();
        if (patientId == "New patient") return;
        statusLabel->setStyleSheet("color: #334155;");
        statusLabel->setText("Loading patient history...");
        apiClient->getPatientHistory(patientId);
    });
    connect(helpButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(
            this, "Patient management help",
            "Search by patient ID, name prefix, or phone. "
            "Receptionists can register or update a patient by selecting "
            "a search result. Receptionists and doctors can view the "
            "selected patient's change history. Fields marked with * "
            "are required.");
    });
    connect(logoutButton, &QPushButton::clicked, this, [this]() {
        apiClient->logout();
        close();
    });
    connect(apiClient, &ApiClient::patientOperationSucceeded,
            this, &PatientManagementWindow::handlePatientSuccess);
    connect(apiClient, &ApiClient::patientOperationFailed,
            this, &PatientManagementWindow::handlePatientError);
    connect(apiClient, &ApiClient::patientDuplicateDetected,
            this, &PatientManagementWindow::confirmDuplicateRegistration);
}

void PatientManagementWindow::searchPatients()
{
    const QString query = searchInput->text().trimmed();
    if (query.isEmpty()) {
        statusLabel->setStyleSheet("color: #a61b1b;");
        statusLabel->setText("Enter a patient ID, name prefix, or phone.");
        return;
    }
    statusLabel->setStyleSheet("color: #334155;");
    statusLabel->setText("Searching...");
    apiClient->searchPatients(query);
}

QJsonObject PatientManagementWindow::patientForm() const
{
    return {
        {"name", nameInput->text().trimmed()},
        {"dateOfBirth", dateOfBirthInput->text().trimmed()},
        {"gender", genderInput->currentText()},
        {"phone", phoneInput->text().trimmed()},
        {"address", addressInput->text().trimmed()}
    };
}

void PatientManagementWindow::submitPatient()
{
    const auto reject = [this](QLineEdit *field, const QString &message) {
        statusLabel->setStyleSheet("color: #a61b1b;");
        statusLabel->setText(message);
        field->setFocus();
        field->selectAll();
    };
    const QString name = nameInput->text().trimmed();
    const QString dateOfBirth = dateOfBirthInput->text().trimmed();
    const QString phone = phoneInput->text().trimmed();
    const QString address = addressInput->text().trimmed();
    const QDate parsedDate =
        QDate::fromString(dateOfBirth, "dd-MM-yyyy");
    if (name.isEmpty() || name.size() > 60) {
        reject(nameInput, "Enter a patient name (up to 60 characters).");
        return;
    }
    if (!dateOfBirthInput->hasAcceptableInput() ||
        !parsedDate.isValid() || parsedDate > QDate::currentDate()) {
        reject(dateOfBirthInput,
               "Enter a valid date of birth as DD-MM-YYYY, not in the future.");
        return;
    }
    if (phone.size() != 10 || !phoneInput->hasAcceptableInput()) {
        reject(phoneInput, "Enter a 10-digit phone number.");
        return;
    }
    if (address.isEmpty() || address.size() > 120) {
        reject(addressInput, "Enter an address (up to 120 characters).");
        return;
    }
    submitButton->setEnabled(false);
    statusLabel->setStyleSheet("color: #334155;");
    statusLabel->setText("Saving patient...");
    if (patientIdLabel->text() == "New patient") {
        apiClient->registerPatient(patientForm());
    } else {
        apiClient->updatePatient(patientIdLabel->text(), patientForm());
    }
}

void PatientManagementWindow::showSearchResult(int row, int)
{
    if (row < 0 || results->item(row, 0) == nullptr) return;
    const QString patientId =
        results->item(row, 0)->data(Qt::UserRole).toString();
    if (patientId.isEmpty()) return;
    patientIdLabel->setText(patientId);
    historyButton->setEnabled(true);
    nameInput->setText(results->item(row, 1)->text());
    dateOfBirthInput->setText(results->item(row, 2)->text());
    phoneInput->setText(results->item(row, 3)->text());
    genderInput->setCurrentText(
        results->item(row, 1)->data(Qt::UserRole).toString());
    addressInput->setText(
        results->item(row, 1)->data(Qt::UserRole + 1).toString());
    submitButton->setText("Save patient changes");
}

void PatientManagementWindow::handlePatientSuccess(
    const QString &operation,
    const QJsonObject &response)
{
    if (operation == "history") {
        const QJsonArray history = response.value("history").toArray();
        if (history.isEmpty()) {
            QMessageBox::information(
                this, "Patient history",
                "No previous patient changes were recorded.");
            return;
        }

        QStringList changes;
        for (const QJsonValue &value : history) {
            const QJsonObject entry = value.toObject();
            const QJsonObject oldValues = entry.value("oldValues").toObject();
            changes.append(
                QString("Changed at: %1\nChanged by: %2\n"
                        "Name: %3\nDate of birth: %4\nGender: %5\n"
                        "Phone: %6\nAddress: %7")
                    .arg(entry.value("changedAt").toString(),
                         entry.value("changedBy").toString(),
                         oldValues.value("name").toString(),
                         oldValues.value("dateOfBirth").toString(),
                         oldValues.value("gender").toString(),
                         oldValues.value("phone").toString(),
                         oldValues.value("address").toString()));
        }
        QMessageBox::information(
            this, "Patient change history", changes.join("\n\n"));
        return;
    }

    if (operation == "search") {
        const QJsonArray patients = response.value("matches").toArray();
        results->setRowCount(patients.size());
        for (int row = 0; row < patients.size(); ++row) {
            const QJsonObject patient = patients.at(row).toObject();
            const QString fields[] = {
                patient.value("patientId").toString(),
                patient.value("name").toString(),
                patient.value("dateOfBirth").toString(),
                patient.value("phone").toString()
            };
            for (int column = 0; column < 4; ++column) {
                auto *item = new QTableWidgetItem(fields[column]);
                if (column == 0) {
                    item->setData(Qt::UserRole, fields[0]);
                } else if (column == 1) {
                    item->setData(
                        Qt::UserRole, patient.value("gender").toString());
                    item->setData(
                        Qt::UserRole + 1,
                        patient.value("address").toString());
                }
                results->setItem(row, column, item);
            }
        }
        statusLabel->setText(
            response.value("message").toString(
                patients.isEmpty() ? "No patient found" : "Search complete."));
        return;
    }

    const QJsonObject patient = response.value("patient").toObject();
    patientIdLabel->setText(patient.value("patientId").toString());
    if (operation == "register") {
        const qint64 elapsed = registrationTimer.elapsed();
        const qint64 minutes = elapsed / 60000;
        const qint64 seconds = (elapsed / 1000) % 60;
        const QString duration =
            QString("%1:%2").arg(minutes, 2, 10, QChar('0'))
                            .arg(seconds, 2, 10, QChar('0'));
        const QString targetResult = elapsed <= 180000
            ? "within the 3-minute target."
            : "over the 3-minute target.";
        statusLabel->setText(
            QString("Patient registered in %1 (%2)")
                .arg(duration, targetResult));
    } else {
        statusLabel->setText("Patient updated successfully.");
    }
    statusLabel->setStyleSheet("color: #14532d;");
    submitButton->setEnabled(true);
    submitButton->setText("Save patient changes");
}

void PatientManagementWindow::handlePatientError(
    const QString &,
    const QString &message)
{
    statusLabel->setStyleSheet("color: #a61b1b;");
    statusLabel->setText(message);
    submitButton->setEnabled(true);
}

void PatientManagementWindow::confirmDuplicateRegistration(
    const QString &message)
{
    const auto answer = QMessageBox::warning(
        this, "Possible duplicate patient",
        message + "\n\nDo you want to save this patient anyway?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        statusLabel->setStyleSheet("color: #334155;");
        statusLabel->setText("Saving confirmed duplicate...");
        apiClient->registerPatient(patientForm(), true);
        return;
    }

    statusLabel->setStyleSheet("color: #a61b1b;");
    statusLabel->setText("Registration cancelled; no patient was saved.");
    submitButton->setEnabled(true);
}

void PatientManagementWindow::clearForm()
{
    registrationTimer.restart();
    patientIdLabel->setText("New patient");
    historyButton->setEnabled(false);
    nameInput->clear();
    dateOfBirthInput->clear();
    genderInput->setCurrentIndex(0);
    phoneInput->clear();
    addressInput->clear();
    submitButton->setText("Register patient");
    statusLabel->clear();
    nameInput->setFocus();
}
