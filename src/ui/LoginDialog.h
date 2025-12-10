#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QString>

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString getEmail() const;
    QString getPassword() const;

    void showMFAPrompt(const QString &ticket);
    void showError(const QString &error);
    void resetAfterError();
    QString getMFACode() const;
    QString getMFATicket() const;

signals:
    void loginRequested(const QString &email, const QString &password);
    void mfaSubmitted(const QString &code, const QString &ticket);

private:
    // Login widgets
    QLineEdit *m_emailInput;
    QLineEdit *m_passwordInput;
    QPushButton *m_loginButton;
    QLabel *m_statusLabel;

    // MFA widgets
    QWidget *m_mfaWidget;
    QLineEdit *m_mfaInput;
    QPushButton *m_mfaButton;
    QString m_mfaTicket;

    void setupLoginUI();
    void setupMFAUI();
    void onLoginClicked();
    void onMFASubmitted();
};
