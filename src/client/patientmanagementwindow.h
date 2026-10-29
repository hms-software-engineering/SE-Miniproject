#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QJsonObject>

class ApiClient;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class PatientManagementWindow : public QDialog
{
    Q_OBJECT

public:
    explicit PatientManagementWindow(ApiClient *apiClient,
                                     bool canManagePatients,
                                     QWidget *parent = nullptr);

private slots:
    void searchPatients();
    void submitPatient();
    void clearForm();
    void showSearchResult(int row, int column);
    void handlePatientSuccess(const QString &operation,
                              const QJsonObject &response);
    void handlePatientError(const QString &operation,
                            const QString &message);
    void confirmDuplicateRegistration(const QString &message);

private:
    QJsonObject patientForm() const;
    ApiClient *apiClient;
    QLineEdit *searchInput;
    QTableWidget *results;
    QLabel *patientIdLabel;
    QLineEdit *nameInput;
    QLineEdit *dateOfBirthInput;
    QComboBox *genderInput;
    QLineEdit *phoneInput;
    QLineEdit *addressInput;
    QLabel *statusLabel;
    QPushButton *submitButton;
    QPushButton *newPatientButton;
    QPushButton *historyButton;
    QElapsedTimer registrationTimer;
};
