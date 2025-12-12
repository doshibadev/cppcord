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
#include "audio/AudioManager.h"
#include "models/Snowflake.h"
#include "models/Message.h"
#include "utils/TokenStorage.h"
#include "utils/AvatarCache.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    DiscordClient *m_client;
    TokenStorage m_tokenStorage;
    AudioManager *m_audioManager;
    AvatarCache *m_avatarCache;

    // UI Components
    QWidget *m_centralWidget;
    QListWidget *m_guildList;
    QListWidget *m_channelList;
    QTextEdit *m_messageLog;
    QLineEdit *m_messageInput;
    QLabel *m_currentTitle;
    QPushButton *m_scrollToBottomBtn;
    QPushButton *m_logoutBtn;
    QPushButton *m_settingsBtn;
    QPushButton *m_muteBtn;
    QPushButton *m_deafenBtn;
    QPushButton *m_callBtn; // Call/Leave Call button for DMs
    QLabel *m_usernameLabel;

    // State
    Snowflake m_selectedGuildId;
    Snowflake m_selectedChannelId;
    QList<Message> m_currentMessages; // Messages for current channel
    bool m_isLoadingMessages;
    bool m_hasMoreMessages;

    // Voice state
    bool m_isInVoice;
    bool m_isMuted;
    bool m_isDeafened;
    Snowflake m_currentVoiceChannelId;

    // Call state (for DMs)
    bool m_isInCall;
    bool m_isRinging;
    QTimer *m_ringingTimer;
    QTimer *m_noAnswerTimer;
    Snowflake m_currentCallChannelId;

    void setupUI();
    void connectSignals();
    void showLoginDialog();
    void tryAutoLogin();
    void handleTokenInvalidated();

    void updateGuildList();
    void updateChannelList();
    void sortGuildList();
    void updateMessageInputPermissions();
    QString formatMessageHtml(const Message &msg, bool grouped = false);
    void onGuildSelected(QListWidgetItem *item);
    void onChannelSelected(QListWidgetItem *item);
    void onChannelDoubleClicked(QListWidgetItem *item);
    void onScrollValueChanged(int value);
    void loadMoreMessages();
    void scrollToBottom();
    void addMessage(const Message &message);
    void displayMessages();

    // Voice methods
    void onVoiceReady();
    void updateVoiceUI();
    void onMuteToggled();
    void onDeafenToggled();

    // Call methods (DM voice)
    void onCallButtonClicked();
    void onCallCreated(Snowflake channelId, const QList<Snowflake> &ringing);
    void onCallUpdated(Snowflake channelId, const QList<Snowflake> &ringing);
    void onCallDeleted(Snowflake channelId);
    void onRingingTimeout();
    void onNoAnswerTimeout();
    void updateCallButton();

    // Logout
    void onLogoutClicked();
    void onSettingsClicked();
};
