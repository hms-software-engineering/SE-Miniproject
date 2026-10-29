#include "workflowwindow.h"

#include "apiclient.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
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
      profileAddressInput(new QLineEdit(this))
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
        symptomsInput->setPlaceholderText("Symptoms");
        symptomsInput->setMaximumHeight(90);
        diagnosisInput->setPlaceholderText("Diagnosis");
        notesInput->setPlaceholderText("Notes (optional)");
        notesInput->setMaximumHeight(90);
        form->addRow("Appointment ID *", appointmentIdInput);
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
        connect(addPrescriptionButton, &QPushButton::clicked,
                this, &WorkflowWindow::addPrescription);
        break;
    }
    case Workflow::UserManagement: {
        setWindowTitle("Create user account");
        helpText =
            "1. Enter a username.\n"
            "2. Enter a password with at least 8 characters, including an "
            "uppercase letter, lowercase letter, digit, and special "
            "character.\n"
            "3. Choose the account role and select Create account.\n\n"
            "For a Doctor account, note the doctor ID shown in the "
            "confirmation.";
        resize(520, 360);
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

    connect(apiClient, &ApiClient::workflowOperationSucceeded,
            this, &WorkflowWindow::handleSuccess);
    connect(apiClient, &ApiClient::workflowOperationFailed,
            this, &WorkflowWindow::handleFailure);
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
    } else if (operation == "createMyProfile") {
        const QJsonObject patient = response.value("patient").toObject();
        showStatus(
            QString("Profile created. Your patient ID is %1.")
                .arg(patient.value("patientId").toString()),
            true);
        primaryButton->setEnabled(false);
    }
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
