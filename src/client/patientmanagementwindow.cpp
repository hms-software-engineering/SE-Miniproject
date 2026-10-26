#include "patientmanagementwindow.h"

#include "apiclient.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
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
      newPatientButton(new QPushButton("New patient", this))
{
    setWindowTitle("Patient management");
    resize(850, 680);

    auto *root = new QVBoxLayout(this);
    auto *searchBox = new QGroupBox("Search patients", this);
    auto *searchLayout = new QVBoxLayout(searchBox);
    auto *searchRow = new QHBoxLayout;
    searchInput->setPlaceholderText("Patient ID, name prefix, or phone");
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
    searchLayout->addWidget(results);
    root->addWidget(searchBox);

    auto *formBox = new QGroupBox("Patient details", this);
    auto *form = new QFormLayout(formBox);
    genderInput->addItems({"Female", "Male", "Other"});
    dateOfBirthInput->setPlaceholderText("DD-MM-YYYY");
    phoneInput->setMaxLength(10);
    addressInput->setMaxLength(120);
    form->addRow("Patient ID", patientIdLabel);
    form->addRow("Name", nameInput);
    form->addRow("Date of birth", dateOfBirthInput);
    form->addRow("Gender", genderInput);
    form->addRow("Phone", phoneInput);
    form->addRow("Address", addressInput);
    if (!canManagePatients) {
        formBox->hide();
    }
    root->addWidget(formBox);

    statusLabel->setWordWrap(true);
    root->addWidget(statusLabel);
    if (canManagePatients) {
        root->addWidget(newPatientButton);
        root->addWidget(submitButton);
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
    connect(apiClient, &ApiClient::patientOperationSucceeded,
            this, &PatientManagementWindow::handlePatientSuccess);
    connect(apiClient, &ApiClient::patientOperationFailed,
            this, &PatientManagementWindow::handlePatientError);
}

void PatientManagementWindow::searchPatients()
{
    const QString query = searchInput->text().trimmed();
    if (query.isEmpty()) {
        statusLabel->setText("Enter a patient ID, name prefix, or phone.");
        return;
    }
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
    if (nameInput->text().trimmed().isEmpty() ||
        dateOfBirthInput->text().trimmed().isEmpty() ||
        phoneInput->text().trimmed().isEmpty() ||
        addressInput->text().trimmed().isEmpty()) {
        statusLabel->setText("Complete all patient fields before saving.");
        return;
    }
    submitButton->setEnabled(false);
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
    statusLabel->setText(operation == "register"
        ? "Patient registered successfully."
        : "Patient updated successfully.");
    submitButton->setEnabled(true);
    submitButton->setText("Save patient changes");
}

void PatientManagementWindow::handlePatientError(
    const QString &,
    const QString &message)
{
    statusLabel->setText(message);
    submitButton->setEnabled(true);
}

void PatientManagementWindow::clearForm()
{
    patientIdLabel->setText("New patient");
    nameInput->clear();
    dateOfBirthInput->clear();
    genderInput->setCurrentIndex(0);
    phoneInput->clear();
    addressInput->clear();
    submitButton->setText("Register patient");
}
