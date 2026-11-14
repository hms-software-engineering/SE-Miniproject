#include "workflowwindow.h"

#include "apiclient.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>

WorkflowWindow::WorkflowWindow(ApiClient *client, Workflow selectedWorkflow,
                               QWidget *parent)
    : QDialog(parent),
      apiClient(client),
      workflow(selectedWorkflow),
      statusLabel(new QLabel(this)),
      primaryButton(new QPushButton(this)),
      patientIdInput(new QLineEdit(this)),
      doctorIdInput(new QLineEdit(this)),
      appointmentDateInput(new QDateEdit(this)),
      slotInput(new QComboBox(this)),
      appointmentIdInput(new QLineEdit(this)),
      symptomsInput(new QTextEdit(this)),
      diagnosisInput(new QLineEdit(this)),
      notesInput(new QTextEdit(this)),
      prescriptionPanel(new QGroupBox("Add prescription", this)),
      visitIdInput(new QLineEdit(this)),
      medicineInput(new QLineEdit(this)),
      dosageInput(new QLineEdit(this)),
      frequencyInput(new QLineEdit(this)),
      durationInput(new QSpinBox(this)),
      usernameInput(new QLineEdit(this)),
      passwordInput(new QLineEdit(this)),
      roleInput(new QComboBox(this)),
      profileNameInput(new QLineEdit(this)),
      profileDateOfBirthInput(new QLineEdit(this)),
      profileGenderInput(new QComboBox(this)),
      profilePhoneInput(new QLineEdit(this)),
      profileAddressInput(new QLineEdit(this)),
      scheduleDateInput(new QDateEdit(this)),
      scheduleRow(new QWidget(this)),
      scheduledAppointmentInput(new QComboBox(this)),
      patientContextLabel(new QLabel(this)),
      billVisitIdInput(new QLineEdit(this)),
      billItemsInput(new QTextEdit(this)),
      billIdInput(new QLineEdit(this)),
      paymentAmountInput(new QLineEdit(this)),
      paymentModeInput(new QComboBox(this)),
      receiptPaymentIdInput(new QLineEdit(this)),
      reportStartDateInput(new QDateEdit(this)),
      reportEndDateInput(new QDateEdit(this)),
      reportFormatInput(new QComboBox(this)),
      auditUsernameInput(new QLineEdit(this)),
      auditFromInput(new QLineEdit(this)),
      auditToInput(new QLineEdit(this)),
      auditActionInput(new QComboBox(this)),
      resultOutput(new QTextEdit(this)),
      targetUserIdInput(new QLineEdit(this)),
      managedUsersInput(new QComboBox(this)),
      managedRoleInput(new QComboBox(this))
{
    setModal(true);
    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    QWidget *appointmentDateRow = nullptr;
    statusLabel->setWordWrap(true);
    primaryButton->setDefault(true);
    auto *helpRow = new QHBoxLayout;
    auto *helpButton = new QPushButton("Workflow help", this);
    helpButton->setAccessibleName("Workflow instructions");
    helpButton->setToolTip(
        "Show the steps and required information for this workflow.");
    helpRow->addStretch();
    helpRow->addWidget(helpButton);
    root->addLayout(helpRow);
    QString helpText;

    switch (workflow) {
    case Workflow::AppointmentBooking: {
        setWindowTitle("Book an appointment");
        helpText =
            "1. Enter the patient's ID (PID- followed by 8 digits).\n"
            "2. Enter the doctor's ID and choose a date.\n"
            "3. Select Find available slots, then choose a listed time.\n"
            "4. Select Book appointment and wait for the confirmation.\n\n"
            "Patient accounts may book only for their own linked patient "
            "record. A patient profile must be created first.";
        resize(600, 360);
        patientIdInput->setPlaceholderText("e.g. PID-00000001");
        doctorIdInput->setText("1");
        doctorIdInput->setPlaceholderText("Doctor ID");
        appointmentDateInput->setCalendarPopup(true);
        appointmentDateInput->setDisplayFormat("yyyy-MM-dd");
        appointmentDateInput->setDateRange(
            QDate::currentDate(), QDate::currentDate().addDays(60));
        QDate firstWeekday = QDate::currentDate();
        while (firstWeekday.dayOfWeek() > 5) {
            firstWeekday = firstWeekday.addDays(1);
        }
        appointmentDateInput->setDate(firstWeekday);
        auto *refreshButton = new QPushButton("Find available slots", this);
        appointmentDateRow = new QWidget(this);
        auto *dateLayout = new QHBoxLayout(appointmentDateRow);
        dateLayout->setContentsMargins(0, 0, 0, 0);
        dateLayout->addWidget(appointmentDateInput);
        dateLayout->addWidget(refreshButton);
        slotInput->setEnabled(false);
        form->addRow("Patient ID *", patientIdInput);
        form->addRow("Doctor ID *", doctorIdInput);
        form->addRow("Date *", appointmentDateRow);
        form->addRow("Available time *", slotInput);
        primaryButton->setText("Book appointment");
        root->addWidget(new QLabel(
            "Choose a free 30-minute slot. Receptionists can book for any "
            "registered patient; patient accounts can book only for their "
            "own linked patient record.", this));
        root->addLayout(form);
        root->addWidget(statusLabel);
        root->addWidget(primaryButton);
        connect(refreshButton, &QPushButton::clicked,
                this, &WorkflowWindow::loadFreeSlots);
        connect(primaryButton, &QPushButton::clicked,
                this, &WorkflowWindow::bookAppointment);
        break;
    }
    case Workflow::Consultation: {
        setWindowTitle("Record consultation");
        helpText =
            "1. Enter the ID of a confirmed appointment assigned to you.\n"
            "2. Enter symptoms and diagnosis. Notes are optional.\n"
            "3. Select Save consultation and wait for the visit ID.\n"
            "4. Enter the medicine, dosage, frequency, and duration, then "
            "save the prescription.\n\n"
            "Only the doctor assigned to the appointment can record its "
            "consultation.";
        resize(620, 560);
        appointmentIdInput->setPlaceholderText("Confirmed appointment ID");
        scheduleDateInput->setCalendarPopup(true);
        scheduleDateInput->setDisplayFormat("yyyy-MM-dd");
        scheduleDateInput->setDate(QDate::currentDate());
        auto *loadScheduleButton = new QPushButton("Load my appointments", this);
        auto *scheduleLayout = new QHBoxLayout(scheduleRow);
        scheduleLayout->setContentsMargins(0, 0, 0, 0);
        scheduleLayout->addWidget(scheduleDateInput);
        scheduleLayout->addWidget(loadScheduleButton);
        patientContextLabel->setWordWrap(true);
        patientContextLabel->setText(
            "Select an appointment to load patient identity and allergy details.");
        symptomsInput->setPlaceholderText("Symptoms");
        symptomsInput->setMaximumHeight(90);
        diagnosisInput->setPlaceholderText("Diagnosis");
        notesInput->setPlaceholderText("Notes (optional)");
        notesInput->setMaximumHeight(90);
        form->addRow("My schedule *", scheduleRow);
        form->addRow("Select appointment", scheduledAppointmentInput);
        form->addRow("Appointment ID *", appointmentIdInput);
        form->addRow("Patient context", patientContextLabel);
        form->addRow("Symptoms *", symptomsInput);
        form->addRow("Diagnosis *", diagnosisInput);
        form->addRow("Notes", notesInput);
        primaryButton->setText("Save consultation");
        root->addWidget(new QLabel(
            "Only the doctor assigned to a confirmed appointment can record "
            "its consultation.", this));
        root->addLayout(form);
        root->addWidget(statusLabel);
        root->addWidget(primaryButton);

        durationInput->setParent(prescriptionPanel);
        auto *prescriptionForm = new QFormLayout(prescriptionPanel);
        visitIdInput->setReadOnly(true);
        medicineInput->setPlaceholderText("Medicine");
        dosageInput->setPlaceholderText("e.g. 1 tablet");
        frequencyInput->setPlaceholderText("e.g. Twice daily");
        durationInput->setRange(1, 3650);
        durationInput->setSuffix(" days");
        prescriptionForm->addRow("Visit ID", visitIdInput);
        prescriptionForm->addRow("Medicine *", medicineInput);
        prescriptionForm->addRow("Dosage *", dosageInput);
        prescriptionForm->addRow("Frequency *", frequencyInput);
        prescriptionForm->addRow("Duration *", durationInput);
        auto *addPrescriptionButton =
            new QPushButton("Save prescription", prescriptionPanel);
        prescriptionForm->addRow(addPrescriptionButton);
        prescriptionPanel->hide();
        root->addWidget(prescriptionPanel);
        connect(primaryButton, &QPushButton::clicked,
                this, &WorkflowWindow::recordVisit);
        connect(loadScheduleButton, &QPushButton::clicked,
                this, &WorkflowWindow::loadMySchedule);
        connect(scheduledAppointmentInput,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this, &WorkflowWindow::selectScheduledAppointment);
        connect(addPrescriptionButton, &QPushButton::clicked,
                this, &WorkflowWindow::addPrescription);
        break;
    }
    case Workflow::UserManagement: {
        setWindowTitle("User management");
        helpText =
            "1. Enter a username.\n"
            "2. Enter a password with at least 8 characters, including an "
            "uppercase letter, lowercase letter, digit, and special "
            "character.\n"
            "3. Choose the account role and select Create account.\n\n"
            "For a Doctor account, note the doctor ID shown in the "
            "confirmation.";
        resize(560, 440);
        usernameInput->setPlaceholderText("Username");
        passwordInput->setPlaceholderText("Password");
        passwordInput->setEchoMode(QLineEdit::Password);
        passwordInput->setToolTip(
            "At least 8 characters with uppercase, lowercase, a digit, "
            "and a special character.");
        roleInput->addItems({"Receptionist", "Doctor", "Patient",
                             "Billing Officer", "Administrator"});
        form->addRow("Username *", usernameInput);
        form->addRow("Password *", passwordInput);
        form->addRow("Role *", roleInput);
        primaryButton->setText("Create account");
        root->addWidget(new QLabel(
            "Passwords require at least 8 characters including uppercase, "
            "lowercase, a digit, and a special character.", this));
        root->addLayout(form);
        root->addWidget(statusLabel);
        root->addWidget(primaryButton);
        auto *refreshUsersButton = new QPushButton("Refresh user list", this);
        managedRoleInput->addItems({"Receptionist", "Patient",
                                    "Billing Officer", "Administrator"});
        auto *roleUpdateButton = new QPushButton("Update selected role", this);
        auto *roleRow = new QHBoxLayout;
        roleRow->addWidget(managedUsersInput, 2);
        roleRow->addWidget(managedRoleInput, 1);
        root->addLayout(roleRow);
        auto *accountActions = new QHBoxLayout;
        accountActions->addWidget(refreshUsersButton);
        accountActions->addWidget(roleUpdateButton);
        root->addLayout(accountActions);
        targetUserIdInput->setPlaceholderText("Target user ID");
        auto *deactivateButton = new QPushButton("Deactivate account", this);
        auto *deactivateRow = new QHBoxLayout;
        deactivateRow->addWidget(targetUserIdInput);
        deactivateRow->addWidget(deactivateButton);
        root->addLayout(deactivateRow);
        connect(deactivateButton, &QPushButton::clicked,
                this, &WorkflowWindow::deactivateUser);
        connect(refreshUsersButton, &QPushButton::clicked, this, [this]() {
            showStatus("Loading user accounts...");
            apiClient->getUsers();
        });
        connect(managedUsersInput,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this, roleUpdateButton](int index) {
            if (index < 0) return;
            targetUserIdInput->setText(
                managedUsersInput->currentData().toString());
            const QString selectedRole =
                managedUsersInput->itemData(index, Qt::UserRole + 1).toString();
            const int roleIndex = managedRoleInput->findText(selectedRole);
            if (roleIndex >= 0) managedRoleInput->setCurrentIndex(roleIndex);
            const bool doctorAccount = selectedRole == "Doctor";
            roleUpdateButton->setEnabled(!doctorAccount);
            if (doctorAccount) {
                showStatus(
                    "Doctor role changes require doctor-profile management.");
            }
        });
        connect(roleUpdateButton, &QPushButton::clicked, this, [this]() {
            const QString userId =
                managedUsersInput->currentData().toString();
            if (userId.isEmpty()) {
                showStatus("Refresh the user list and select an account.");
                return;
            }
            if (QMessageBox::question(
                    this, "Confirm role change",
                    QString("Change user %1 to %2?")
                        .arg(userId, managedRoleInput->currentText())) !=
                QMessageBox::Yes) {
                showStatus("Role change cancelled.");
                return;
            }
            showStatus("Updating account role...");
            apiClient->changeUserRole(
                userId, managedRoleInput->currentText());
        });
        connect(primaryButton, &QPushButton::clicked,
                this, &WorkflowWindow::createUser);
        break;
    }
    case Workflow::PatientProfile: {
        setWindowTitle("Create my patient profile");
        helpText =
            "1. Enter your name, date of birth (DD-MM-YYYY), gender, "
            "10-digit phone number, and address.\n"
            "2. Select Save my profile and wait for the confirmation.\n"
            "3. Use the patient ID in the confirmation when booking an "
            "appointment.\n\n"
            "This creates the patient record linked to your login.";
        resize(520, 430);
        profileDateOfBirthInput->setPlaceholderText("DD-MM-YYYY");
        profileGenderInput->addItems({"Female", "Male", "Other"});
        profilePhoneInput->setMaxLength(10);
        profileAddressInput->setMaxLength(120);
        form->addRow("Name *", profileNameInput);
        form->addRow("Date of birth *", profileDateOfBirthInput);
        form->addRow("Gender *", profileGenderInput);
        form->addRow("Phone *", profilePhoneInput);
        form->addRow("Address *", profileAddressInput);
        primaryButton->setText("Save my profile");
        root->addWidget(new QLabel(
            "Create the patient record linked to your login. You will need "
            "the generated patient ID when booking an appointment.", this));
        root->addLayout(form);
        root->addWidget(statusLabel);
        root->addWidget(primaryButton);
        connect(primaryButton, &QPushButton::clicked, this, [this]() {
            const QString name = profileNameInput->text().trimmed();
            const QString dob = profileDateOfBirthInput->text().trimmed();
            const QString phone = profilePhoneInput->text().trimmed();
            const QString address = profileAddressInput->text().trimmed();
            if (name.isEmpty() || name.size() > 100 || dob.isEmpty() ||
                phone.isEmpty() || address.isEmpty()) {
                showStatus("Complete all profile fields before saving.");
                return;
            }
            setBusy(true);
            showStatus("Saving your patient profile...");
            apiClient->createMyPatientProfile({
                {"name", name}, {"dateOfBirth", dob},
                {"gender", profileGenderInput->currentText()},
                {"phone", phone}, {"address", address}
            });
        });
        break;
    }
    case Workflow::PatientRecords: {
        setWindowTitle("My health records");
        helpText =
            "Choose whether to load your own clinical history or bills. "
            "The server restricts these endpoints to the signed-in patient's "
            "linked records.";
        resize(700, 520);
        auto *historyButton = new QPushButton("Load my clinical history", this);
        auto *billsButton = new QPushButton("Load my bills", this);
        resultOutput->setReadOnly(true);
        resultOutput->setPlaceholderText("Your records appear here.");
        root->addWidget(new QLabel(
            "Only records linked to your signed-in patient account are shown.",
            this));
        root->addWidget(resultOutput, 1);
        root->addWidget(statusLabel);
        auto *recordActions = new QHBoxLayout;
        recordActions->addWidget(historyButton);
        recordActions->addWidget(billsButton);
        root->addLayout(recordActions);
        primaryButton->hide();
        connect(historyButton, &QPushButton::clicked, this, [this]() {
            showStatus("Loading your clinical history...");
            apiClient->getMyRecords("history");
        });
        connect(billsButton, &QPushButton::clicked, this, [this]() {
            showStatus("Loading your bills...");
            apiClient->getMyRecords("bills");
        });
        break;
    }
    case Workflow::Billing: {
        setWindowTitle("Billing and payments");
        helpText =
            "Create one itemized bill for a visit, record payments against "
            "its bill ID, and download a receipt using the returned payment ID.";
        resize(680, 720);
        billVisitIdInput->setPlaceholderText("Visit ID");
        billItemsInput->setPlaceholderText(
            "One item per line: description | amount (e.g. Consultation | 500.00)");
        billItemsInput->setMaximumHeight(100);
        auto *createBillButton = new QPushButton("Create itemized bill", this);
        auto *billForm = new QFormLayout;
        billForm->addRow("Visit ID *", billVisitIdInput);
        billForm->addRow("Items *", billItemsInput);
        root->addWidget(new QLabel(
            "Create bills from completed visits. Tax is applied by the server.",
            this));
        root->addLayout(billForm);
        root->addWidget(createBillButton);

        billIdInput->setPlaceholderText("Bill ID");
        paymentAmountInput->setPlaceholderText("Amount, e.g. 100.00");
        paymentModeInput->addItems({"Cash", "Card", "UPI"});
        auto *paymentButton = new QPushButton("Record payment", this);
        auto *paymentForm = new QFormLayout;
        paymentForm->addRow("Bill ID *", billIdInput);
        paymentForm->addRow("Amount *", paymentAmountInput);
        paymentForm->addRow("Mode *", paymentModeInput);
        root->addWidget(new QLabel("Record a payment", this));
        root->addLayout(paymentForm);
        root->addWidget(paymentButton);

        receiptPaymentIdInput->setPlaceholderText("Payment ID");
        auto *receiptButton = new QPushButton("Download receipt", this);
        auto *receiptForm = new QFormLayout;
        receiptForm->addRow("Payment ID *", receiptPaymentIdInput);
        root->addWidget(new QLabel("Retrieve a payment receipt", this));
        root->addLayout(receiptForm);
        root->addWidget(receiptButton);
        resultOutput->setReadOnly(true);
        resultOutput->setMaximumHeight(130);
        root->addWidget(resultOutput);
        root->addWidget(statusLabel);
        primaryButton->hide();
        connect(createBillButton, &QPushButton::clicked,
                this, &WorkflowWindow::createBill);
        connect(paymentButton, &QPushButton::clicked,
                this, &WorkflowWindow::recordPayment);
        connect(receiptButton, &QPushButton::clicked,
                this, &WorkflowWindow::loadReceipt);
        break;
    }
    case Workflow::Reports: {
        setWindowTitle("Appointment and revenue reports");
        helpText =
            "Choose an inclusive date range and CSV or PDF. The report is "
            "generated by the server and saved to a file you select.";
        resize(620, 380);
        for (auto *date : {reportStartDateInput, reportEndDateInput}) {
            date->setCalendarPopup(true);
            date->setDisplayFormat("yyyy-MM-dd");
            date->setDate(QDate::currentDate());
        }
        reportStartDateInput->setDate(
            QDate::currentDate().addDays(-7));
        reportFormatInput->addItems({"CSV", "PDF"});
        form->addRow("Start date *", reportStartDateInput);
        form->addRow("End date *", reportEndDateInput);
        form->addRow("Format *", reportFormatInput);
        primaryButton->setText("Generate and save report");
        root->addLayout(form);
        root->addWidget(statusLabel);
        root->addWidget(primaryButton);
        connect(primaryButton, &QPushButton::clicked,
                this, &WorkflowWindow::generateReport);
        break;
    }
    case Workflow::AuditLog: {
        setWindowTitle("Administrator audit log");
        helpText =
            "Optionally filter the audit log by username, action, and inclusive "
            "date range. Audit entries exclude passwords and clinical payloads.";
        resize(780, 600);
        auditUsernameInput->setPlaceholderText("Username (optional)");
        auditFromInput->setPlaceholderText("From date YYYY-MM-DD (optional)");
        auditToInput->setPlaceholderText("To date YYYY-MM-DD (optional)");
        auditActionInput->addItem("All actions", "");
        for (const QString &action : {
                 "LOGIN", "LOGIN_FAILED", "LOGOUT", "VIEW_ACCOUNT",
                 "VIEW_AUDIT_LOG", "GENERATE_DAILY_REPORT",
                 "VIEW_PATIENT_HISTORY", "VIEW_PATIENT",
                 "CREATE_PATIENT_PROFILE", "VIEW_OWN_SCHEDULE",
                 "VIEW_FREE_SLOTS", "CREATE_PATIENT", "UPDATE_PATIENT",
                 "DEACTIVATE_USER", "UPDATE_USER_ROLE", "CREATE_USER",
                 "CREATE_APPOINTMENT", "CANCEL_APPOINTMENT",
                 "RESCHEDULE_APPOINTMENT", "CREATE_VISIT",
                 "CREATE_PRESCRIPTION", "CREATE_BILL", "RECORD_PAYMENT",
                 "VIEW_RECEIPT", "UPDATE_PATIENT_ALLERGIES",
                 "OVERRIDE_ALLERGY_WARNING", "VIEW_VISIT_HISTORY",
                 "VIEW_OWN_HISTORY", "VIEW_OWN_BILLS"})
            auditActionInput->addItem(action, action);
        form->addRow("Username", auditUsernameInput);
        form->addRow("Action", auditActionInput);
        form->addRow("From", auditFromInput);
        form->addRow("To", auditToInput);
        resultOutput->setReadOnly(true);
        auto *loadAuditButton = new QPushButton("Load audit entries", this);
        root->addLayout(form);
        root->addWidget(loadAuditButton);
        root->addWidget(resultOutput, 1);
        root->addWidget(statusLabel);
        primaryButton->hide();
        connect(loadAuditButton, &QPushButton::clicked,
                this, &WorkflowWindow::loadAuditLog);
        break;
    }
    }

    connect(helpButton, &QPushButton::clicked, this, [this, helpText]() {
        QMessageBox::information(this, "Workflow help", helpText);
    });

    root->removeWidget(primaryButton);
    auto *backButton = new QPushButton("Back to dashboard", this);
    backButton->setToolTip(
        "Close this workflow. Use Logout on the dashboard to return to sign in.");
    auto *actions = new QHBoxLayout;
    actions->addWidget(primaryButton, 1);
    actions->addWidget(backButton);
    root->addLayout(actions);
    connect(backButton, &QPushButton::clicked, this, &QDialog::reject);

    const auto hideField = [form](QWidget *widget) {
        if (widget == nullptr) {
            return;
        }
        if (QWidget *label = form->labelForField(widget)) {
            label->hide();
        }
        widget->hide();
    };
    if (workflow != Workflow::AppointmentBooking) {
        hideField(patientIdInput);
        hideField(doctorIdInput);
        hideField(appointmentDateRow);
        hideField(appointmentDateInput);
        hideField(slotInput);
    }
    if (workflow != Workflow::Consultation) {
        hideField(appointmentIdInput);
        hideField(symptomsInput);
        hideField(diagnosisInput);
        hideField(notesInput);
        hideField(scheduleDateInput);
        hideField(scheduleRow);
        hideField(scheduledAppointmentInput);
        hideField(patientContextLabel);
        prescriptionPanel->hide();
    }
    if (workflow != Workflow::UserManagement) {
        hideField(usernameInput);
        hideField(passwordInput);
        hideField(roleInput);
    }
    if (workflow != Workflow::PatientProfile) {
        hideField(profileNameInput);
        hideField(profileDateOfBirthInput);
        hideField(profileGenderInput);
        hideField(profilePhoneInput);
        hideField(profileAddressInput);
    }
    if (workflow != Workflow::AuditLog) {
        hideField(auditUsernameInput);
        hideField(auditActionInput);
        hideField(auditFromInput);
        hideField(auditToInput);
    }

    connect(apiClient, &ApiClient::workflowOperationSucceeded,
            this, &WorkflowWindow::handleSuccess);
    connect(apiClient, &ApiClient::workflowOperationFailed,
            this, &WorkflowWindow::handleFailure);
    connect(apiClient, &ApiClient::workflowArraySucceeded,
            this, &WorkflowWindow::handleArraySuccess);
    connect(apiClient, &ApiClient::prescriptionAllergyWarning,
            this, [this](const QString &message) {
        if (workflow != Workflow::Consultation) {
            return;
        }
        bool accepted = false;
        const QString reason = QInputDialog::getText(
            this, "Allergy warning",
            message + "\n\nEnter a reason to override this warning:",
            QLineEdit::Normal, QString(), &accepted).trimmed();
        if (!accepted) {
            showStatus("Prescription was not saved; the allergy warning "
                       "was not overridden.");
            return;
        }
        if (reason.isEmpty()) {
            showStatus("An override reason is required. The prescription "
                       "was not saved.");
            return;
        }
        setBusy(true);
        showStatus("Submitting the prescription with the override reason...");
        apiClient->addPrescription(
            visitIdInput->text(), medicineInput->text().trimmed(),
            dosageInput->text().trimmed(), frequencyInput->text().trimmed(),
            QString::number(durationInput->value()), reason);
    });
    connect(apiClient, &ApiClient::workflowDownloadSucceeded,
            this, [this](const QString &operation, const QByteArray &data,
                         const QString &contentType) {
        if ((operation != "report" && operation != "receipt") ||
            data.isEmpty()) {
            if (data.isEmpty()) showStatus("The server returned an empty file.");
            return;
        }
        setBusy(false);
        QString suggestedName;
        if (operation == "receipt") {
            suggestedName = QString("receipt-%1.html")
                                .arg(receiptPaymentIdInput->text());
        } else {
            suggestedName = QString("daily-report-%1.%2")
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"),
                     reportFormatInput->currentText().toLower());
        }
        const QString filter = operation == "receipt"
            ? "HTML files (*.html)" : "Report files (*." +
                reportFormatInput->currentText().toLower() + ")";
        const QString fileName = QFileDialog::getSaveFileName(
            this, "Save " + operation, suggestedName, filter);
        if (fileName.isEmpty()) {
            showStatus("Download cancelled; no file was written.");
            return;
        }
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
            const QString error = file.errorString();
            file.close();
            showStatus("Could not save the downloaded file: " + error);
            return;
        }
        file.close();
        showStatus(QString("%1 saved (%2).")
                       .arg(fileName, contentType), true);
    });
}

void WorkflowWindow::loadFreeSlots()
{
    bool validDoctorId = false;
    const qlonglong doctorId = doctorIdInput->text().toLongLong(&validDoctorId);
    if (!validDoctorId || doctorId <= 0) {
        showStatus("Enter a valid positive doctor ID.");
        return;
    }
    slotInput->clear();
    slotInput->setEnabled(false);
    showStatus("Loading available times...");
    apiClient->getFreeSlots(
        QString::number(doctorId),
        appointmentDateInput->date().toString("yyyy-MM-dd"));
}

void WorkflowWindow::bookAppointment()
{
    const QString patientId = patientIdInput->text().trimmed();
    const bool validPatientId =
        patientId.size() == 12 && patientId.startsWith("PID-") &&
        std::all_of(patientId.cbegin() + 4, patientId.cend(),
                    [](QChar character) {
            return character >= QLatin1Char('0') &&
                   character <= QLatin1Char('9');
        });
    bool validDoctorId = false;
    const qlonglong doctorId = doctorIdInput->text().toLongLong(&validDoctorId);
    if (!validPatientId || !validDoctorId || doctorId <= 0 ||
        slotInput->currentText().isEmpty()) {
        showStatus("Enter a valid patient ID, doctor ID, and available time.");
        return;
    }
    setBusy(true);
    showStatus("Booking appointment...");
    apiClient->bookAppointment(
        patientId, QString::number(doctorId),
        appointmentDateInput->date().toString("yyyy-MM-dd"),
        slotInput->currentText());
}

void WorkflowWindow::recordVisit()
{
    bool validAppointmentId = false;
    const qlonglong appointmentId =
        appointmentIdInput->text().toLongLong(&validAppointmentId);
    const QString symptoms = symptomsInput->toPlainText().trimmed();
    const QString diagnosis = diagnosisInput->text().trimmed();
    const QString notes = notesInput->toPlainText().trimmed();
    if (!validAppointmentId || appointmentId <= 0 || symptoms.isEmpty() ||
        diagnosis.isEmpty() || symptoms.size() > 4000 ||
        diagnosis.size() > 4000 || notes.size() > 8000) {
        showStatus("Provide a valid appointment ID, symptoms, and diagnosis "
                   "(within the field length limits).");
        return;
    }
    setBusy(true);
    showStatus("Saving consultation...");
    apiClient->recordVisit(QString::number(appointmentId), symptoms,
                           diagnosis, notes);
}

void WorkflowWindow::addPrescription()
{
    if (visitIdInput->text().isEmpty() ||
        medicineInput->text().trimmed().isEmpty() ||
        dosageInput->text().trimmed().isEmpty() ||
        frequencyInput->text().trimmed().isEmpty() ||
        medicineInput->text().trimmed().size() > 200 ||
        dosageInput->text().trimmed().size() > 120 ||
        frequencyInput->text().trimmed().size() > 120) {
        showStatus("Complete the prescription fields within their length limits.");
        return;
    }
    setBusy(true);
    showStatus("Saving prescription...");
    apiClient->addPrescription(
        visitIdInput->text(), medicineInput->text().trimmed(),
        dosageInput->text().trimmed(), frequencyInput->text().trimmed(),
        QString::number(durationInput->value()));
}

void WorkflowWindow::createUser()
{
    const QString password = passwordInput->text();
    bool hasUpper = false;
    bool hasLower = false;
    bool hasDigit = false;
    bool hasSpecial = false;
    for (const QChar character : password) {
        hasUpper |= character.isUpper();
        hasLower |= character.isLower();
        hasDigit |= character.isDigit();
        hasSpecial |= !character.isLetterOrNumber() &&
                      !character.isSpace();
    }
    if (usernameInput->text().trimmed().isEmpty() ||
        usernameInput->text().trimmed().size() > 100 ||
        password.size() < 8 || !hasUpper || !hasLower || !hasDigit ||
        !hasSpecial) {
        showStatus("Enter a username (up to 100 characters) and a strong "
                   "password with uppercase, lowercase, a digit, and a "
                   "special character.");
        return;
    }
    setBusy(true);
    showStatus("Creating account...");
    apiClient->createUser(usernameInput->text().trimmed(),
                          passwordInput->text(), roleInput->currentText());
}

void WorkflowWindow::deactivateUser()
{
    bool valid = false;
    const qlonglong userId = targetUserIdInput->text().trimmed()
                                 .toLongLong(&valid);
    if (!valid || userId <= 0) {
        showStatus("Enter a positive target user ID.");
        return;
    }
    if (QMessageBox::question(
            this, "Confirm account deactivation",
            QString("Deactivate user %1? They will no longer be able to sign in.")
                .arg(userId)) != QMessageBox::Yes) {
        showStatus("Account deactivation cancelled.");
        return;
    }
    showStatus("Deactivating account...");
    apiClient->deactivateUser(QString::number(userId));
}

void WorkflowWindow::createBill()
{
    bool validVisitId = false;
    const qlonglong visitId = billVisitIdInput->text().trimmed()
                                  .toLongLong(&validVisitId);
    const QStringList lines = billItemsInput->toPlainText()
                                  .split('\n', Qt::SkipEmptyParts);
    if (!validVisitId || visitId <= 0 || lines.isEmpty() || lines.size() > 100) {
        showStatus("Enter a valid visit ID and between 1 and 100 bill items.");
        return;
    }
    QJsonArray items;
    for (const QString &line : lines) {
        const QStringList fields = line.split('|');
        if (fields.size() != 2 || fields[0].trimmed().isEmpty() ||
            fields[1].trimmed().isEmpty()) {
            showStatus("Each bill item must be entered as description | amount.");
            return;
        }
        items.append(QJsonObject{
            {"description", fields[0].trimmed()},
            {"amount", fields[1].trimmed()}
        });
    }
    showStatus("Creating itemized bill...");
    apiClient->createBill(QString::number(visitId), items);
}

void WorkflowWindow::recordPayment()
{
    bool validBillId = false;
    const qlonglong billId = billIdInput->text().trimmed()
                                 .toLongLong(&validBillId);
    const QString amount = paymentAmountInput->text().trimmed();
    if (!validBillId || billId <= 0 || amount.isEmpty()) {
        showStatus("Enter a positive bill ID and payment amount.");
        return;
    }
    showStatus("Recording payment...");
    apiClient->recordPayment(QString::number(billId), amount,
                             paymentModeInput->currentText());
}

void WorkflowWindow::loadReceipt()
{
    bool validPaymentId = false;
    const qlonglong paymentId = receiptPaymentIdInput->text().trimmed()
                                    .toLongLong(&validPaymentId);
    if (!validPaymentId || paymentId <= 0) {
        showStatus("Enter a positive payment ID.");
        return;
    }
    showStatus("Retrieving receipt...");
    apiClient->getReceipt(QString::number(paymentId));
}

void WorkflowWindow::generateReport()
{
    const QDate start = reportStartDateInput->date();
    const QDate end = reportEndDateInput->date();
    if (start > end) {
        showStatus("The report start date must not follow its end date.");
        return;
    }
    setBusy(true);
    showStatus("Generating report...");
    apiClient->getReport(start.toString("yyyy-MM-dd"),
                         end.toString("yyyy-MM-dd"),
                         reportFormatInput->currentText().toLower());
}

void WorkflowWindow::loadAuditLog()
{
    const QString from = auditFromInput->text().trimmed();
    const QString to = auditToInput->text().trimmed();
    const auto validDate = [](const QString &value) {
        if (value.isEmpty()) return true;
        const QDate parsed = QDate::fromString(value, "yyyy-MM-dd");
        return parsed.isValid() && parsed.toString("yyyy-MM-dd") == value;
    };
    if (!validDate(from) || !validDate(to) ||
        (!from.isEmpty() && !to.isEmpty() && from > to)) {
        showStatus("Enter valid ISO dates and ensure From does not follow To.");
        return;
    }
    resultOutput->clear();
    showStatus("Loading audit entries...");
    apiClient->getAuditLog(
        auditUsernameInput->text().trimmed(),
        auditActionInput->currentData().toString(), from, to);
}

void WorkflowWindow::loadMySchedule()
{
    scheduledAppointmentInput->clear();
    patientContextLabel->setText("Loading your appointments...");
    apiClient->getMySchedule(scheduleDateInput->date().toString("yyyy-MM-dd"));
}

void WorkflowWindow::selectScheduledAppointment(int index)
{
    if (index < 0) return;
    const QString appointmentId =
        scheduledAppointmentInput->itemData(index).toString();
    const QString patientId =
        scheduledAppointmentInput->itemData(index, Qt::UserRole + 1).toString();
    if (appointmentId.isEmpty() || patientId.isEmpty()) return;
    appointmentIdInput->setText(appointmentId);
    patientContextLabel->setText("Loading patient details...");
    apiClient->getPatientDetails(patientId);
}

void WorkflowWindow::handleSuccess(const QString &operation,
                                   const QJsonObject &response)
{
    if (operation == "freeSlots") {
        const QJsonArray availableSlots =
            response.value("slots").toArray();
        for (const QJsonValue &value : availableSlots) {
            slotInput->addItem(value.toString());
        }
        slotInput->setEnabled(!availableSlots.isEmpty());
        if (availableSlots.isEmpty()) {
            showStatus(
                "No available times. Choose a working day for this doctor.");
        } else {
            showStatus(
                QString("%1 available time(s).").arg(availableSlots.size()),
                true);
        }
        return;
    }
    if (operation == "myHistory" || operation == "myBills" ||
        operation == "patientDetails") {
        const QJsonObject display =
            operation == "patientDetails"
                ? response.value("patient").toObject()
                : response;
        if (operation == "patientDetails") {
            const QString name = display.value("name").toString();
            const QString allergies = display.value("allergies").toString();
            patientContextLabel->setText(
                QString("Patient: %1 (%2)\nAllergies: %3")
                    .arg(name.isEmpty() ? "Name unavailable" : name,
                         display.value("patientId").toString(),
                         allergies.isEmpty()
                             ? "No allergy information recorded"
                             : allergies));
        } else {
            resultOutput->setPlainText(
                QJsonDocument(display).toJson(QJsonDocument::Indented));
            showStatus("Records loaded.", true);
        }
        return;
    }
    setBusy(false);
    if (operation == "bookAppointment") {
        const QJsonObject appointment =
            response.value("appointment").toObject();
        showStatus(
            QString("Appointment %1 confirmed for %2 at %3.")
                .arg(appointment.value("appointmentId").toVariant().toString(),
                     appointment.value("slotStart").toString(),
                     appointment.value("status").toString()),
            true);
    } else if (operation == "recordVisit") {
        const QString visitId =
            response.value("visitId").toVariant().toString();
        visitIdInput->setText(visitId);
        prescriptionPanel->setVisible(!visitId.isEmpty());
        primaryButton->setEnabled(false);
        showStatus(QString("Consultation saved. Visit ID: %1. You can now "
                           "add a prescription.").arg(visitId), true);
        if (!visitId.isEmpty()) {
            resize(620, 680);
            medicineInput->setFocus();
        }
    } else if (operation == "addPrescription") {
        showStatus("Prescription saved successfully.", true);
        QMessageBox::information(
            this, "Prescription saved",
            "The prescription was saved successfully.");
        medicineInput->clear();
        dosageInput->clear();
        frequencyInput->clear();
    } else if (operation == "createUser") {
        const QString userId =
            response.value("userId").toVariant().toString();
        QString details = QString("Account created. User ID: %1; role: %2.")
                              .arg(userId, response.value("role").toString());
        const QString doctorId =
            response.value("doctorId").toVariant().toString();
        if (!doctorId.isEmpty()) {
            details += QString("; doctor ID: %1 (weekday schedule 09:00-17:00).")
                           .arg(doctorId);
        }
        showStatus(details, true);
        usernameInput->clear();
        passwordInput->clear();
        if (!userId.isEmpty()) targetUserIdInput->setText(userId);
    } else if (operation == "deactivateUser") {
        showStatus(response.value("message").toString(
                       "Account deactivated."),
                   true);
        targetUserIdInput->clear();
        showStatus("Account deactivated. Refresh the user list to update it.",
                   true);
    } else if (operation == "changeUserRole") {
        showStatus(response.value("message").toString(
                       "User role updated. Refresh the list to verify."),
                   true);
        apiClient->getUsers();
    } else if (operation == "createBill") {
        const QJsonObject bill = response.value("bill").toObject();
        const QString billId = bill.value("billId").toVariant().toString();
        if (!billId.isEmpty()) billIdInput->setText(billId);
        resultOutput->setPlainText(
            QJsonDocument(bill).toJson(QJsonDocument::Indented));
        showStatus(QString("Bill %1 created. Total: %2.")
                       .arg(billId, bill.value("total").toString()),
                   true);
    } else if (operation == "recordPayment") {
        const QJsonObject payment = response.value("payment").toObject();
        const QString paymentId =
            payment.value("paymentId").toVariant().toString();
        if (!paymentId.isEmpty()) receiptPaymentIdInput->setText(paymentId);
        resultOutput->setPlainText(
            QJsonDocument(payment).toJson(QJsonDocument::Indented));
        showStatus(QString("Payment recorded. Outstanding balance: %1.")
                       .arg(payment.value("balance").toString()),
                   true);
    } else if (operation == "createMyProfile") {
        const QJsonObject patient = response.value("patient").toObject();
        showStatus(
            QString("Profile created. Your patient ID is %1.")
                .arg(patient.value("patientId").toString()),
            true);
        primaryButton->setEnabled(false);
    } else if (operation == "mySchedule") {
        showStatus("Schedule loaded.", true);
    }
}

void WorkflowWindow::handleArraySuccess(const QString &operation,
                                        const QJsonArray &response)
{
    if (operation == "userList") {
        {
            const QSignalBlocker blocker(managedUsersInput);
            managedUsersInput->clear();
            for (const QJsonValue &value : response) {
                const QJsonObject user = value.toObject();
                const QString userId = user.value("userId").toString();
                const QString username = user.value("username").toString();
                const QString role = user.value("role").toString();
                if (userId.isEmpty() || username.isEmpty()) continue;
                const QString doctorId =
                    user.value("doctorId").isString()
                        ? QString("; doctor ID %1")
                              .arg(user.value("doctorId").toString())
                        : QString();
                managedUsersInput->addItem(
                    QString("%1 — %2 — %3%4")
                        .arg(userId, username, role, doctorId)
                        .append(user.value("active").toBool()
                                    ? QString()
                                    : " — inactive"),
                    userId);
                managedUsersInput->setItemData(
                    managedUsersInput->count() - 1, role, Qt::UserRole + 1);
            }
        }
        if (managedUsersInput->count() > 0) {
            managedUsersInput->setCurrentIndex(0);
        } else {
            targetUserIdInput->clear();
        }
        showStatus(QString("%1 account(s) loaded.")
                       .arg(managedUsersInput->count()),
                   true);
        return;
    }
    if (operation == "auditLog") {
        resultOutput->setPlainText(
            QJsonDocument(response).toJson(QJsonDocument::Indented));
        showStatus("Audit entries loaded.", true);
        return;
    }
    if (operation != "mySchedule") return;
    scheduledAppointmentInput->clear();
    for (const QJsonValue &value : response) {
        const QJsonObject appointment = value.toObject();
        const QString id =
            appointment.value("appointmentId").toVariant().toString();
        const QString patientId = appointment.value("patientId").toString();
        const QString slot = appointment.value("slotStart").toString();
        if (id.isEmpty() || patientId.isEmpty()) continue;
        scheduledAppointmentInput->addItem(
            QString("%1 — patient %2 — %3")
                .arg(id, patientId, slot),
            id);
        scheduledAppointmentInput->setItemData(
            scheduledAppointmentInput->count() - 1, patientId,
            Qt::UserRole + 1);
    }
    if (scheduledAppointmentInput->count() == 0) {
        patientContextLabel->setText(
            "No appointments were found for this date.");
        showStatus("No appointments found for the selected date.");
        return;
    }
    showStatus(QString("%1 appointment(s) loaded.")
                   .arg(scheduledAppointmentInput->count()),
               true);
    selectScheduledAppointment(scheduledAppointmentInput->currentIndex());
}

void WorkflowWindow::handleFailure(const QString &operation,
                                   const QString &message)
{
    setBusy(false);
    if (operation == "freeSlots") {
        slotInput->clear();
        slotInput->setEnabled(false);
    }
    showStatus(message);
}

void WorkflowWindow::setBusy(bool busy)
{
    primaryButton->setEnabled(!busy);
}

void WorkflowWindow::showStatus(const QString &message, bool success)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(success
        ? "color: #14532d;" : "color: #a61b1b;");
}
