#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include "../network/DiscordClient.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    DiscordClient *m_client;
    QLabel *m_statusLabel;
    QLabel *m_userInfoLabel;
    QPushButton *m_loginButton;
    QPushButton *m_getUserButton;

    void setupUI();
    void connectSignals();
    void showLoginDialog();
};
