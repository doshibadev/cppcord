#include "MainWindow.h"
#include "LoginDialog.h"
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_client(new DiscordClient(this))
{
    setWindowTitle("CPPCord - Discord Client");
    resize(800, 600);

    setupUI();
    connectSignals();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Status label to show login state
    m_statusLabel = new QLabel("Not logged in", centralWidget);
    QFont font = m_statusLabel->font();
    font.setPointSize(12);
    m_statusLabel->setFont(font);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    // User info label
    m_userInfoLabel = new QLabel("", centralWidget);
    m_userInfoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_userInfoLabel);

    m_loginButton = new QPushButton("Login to Discord", centralWidget);
    layout->addWidget(m_loginButton);

    m_getUserButton = new QPushButton("Get User Info", centralWidget);
    m_getUserButton->setEnabled(false);
    layout->addWidget(m_getUserButton);

    layout->addStretch();

    setCentralWidget(centralWidget);
}

void MainWindow::connectSignals()
{
    connect(m_loginButton, &QPushButton::clicked, this, &MainWindow::showLoginDialog);

    connect(m_getUserButton, &QPushButton::clicked, [this]() {
        qDebug() << "Fetching user info...";
        m_client->getCurrentUser();
    });

    connect(m_client, &DiscordClient::loginSuccess, [this]() {
        qDebug() << "Login successful!";
        m_statusLabel->setText("Logged in successfully!");
        m_statusLabel->setStyleSheet("QLabel { color: green; }");
        m_getUserButton->setEnabled(true);
    });

    connect(m_client, &DiscordClient::userInfoReceived, [this](const User &user) {
        QString info = QString("Username: %1\nUser ID: %2")
                           .arg(user.getTag())
                           .arg(user.id);
        m_userInfoLabel->setText(info);
        qDebug() << "User info received:" << info;
    });

    connect(m_client, &DiscordClient::apiError, [](const QString &error) {
        qDebug() << "API Error:" << error;
        QMessageBox::warning(nullptr, "API Error", error);
    });
}

void MainWindow::showLoginDialog()
{
    LoginDialog *dialog = new LoginDialog(this);

    connect(dialog, &LoginDialog::loginRequested, [this](const QString &email, const QString &password) {
        qDebug() << "Attempting login...";
        m_client->login(email, password);
    });

    connect(dialog, &LoginDialog::mfaSubmitted, [this](const QString &code, const QString &ticket) {
        qDebug() << "Submitting MFA...";
        m_client->submitMFA(code, ticket);
    });

    connect(m_client, &DiscordClient::mfaRequired, dialog, &LoginDialog::showMFAPrompt);

    connect(m_client, &DiscordClient::loginSuccess, dialog, &LoginDialog::accept);

    connect(m_client, &DiscordClient::loginError, [dialog](const QString &error) {
        dialog->showError(error);
        dialog->resetAfterError();
    });

    dialog->exec();
    dialog->deleteLater();
}
