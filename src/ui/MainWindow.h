#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QTextEdit>
#include "network/DiscordClient.h"
#include "models/Snowflake.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    DiscordClient *m_client;

    // UI Components
    QWidget *m_centralWidget;
    QListWidget *m_guildList;
    QListWidget *m_channelList;
    QTextEdit *m_messageLog;
    QLineEdit *m_messageInput; // For future sending
    QLabel *m_currentTitle;    // Shows Guild/Channel name

    // State
    Snowflake m_selectedGuildId; // 0 for Home/DM
    Snowflake m_selectedChannelId;

    void setupUI();
    void connectSignals();
    void showLoginDialog();

    void updateGuildList();
    void updateChannelList();
    void onGuildSelected(QListWidgetItem *item);
    void onChannelSelected(QListWidgetItem *item);
};
