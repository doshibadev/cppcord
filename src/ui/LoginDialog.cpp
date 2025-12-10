#include "LoginDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Login to Discord");
    setModal(true);
    resize(400, 250);

    setupLoginUI();
    setupMFAUI();

    // Hide MFA UI initially
    m_mfaWidget->hide();
}

void LoginDialog::setupLoginUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *titleLabel = new QLabel("Discord Login", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QFormLayout *formLayout = new QFormLayout();

    m_emailInput = new QLineEdit(this);
    m_emailInput->setPlaceholderText("Email");
    formLayout->addRow("Email:", m_emailInput);

    m_passwordInput = new QLineEdit(this);
    m_passwordInput->setPlaceholderText("Password");
    m_passwordInput->setEchoMode(QLineEdit::Password);
    formLayout->addRow("Password:", m_passwordInput);

    mainLayout->addLayout(formLayout);

    m_loginButton = new QPushButton("Login", this);
    mainLayout->addWidget(m_loginButton);

    m_statusLabel = new QLabel("", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("QLabel { color: red; }");
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_passwordInput, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
}

void LoginDialog::setupMFAUI()
{
    m_mfaWidget = new QWidget(this);
    QVBoxLayout *mfaLayout = new QVBoxLayout(m_mfaWidget);

    QLabel *mfaLabel = new QLabel("Enter 2FA Code", m_mfaWidget);
    QFont mfaFont = mfaLabel->font();
    mfaFont.setPointSize(14);
    mfaFont.setBold(true);
    mfaLabel->setFont(mfaFont);
    mfaLabel->setAlignment(Qt::AlignCenter);
    mfaLayout->addWidget(mfaLabel);

    m_mfaInput = new QLineEdit(m_mfaWidget);
    m_mfaInput->setPlaceholderText("6-digit code");
    m_mfaInput->setMaxLength(6);
    mfaLayout->addWidget(m_mfaInput);

    m_mfaButton = new QPushButton("Submit", m_mfaWidget);
    mfaLayout->addWidget(m_mfaButton);

    mfaLayout->addStretch();

    layout()->addWidget(m_mfaWidget);

    connect(m_mfaButton, &QPushButton::clicked, this, &LoginDialog::onMFASubmitted);
    connect(m_mfaInput, &QLineEdit::returnPressed, this, &LoginDialog::onMFASubmitted);
}

void LoginDialog::onLoginClicked()
{
    QString email = m_emailInput->text().trimmed();
    QString password = m_passwordInput->text();

    if (email.isEmpty() || password.isEmpty())
    {
        m_statusLabel->setText("Please fill in all fields");
        return;
    }

    m_statusLabel->setText("Logging in...");
    m_loginButton->setEnabled(false);

    emit loginRequested(email, password);
}

void LoginDialog::onMFASubmitted()
{
    QString code = m_mfaInput->text().trimmed();

    if (code.length() != 6)
    {
        m_statusLabel->setText("Invalid code format");
        return;
    }

    m_statusLabel->setText("Verifying...");
    m_mfaButton->setEnabled(false);

    emit mfaSubmitted(code, m_mfaTicket);
}

void LoginDialog::showMFAPrompt(const QString &ticket)
{
    m_mfaTicket = ticket;

    // Hide login form
    m_emailInput->hide();
    m_passwordInput->hide();
    m_loginButton->hide();

    // Show MFA form
    m_mfaWidget->show();
    m_mfaInput->setFocus();

    m_statusLabel->setText("2FA required");
    m_statusLabel->setStyleSheet("QLabel { color: blue; }");
}

QString LoginDialog::getEmail() const
{
    return m_emailInput->text();
}

QString LoginDialog::getPassword() const
{
    return m_passwordInput->text();
}

QString LoginDialog::getMFACode() const
{
    return m_mfaInput->text();
}

QString LoginDialog::getMFATicket() const
{
    return m_mfaTicket;
}

void LoginDialog::showError(const QString &error)
{
    m_statusLabel->setText(error);
    m_statusLabel->setStyleSheet("QLabel { color: red; }");

    // Re-enable buttons so user can retry
    m_loginButton->setEnabled(true);
    m_mfaButton->setEnabled(true);
}

void LoginDialog::resetAfterError()
{
    // If we're in MFA mode and there's an error, let user retry MFA
    if (m_mfaWidget->isVisible())
    {
        m_mfaInput->clear();
        m_mfaInput->setFocus();
        m_mfaButton->setEnabled(true);
    }
    else
    {
        // In login mode, just re-enable the button
        m_loginButton->setEnabled(true);
        m_passwordInput->setFocus();
    }
}
