#pragma once

#include <QDialog>
#include <QJsonObject>

class ApiClient;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTextEdit;
class QWidget;

class WorkflowWindow : public QDialog
{
    Q_OBJECT

public:
    enum class Workflow {
        AppointmentBooking,
        Consultation,
        UserManagement,
        PatientProfile
    };

    WorkflowWindow(ApiClient *apiClient, Workflow workflow,
                   QWidget *parent = nullptr);

private slots:
    void loadFreeSlots();
    void bookAppointment();
    void recordVisit();
    void addPrescription();
    void createUser();
    void handleSuccess(const QString &operation,
                       const QJsonObject &response);
    void handleFailure(const QString &operation,
                       const QString &message);

private:
    void setBusy(bool busy);
    void showStatus(const QString &message, bool success = false);

    ApiClient *apiClient;
    Workflow workflow;
    QLabel *statusLabel;
    QPushButton *primaryButton;
    QLineEdit *patientIdInput;
    QLineEdit *doctorIdInput;
    QDateEdit *appointmentDateInput;
    QComboBox *slotInput;
    QLineEdit *appointmentIdInput;
    QTextEdit *symptomsInput;
    QLineEdit *diagnosisInput;
    QTextEdit *notesInput;
    QWidget *prescriptionPanel;
    QLineEdit *visitIdInput;
    QLineEdit *medicineInput;
    QLineEdit *dosageInput;
    QLineEdit *frequencyInput;
    QSpinBox *durationInput;
    QLineEdit *usernameInput;
    QLineEdit *passwordInput;
    QComboBox *roleInput;
    QLineEdit *profileNameInput;
    QLineEdit *profileDateOfBirthInput;
    QComboBox *profileGenderInput;
    QLineEdit *profilePhoneInput;
    QLineEdit *profileAddressInput;
};
