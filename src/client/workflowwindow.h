#pragma once

#include <QDialog>
#include <QJsonObject>

class ApiClient;
class QComboBox;
class QDateEdit;
class QJsonArray;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
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
        PatientProfile,
        PatientRecords,
        Billing,
        Reports,
        AuditLog
    };

    WorkflowWindow(ApiClient *apiClient, Workflow workflow,
                   QWidget *parent = nullptr);

private slots:
    void loadFreeSlots();
    void bookAppointment();
    void recordVisit();
    void addPrescription();
    void createUser();
    void deactivateUser();
    void createBill();
    void recordPayment();
    void loadReceipt();
    void generateReport();
    void loadAuditLog();
    void loadMySchedule();
    void selectScheduledAppointment(int index);
    void handleSuccess(const QString &operation,
                       const QJsonObject &response);
    void handleArraySuccess(const QString &operation,
                            const QJsonArray &response);
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
    QDateEdit *scheduleDateInput;
    QWidget *scheduleRow;
    QComboBox *scheduledAppointmentInput;
    QLabel *patientContextLabel;
    QLineEdit *billVisitIdInput;
    QTextEdit *billItemsInput;
    QLineEdit *billIdInput;
    QLineEdit *paymentAmountInput;
    QComboBox *paymentModeInput;
    QLineEdit *receiptPaymentIdInput;
    QDateEdit *reportStartDateInput;
    QDateEdit *reportEndDateInput;
    QComboBox *reportFormatInput;
    QLineEdit *auditUsernameInput;
    QLineEdit *auditFromInput;
    QLineEdit *auditToInput;
    QComboBox *auditActionInput;
    QTextEdit *resultOutput;
    QLineEdit *targetUserIdInput;
    QComboBox *managedUsersInput;
    QComboBox *managedRoleInput;
};
