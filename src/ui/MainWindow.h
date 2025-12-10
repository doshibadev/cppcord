#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QScrollBar>
#include "network/DiscordClient.h"
#include "models/Snowflake.h"
#include "models/Message.h"
#include "utils/TokenStorage.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    DiscordClient *m_client;
    TokenStorage m_tokenStorage;

    // UI Components
    QWidget *m_centralWidget;
    QListWidget *m_guildList;
    QListWidget *m_channelList;
    QTextEdit *m_messageLog;
    QLineEdit *m_messageInput;
    QLabel *m_currentTitle;
    QPushButton *m_scrollToBottomBtn;

    // State
    Snowflake m_selectedGuildId;
    Snowflake m_selectedChannelId;
    QList<Message> m_currentMessages; // Messages for current channel
    bool m_isLoadingMessages;
    bool m_hasMoreMessages;

    void setupUI();
    void connectSignals();
    void showLoginDialog();
    void tryAutoLogin();
    void handleTokenInvalidated();

    void updateGuildList();
    void updateChannelList();
    void sortGuildList();
    void updateMessageInputPermissions();
    void onGuildSelected(QListWidgetItem *item);
    void onChannelSelected(QListWidgetItem *item);
    void onScrollValueChanged(int value);
    void loadMoreMessages();
    void scrollToBottom();
    void addMessage(const Message &message);
    void displayMessages();
};
