#include "MainWindow.h"
#include "LoginDialog.h"
#include "SettingsDialog.h"
#include "network/VoiceClient.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QMessageBox>
#include <QListWidget>
#include <QPainter>
#include <QBrush>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QBuffer>
#include <QLocale>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_client(new DiscordClient(this)),
      m_audioManager(new AudioManager(this)),
      m_selectedGuildId(0),
      m_selectedChannelId(0),
      m_isLoadingMessages(false),
      m_hasMoreMessages(true),
      m_isInVoice(false),
      m_isMuted(false),
      m_isDeafened(false),
      m_currentVoiceChannelId(0),
      m_isInCall(false),
      m_isRinging(false),
      m_currentCallChannelId(0)
{
    setWindowTitle("CPPCord - Discord Client");
    resize(1200, 800);

    // Initialize timers for calling
    m_ringingTimer = new QTimer(this);
    m_ringingTimer->setSingleShot(true);
    m_ringingTimer->setInterval(10000); // 10 seconds
    connect(m_ringingTimer, &QTimer::timeout, this, &MainWindow::onRingingTimeout);

    m_noAnswerTimer = new QTimer(this);
    m_noAnswerTimer->setSingleShot(true);
    m_noAnswerTimer->setInterval(300000); // 5 minutes
    connect(m_noAnswerTimer, &QTimer::timeout, this, &MainWindow::onNoAnswerTimeout);

    // Initialize audio manager
    if (!m_audioManager->initialize())
    {
        qWarning() << "Failed to initialize audio manager";
    }

    setupUI();
    connectSignals();

    // Show login dialog immediately if not logged in
    // Or let user click a button. For now, let's keep the button approach but maybe auto-show?
    // Let's stick to manual login trigger for now, but UI should look "empty"
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 1. Guild List (Left)
    m_guildList = new QListWidget(m_centralWidget);
    m_guildList->setFixedWidth(72); // Discord style narrow width
    m_guildList->setIconSize(QSize(40, 40));
    m_guildList->setStyleSheet(
        "QListWidget { background-color: #202225; border: none; padding: 0px; }"
        "QListWidget::item { padding: 0px; margin: 8px 12px; border-radius: 20px; width: 48px; height: 48px; }"
        "QListWidget::item:selected { background-color: #5865F2; border-radius: 16px; }"
        "QListWidget::item:hover { background-color: #5865F2; border-radius: 16px; }");

    // Add "Home" button
    QListWidgetItem *homeItem = new QListWidgetItem("DM");
    homeItem->setData(Qt::UserRole, 0); // ID 0 for Home
    homeItem->setTextAlignment(Qt::AlignCenter);
    m_guildList->addItem(homeItem);

    mainLayout->addWidget(m_guildList);

    // 2. Channel/DM List (Middle)
    QWidget *channelPanel = new QWidget(m_centralWidget);
    channelPanel->setFixedWidth(240);
    QVBoxLayout *channelLayout = new QVBoxLayout(channelPanel);

    m_currentTitle = new QLabel("Direct Messages", channelPanel);
    QFont titleFont = m_currentTitle->font();
    titleFont.setBold(true);
    m_currentTitle->setFont(titleFont);
    channelLayout->addWidget(m_currentTitle);

    m_channelList = new QListWidget(channelPanel);
    channelLayout->addWidget(m_channelList);

    // User info area at bottom of channel list
    QWidget *userInfoWidget = new QWidget(channelPanel);
    userInfoWidget->setFixedHeight(60);
    userInfoWidget->setStyleSheet("QWidget { background-color: #292B2F; border-top: 1px solid #202225; }");
    QHBoxLayout *userInfoLayout = new QHBoxLayout(userInfoWidget);
    userInfoLayout->setContentsMargins(8, 8, 8, 8);

    m_usernameLabel = new QLabel("Not logged in", userInfoWidget);
    m_usernameLabel->setStyleSheet("QLabel { color: #FFFFFF; font-weight: bold; }");
    userInfoLayout->addWidget(m_usernameLabel);
    userInfoLayout->addStretch();

    // Voice control buttons
    m_muteBtn = new QPushButton("🎤", userInfoWidget);
    m_muteBtn->setCheckable(true);
    m_muteBtn->setStyleSheet(
        "QPushButton { background-color: #4E5058; color: white; border-radius: 4px; padding: 6px; font-size: 16px; }"
        "QPushButton:hover { background-color: #5D6269; }"
        "QPushButton:checked { background-color: #ED4245; }");
    m_muteBtn->setFixedSize(32, 32);
    m_muteBtn->setToolTip("Mute");
    m_muteBtn->setEnabled(false);
    userInfoLayout->addWidget(m_muteBtn);

    m_deafenBtn = new QPushButton("🔊", userInfoWidget);
    m_deafenBtn->setCheckable(true);
    m_deafenBtn->setStyleSheet(
        "QPushButton { background-color: #4E5058; color: white; border-radius: 4px; padding: 6px; font-size: 16px; }"
        "QPushButton:hover { background-color: #5D6269; }"
        "QPushButton:checked { background-color: #ED4245; }");
    m_deafenBtn->setFixedSize(32, 32);
    m_deafenBtn->setToolTip("Deafen");
    m_deafenBtn->setEnabled(false);
    userInfoLayout->addWidget(m_deafenBtn);

    m_settingsBtn = new QPushButton("⚙", userInfoWidget);
    m_settingsBtn->setStyleSheet(
        "QPushButton { background-color: #4E5058; color: white; border-radius: 4px; padding: 6px 12px; font-size: 16px; }"
        "QPushButton:hover { background-color: #5D6269; }");
    m_settingsBtn->setFixedSize(32, 32);
    m_settingsBtn->setToolTip("Settings");
    userInfoLayout->addWidget(m_settingsBtn);

    m_logoutBtn = new QPushButton("Logout", userInfoWidget);
    m_logoutBtn->setStyleSheet(
        "QPushButton { background-color: #ED4245; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #C03537; }");
    m_logoutBtn->setFixedHeight(32);
    userInfoLayout->addWidget(m_logoutBtn);

    channelLayout->addWidget(userInfoWidget);

    mainLayout->addWidget(channelPanel);

    // 3. Chat Area (Right)
    QWidget *chatPanel = new QWidget(m_centralWidget);
    QVBoxLayout *chatLayout = new QVBoxLayout(chatPanel);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);

    // Top bar with call button (for DMs)
    QWidget *topBar = new QWidget(chatPanel);
    topBar->setFixedHeight(48);
    topBar->setStyleSheet("QWidget { background-color: #36393F; border-bottom: 1px solid #202225; }");
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(16, 0, 16, 0);

    topBarLayout->addStretch();

    m_callBtn = new QPushButton("📞 Call", topBar);
    m_callBtn->setStyleSheet(
        "QPushButton { background-color: #3BA55D; color: white; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2D7D46; }");
    m_callBtn->setFixedHeight(32);
    m_callBtn->hide(); // Hidden by default, shown for DM channels
    topBarLayout->addWidget(m_callBtn);

    chatLayout->addWidget(topBar);

    // Message display with scroll to bottom button
    QWidget *messageContainer = new QWidget(chatPanel);
    QVBoxLayout *messageContainerLayout = new QVBoxLayout(messageContainer);
    messageContainerLayout->setContentsMargins(0, 0, 0, 0);
    messageContainerLayout->setSpacing(0);

    m_messageLog = new QTextEdit(messageContainer);
    m_messageLog->setReadOnly(true);
    m_messageLog->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_messageLog->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    messageContainerLayout->addWidget(m_messageLog);

    // Scroll to bottom button (initially hidden)
    m_scrollToBottomBtn = new QPushButton("↓ Jump to present", messageContainer);
    m_scrollToBottomBtn->setStyleSheet(
        "QPushButton { background-color: #5865F2; color: white; border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #4752C4; }");
    m_scrollToBottomBtn->setFixedHeight(36);
    m_scrollToBottomBtn->hide();

    // Position button above message input
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(m_scrollToBottomBtn);
    btnLayout->addStretch();
    btnLayout->setContentsMargins(10, 10, 10, 10);

    messageContainerLayout->addLayout(btnLayout);
    chatLayout->addWidget(messageContainer);

    m_messageInput = new QLineEdit(chatPanel);
    m_messageInput->setPlaceholderText("Message...");
    chatLayout->addWidget(m_messageInput);
    mainLayout->addWidget(chatPanel);

    setCentralWidget(m_centralWidget);

    // Try auto-login after UI is set up
    QTimer::singleShot(100, this, &MainWindow::tryAutoLogin);
}

void MainWindow::connectSignals()
{
    connect(m_guildList, &QListWidget::itemClicked, this, &MainWindow::onGuildSelected);
    connect(m_channelList, &QListWidget::itemClicked, this, &MainWindow::onChannelSelected);
    connect(m_channelList, &QListWidget::itemDoubleClicked, this, &MainWindow::onChannelDoubleClicked);
    connect(m_scrollToBottomBtn, &QPushButton::clicked, this, &MainWindow::scrollToBottom);
    connect(m_logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(m_muteBtn, &QPushButton::clicked, this, &MainWindow::onMuteToggled);
    connect(m_deafenBtn, &QPushButton::clicked, this, &MainWindow::onDeafenToggled);
    connect(m_callBtn, &QPushButton::clicked, this, &MainWindow::onCallButtonClicked);

    // Voice client connections
    connect(m_client->getVoiceClient(), &VoiceClient::ready, this, &MainWindow::onVoiceReady);

    // Call event connections
    connect(m_client, &DiscordClient::callCreated, this, &MainWindow::onCallCreated);
    connect(m_client, &DiscordClient::callUpdated, this, &MainWindow::onCallUpdated);
    connect(m_client, &DiscordClient::callDeleted, this, &MainWindow::onCallDeleted);

    // Audio manager connections - send audio to voice client
    connect(m_audioManager, &AudioManager::opusDataReady, m_client->getVoiceClient(), &VoiceClient::sendAudio);

    // Voice client audio received - play it
    connect(m_client->getVoiceClient(), &VoiceClient::audioDataReceived, m_audioManager, &AudioManager::addOpusData);

    // Scroll detection for loading more messages and showing/hiding scroll button
    connect(m_messageLog->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::onScrollValueChanged);

    connect(m_messageInput, &QLineEdit::returnPressed, [this]()
            {
        QString content = m_messageInput->text();
        if (!content.isEmpty() && m_selectedChannelId != 0) {
            m_client->sendMessage(m_selectedChannelId, content);
            m_messageInput->clear();
        } });

    connect(m_client, &DiscordClient::loginSuccess, [this]()
            {
        // Update username display
        const User *user = m_client->currentUser();
        if (user) {
            m_usernameLabel->setText(user->username);
        }
        // Clear lists or init state
        updateGuildList(); });

    connect(m_client, &DiscordClient::guildCreated, [this](const Guild &guild)
            {
        // Add guild to the list with placeholder icon (2-letter abbreviation)
        QListWidgetItem *item = new QListWidgetItem(guild.name.left(2).toUpper());
        item->setData(Qt::UserRole, QVariant::fromValue(guild.id));
        item->setData(Qt::UserRole + 1, guild.name); // Store full name
        item->setData(Qt::UserRole + 2, guild.joinedAt); // Store join time for sorting
        item->setToolTip(guild.name);
        item->setTextAlignment(Qt::AlignCenter);
        m_guildList->addItem(item);
        qDebug() << "Added guild to UI:" << guild.name << "(ID:" << guild.id << ")";

        // Sort guilds by join time (oldest first, which means newest at bottom)
        sortGuildList(); });

    connect(m_client, &DiscordClient::channelCreated, [this](const Channel &channel)
            {
        if (m_selectedGuildId == 0 && channel.isDm()) {
            updateChannelList();
        } });

    connect(m_client, &DiscordClient::messagesLoaded, [this](Snowflake channelId, const QList<Message> &messages)
            {
        if (channelId != m_selectedChannelId) return;

        m_isLoadingMessages = false;

        // Add messages to our list (newer messages at the end)
        for (const Message &msg : messages) {
            // Check if message already exists
            bool exists = false;
            for (const Message &existing : m_currentMessages) {
                if (existing.id == msg.id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                m_currentMessages.prepend(msg); // Add to beginning (older messages)
            }
        }

        // Sort messages by ID (chronological order)
        std::sort(m_currentMessages.begin(), m_currentMessages.end(),
                  [](const Message &a, const Message &b) { return a.id < b.id; });

        m_hasMoreMessages = messages.size() >= 50; // Discord returns 50 messages max
        displayMessages();

        // Scroll to bottom only if this was the initial load
        // (if we have <= 50 messages, or if we were at the bottom before?)
        // Actually, if we just loaded the first batch, we want to scroll to bottom.
        // If we loaded older messages, we want to stay where we were (roughly).
        // For simplicity, just scroll to bottom on initial load.
        if (m_currentMessages.size() <= 50) {
             scrollToBottom();
        } });

    connect(m_client, &DiscordClient::newMessage, this, &MainWindow::addMessage);

    connect(m_client, &DiscordClient::apiError, [](const QString &error)
            { QMessageBox::warning(nullptr, "API Error", error); });

    connect(m_client, &DiscordClient::tokenInvalidated, this, &MainWindow::handleTokenInvalidated);

    connect(m_client, &DiscordClient::guildIconLoaded, this, [this](Snowflake guildId, const QPixmap &icon)
            {
        // Find the guild item and update its icon
        for (int i = 0; i < m_guildList->count(); ++i)
        {
            QListWidgetItem *item = m_guildList->item(i);
            if (item->data(Qt::UserRole).toULongLong() == guildId)
            {
                // Scale icon to 40x40 and make it circular (smaller to fit better)
                QPixmap scaled = icon.scaled(40, 40, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

                // Create circular mask
                QPixmap rounded(40, 40);
                rounded.fill(Qt::transparent);
                QPainter painter(&rounded);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setBrush(QBrush(scaled));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(0, 0, 40, 40);

                item->setIcon(QIcon(rounded));
                item->setText(""); // Remove text placeholder
                break;
            }
        } });
}

void MainWindow::tryAutoLogin()
{
    qDebug() << "tryAutoLogin: Checking for saved token...";

    if (m_tokenStorage.hasToken())
    {
        qDebug() << "tryAutoLogin: Token found in storage";
        QString token = m_tokenStorage.loadToken();

        if (!token.isEmpty())
        {
            qDebug() << "tryAutoLogin: Loading saved token, length:" << token.length();
            m_client->loginWithToken(token);
            return;
        }
        else
        {
            qWarning() << "tryAutoLogin: Token loaded but is empty!";
        }
    }
    else
    {
        qDebug() << "tryAutoLogin: No saved token found";
    }

    // No saved token, show login dialog
    qDebug() << "tryAutoLogin: Showing login dialog";
    showLoginDialog();
}

void MainWindow::handleTokenInvalidated()
{
    qDebug() << "Token invalidated, showing login dialog";
    QMessageBox::information(this, "Session Expired",
                             "Your session has expired. Please log in again.");
    showLoginDialog();
}

void MainWindow::onGuildSelected(QListWidgetItem *item)
{
    if (!item)
        return;
    bool ok;
    Snowflake guildId = item->data(Qt::UserRole).toULongLong(&ok);
    if (!ok)
        guildId = 0; // "DM" item might set int 0

    m_selectedGuildId = guildId;

    if (m_selectedGuildId == 0)
    {
        m_currentTitle->setText("Direct Messages");
    }
    else
    {
        m_currentTitle->setText(item->text());
    }

    updateChannelList();
    updateCallButton(); // Update call button when switching between guilds and DMs
}

void MainWindow::onChannelSelected(QListWidgetItem *item)
{
    if (!item)
        return;
    bool ok;
    m_selectedChannelId = item->data(Qt::UserRole).toULongLong(&ok);

    if (ok)
    {
        qDebug() << "Selected Channel ID:" << m_selectedChannelId << "Name:" << item->text();

        // Reset message state for new channel
        m_currentMessages.clear();
        m_hasMoreMessages = true;
        m_isLoadingMessages = false;

        m_messageLog->clear();
        m_messageLog->setText("Loading messages...");
        m_client->getChannelMessages(m_selectedChannelId);

        // Update message input permissions
        updateMessageInputPermissions();

        // Update call button visibility/state
        updateCallButton();
    }
}

void MainWindow::sortGuildList()
{
    // Skip the first item (Home/DM button)
    QList<QListWidgetItem *> items;
    for (int i = 1; i < m_guildList->count(); ++i)
    {
        items.append(m_guildList->takeItem(1)); // Always take index 1 since items shift
    }

    // Sort by joinedAt timestamp (oldest first)
    std::sort(items.begin(), items.end(), [](QListWidgetItem *a, QListWidgetItem *b)
              {
                  QString timeA = a->data(Qt::UserRole + 2).toString();
                  QString timeB = b->data(Qt::UserRole + 2).toString();
                  return timeA < timeB; // Oldest first (earliest timestamp)
              });

    // Re-add sorted items
    for (QListWidgetItem *item : items)
    {
        m_guildList->addItem(item);
    }
}

void MainWindow::updateMessageInputPermissions()
{
    // Find the current guild and channel
    const QList<Guild> &guilds = m_client->getGuilds();

    // DMs are always allowed
    if (m_selectedGuildId == 0)
    {
        m_messageInput->setEnabled(true);
        m_messageInput->setPlaceholderText("Message...");
        return;
    }

    // Find guild and channel
    for (const Guild &guild : guilds)
    {
        if (guild.id == m_selectedGuildId)
        {
            for (const Channel &channel : guild.channels)
            {
                if (channel.id == m_selectedChannelId)
                {
                    bool canSend = m_client->canSendMessages(guild, channel);
                    m_messageInput->setEnabled(canSend);

                    if (canSend)
                    {
                        m_messageInput->setPlaceholderText("Message #" + channel.name);
                    }
                    else
                    {
                        m_messageInput->setPlaceholderText("You do not have permission to send messages in this channel");
                        m_messageInput->setStyleSheet("QLineEdit { color: #72767d; }");
                    }
                    return;
                }
            }
        }
    }
}

void MainWindow::updateGuildList()
{
    // Keeping "Home" at top
    // The rest are added via guildCreated signal dynamically
}

void MainWindow::updateChannelList()
{
    m_channelList->clear();

    if (m_selectedGuildId == 0)
    {
        // Show DMs - sorted by last message ID (descending)
        QList<Channel> dms = m_client->getPrivateChannels();

        // Sort by last message ID in descending order (most recent first)
        std::sort(dms.begin(), dms.end(), [](const Channel &a, const Channel &b)
                  { return a.lastMessageId > b.lastMessageId; });

        for (const Channel &dm : dms)
        {
            QString name = dm.name;

            // Generate name from recipients if empty
            if (name.isEmpty() && !dm.recipients.isEmpty())
            {
                QStringList names;
                for (const User &recipient : dm.recipients)
                {
                    names.append(recipient.username);
                }
                name = names.join(", ");
            }

            if (name.isEmpty())
            {
                name = "Unknown DM";
            }

            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, QString::number(dm.id));
            m_channelList->addItem(item);
        }
    }
    else
    {
        // Show Guild Channels
        // Find guild
        const QList<Guild> &guilds = m_client->getGuilds();
        for (const Guild &g : guilds)
        {
            if (g.id == m_selectedGuildId)
            {
                for (const Channel &c : g.channels)
                {
                    // Filter out channels the user can't view
                    if (!m_client->canViewChannel(g, c))
                    {
                        continue;
                    }

                    if (c.isCategory())
                    {
                        QListWidgetItem *item = new QListWidgetItem(c.name.toUpper());
                        item->setFlags(Qt::NoItemFlags); // Not selectable
                        QFont font = item->font();
                        font.setBold(true);
                        font.setPointSize(9);
                        item->setFont(font);
                        item->setForeground(QColor(150, 150, 150));
                        m_channelList->addItem(item);
                    }
                    else if (!c.isCategory())
                    {
                        // Indent channels that are under a category
                        QString prefix = c.parentId != 0 ? "    " : "";
                        QString icon = c.isVoice() ? "🔊 " : "# ";
                        QListWidgetItem *item = new QListWidgetItem(prefix + icon + c.name);
                        item->setData(Qt::UserRole, QString::number(c.id));

                        // Highlight if this is the connected voice channel
                        if (c.isVoice() && m_isInVoice && c.id == m_currentVoiceChannelId)
                        {
                            QFont font = item->font();
                            font.setBold(true);
                            item->setFont(font);
                            item->setForeground(QColor(88, 101, 242)); // Discord blurple
                        }

                        m_channelList->addItem(item);
                    }
                }
                break;
            }
        }
    }
}

void MainWindow::showLoginDialog()
{
    LoginDialog *dialog = new LoginDialog(this);

    connect(dialog, &LoginDialog::loginRequested, [this](const QString &email, const QString &password)
            { m_client->login(email, password); });

    connect(dialog, &LoginDialog::mfaSubmitted, [this](const QString &code, const QString &ticket)
            { m_client->submitMFA(code, ticket); });

    connect(m_client, &DiscordClient::mfaRequired, dialog, &LoginDialog::showMFAPrompt);

    connect(m_client, &DiscordClient::loginSuccess, dialog, &LoginDialog::accept);

    connect(m_client, &DiscordClient::loginError, [dialog](const QString &error)
            {
        dialog->showError(error);
        dialog->resetAfterError(); });

    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::onScrollValueChanged(int value)
{
    QScrollBar *scrollBar = m_messageLog->verticalScrollBar();

    // Show/hide scroll to bottom button
    bool atBottom = (value >= scrollBar->maximum() - 10);
    m_scrollToBottomBtn->setVisible(!atBottom);

    // Load more messages when scrolled to top
    if (value == scrollBar->minimum() && m_hasMoreMessages && !m_isLoadingMessages && m_selectedChannelId != 0)
    {
        loadMoreMessages();
    }
}

void MainWindow::loadMoreMessages()
{
    if (m_isLoadingMessages || !m_hasMoreMessages || m_selectedChannelId == 0)
        return;

    // Don't try to load more if we have no messages yet (initial load still pending)
    if (m_currentMessages.isEmpty())
    {
        qDebug() << "No messages loaded yet, skipping pagination";
        return;
    }

    m_isLoadingMessages = true;

    // Get oldest message ID for pagination
    Snowflake beforeId = m_currentMessages.first().id;

    // Request messages before the oldest one we have
    qDebug() << "Loading more messages before ID:" << beforeId;

    // Save current scroll position to restore after loading
    QScrollBar *scrollBar = m_messageLog->verticalScrollBar();
    int oldValue = scrollBar->value();
    int oldMax = scrollBar->maximum();

    // Store these for later restoration in messagesLoaded handler
    // For now, just call the API
    m_client->getChannelMessagesBefore(m_selectedChannelId, beforeId);
}

void MainWindow::scrollToBottom()
{
    QScrollBar *scrollBar = m_messageLog->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
    m_scrollToBottomBtn->hide();

    // Trim old messages when jumping to bottom
    if (m_currentMessages.size() > 100)
    {
        // Keep only the last 100 messages
        int toRemove = m_currentMessages.size() - 100;
        m_currentMessages.erase(m_currentMessages.begin(), m_currentMessages.begin() + toRemove);
        displayMessages();

        // Scroll back to bottom after redisplay
        QTimer::singleShot(50, this, [this, scrollBar]()
                           { scrollBar->setValue(scrollBar->maximum()); });
    }
}

void MainWindow::addMessage(const Message &message)
{
    // Update DM last message ID for sorting
    if (m_selectedGuildId == 0)
    {
        // Update the channel's last message ID in the client's data
        QList<Channel> &dms = const_cast<QList<Channel> &>(m_client->getPrivateChannels());
        for (Channel &dm : dms)
        {
            if (dm.id == message.channelId)
            {
                dm.lastMessageId = message.id;
                // Refresh DM list to resort
                if (m_selectedGuildId == 0)
                {
                    updateChannelList();
                }
                break;
            }
        }
    }

    // Only add if it's for the current channel
    if (message.channelId != m_selectedChannelId)
    {
        return;
    }

    // Check if message already exists
    for (const Message &existing : m_currentMessages)
    {
        if (existing.id == message.id)
        {
            return;
        }
    }

    m_currentMessages.append(message);

    // Keep only last 200 messages in memory to avoid bloat
    if (m_currentMessages.size() > 200)
    {
        m_currentMessages.removeFirst();
    }

    // Check if user is at bottom before adding
    QScrollBar *scrollBar = m_messageLog->verticalScrollBar();
    bool wasAtBottom = (scrollBar->value() >= scrollBar->maximum() - 10);

    // Download avatar if not cached
    if (!m_userAvatars.contains(message.author.id) && !message.author.avatar.isEmpty())
    {
        downloadUserAvatar(message.author.id, message.author.avatar);
    }

    // Add message to display
    m_messageLog->append(formatMessageHtml(message));

    // Auto-scroll to bottom only if user was already at bottom
    if (wasAtBottom)
    {
        scrollToBottom();
    }
}

void MainWindow::displayMessages()
{
    m_messageLog->clear();

    if (m_currentMessages.isEmpty())
    {
        // Show empty state message
        m_messageLog->setAlignment(Qt::AlignCenter);
        QString emptyMessage = "<p style='color: #72767d; font-size: 18px; font-weight: bold; margin-top: 150px;'>"
                               "This is the beginning of your conversation"
                               "</p>";
        m_messageLog->append(emptyMessage);
        return;
    }

    m_messageLog->setAlignment(Qt::AlignLeft);

    QDate lastDate;
    Snowflake lastAuthorId = 0;
    QDateTime lastTimestamp;

    for (int i = 0; i < m_currentMessages.size(); ++i)
    {
        const Message &msg = m_currentMessages[i];

        // Download avatar if not cached
        if (!m_userAvatars.contains(msg.author.id) && !msg.author.avatar.isEmpty())
        {
            downloadUserAvatar(msg.author.id, msg.author.avatar);
        }

        // Check if we need a date separator
        QDate msgDate = msg.timestamp.date();
        if (msgDate != lastDate)
        {
            QString dateText;
            QDate today = QDate::currentDate();
            if (msgDate == today)
                dateText = "Today";
            else if (msgDate == today.addDays(-1))
                dateText = "Yesterday";
            else
                dateText = msgDate.toString("MMMM d, yyyy");

            m_messageLog->append(QString(
                                     "<div style='position: relative; margin: 16px 16px; height: 1px; background: #3f4147;'>"
                                     "  <span style='position: absolute; left: 50%%; transform: translateX(-50%%); top: -9px; "
                                     "         background: #36393f; padding: 2px 8px; color: #72767d; font-size: 12px; font-weight: 600;'>%1</span>"
                                     "</div>")
                                     .arg(dateText));

            lastDate = msgDate;
            lastAuthorId = 0; // Reset grouping after date separator
        }

        // Check if this message should be grouped with previous
        bool shouldGroup = false;
        if (i > 0 && msg.author.id == lastAuthorId)
        {
            // Group if same author and within 5 minutes
            qint64 timeDiff = lastTimestamp.secsTo(msg.timestamp);
            if (timeDiff < 300) // 5 minutes
            {
                shouldGroup = true;
            }
        }

        m_messageLog->append(formatMessageHtml(msg, shouldGroup));

        lastAuthorId = msg.author.id;
        lastTimestamp = msg.timestamp;
    }
}

QString MainWindow::formatMessageHtml(const Message &msg, bool grouped)
{
    // Use system locale for time formatting
    QLocale locale;
    QString timestamp = locale.toString(msg.timestamp.time(), QLocale::ShortFormat);

    if (grouped)
    {
        // Grouped message - indent to align with first message content using table
        QString html = QString(
                           "<table style='margin: 0 16px 0 16px; border-collapse: collapse; width: calc(100%% - 32px);' cellspacing='0' cellpadding='0'>"
                           "  <tr>"
                           "    <td style='width: 40px; padding: 0;'></td>" // Avatar space
                           "    <td style='width: 16px; padding: 0;'></td>" // Margin space
                           "    <td style='vertical-align: top; padding: 0;'>"
                           "      <div style='color: #dcddde; font-size: 15px; line-height: 1.375; word-wrap: break-word;'>%1</div>"
                           "    </td>"
                           "  </tr>"
                           "</table>")
                           .arg(msg.content.toHtmlEscaped().replace("\n", "<br>"));

        return html;
    }

    // Get avatar as base64 data URL or use default
    QString avatarData;
    if (m_userAvatars.contains(msg.author.id))
    {
        QPixmap avatar = m_userAvatars[msg.author.id];
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        avatar.save(&buffer, "PNG");
        avatarData = QString("data:image/png;base64,%1").arg(QString(ba.toBase64()));
    }
    else
    {
        // Default gray circle with first letter of username
        avatarData = "data:image/svg+xml;base64," + QString::fromLatin1(
                                                        QString("<svg width='40' height='40' xmlns='http://www.w3.org/2000/svg'>"
                                                                "<circle cx='20' cy='20' r='20' fill='#5865F2'/>"
                                                                "<text x='20' y='28' font-size='18' fill='white' text-anchor='middle' font-family='Arial'>%1</text>"
                                                                "</svg>")
                                                            .arg(msg.author.username.left(1).toUpper())
                                                            .toUtf8()
                                                            .toBase64());
    }

    // First message in group - show avatar, username and timestamp
    // Using table layout for better QTextDocument support
    QString html = QString(
                       "<table style='margin: 16px 16px 0 16px; border-collapse: collapse; width: calc(100%% - 32px);' cellspacing='0' cellpadding='0'>"
                       "  <tr>"
                       "    <td style='width: 40px; vertical-align: top; padding: 0;'>"
                       "      <img src='%1' width='40' height='40' style='border-radius: 50%%;'/>"
                       "    </td>"
                       "    <td style='width: 16px; padding: 0;'></td>"
                       "    <td style='vertical-align: top; padding: 0;'>"
                       "      <div style='margin-bottom: 2px;'>"
                       "        <span style='color: #ffffff; font-weight: 600; font-size: 16px;'>%2</span>"
                       "        <span style='color: #a3a6aa; font-size: 12px; margin-left: 8px; font-weight: 400;'>%3</span>"
                       "      </div>"
                       "      <div style='color: #dcddde; font-size: 15px; line-height: 1.375; word-wrap: break-word;'>%4</div>"
                       "    </td>"
                       "  </tr>"
                       "</table>")
                       .arg(avatarData)
                       .arg(msg.author.username.toHtmlEscaped())
                       .arg(timestamp)
                       .arg(msg.content.toHtmlEscaped().replace("\n", "<br>"));

    return html;
}

QString MainWindow::getUserAvatarUrl(Snowflake userId, const QString &avatarHash) const
{
    if (avatarHash.isEmpty())
        return QString();

    // Discord CDN URL format: https://cdn.discordapp.com/avatars/{user_id}/{avatar_hash}.png
    QString extension = avatarHash.startsWith("a_") ? ".gif" : ".png";
    return QString("https://cdn.discordapp.com/avatars/%1/%2%3")
        .arg(userId)
        .arg(avatarHash)
        .arg(extension);
}

void MainWindow::downloadUserAvatar(Snowflake userId, const QString &avatarHash)
{
    if (avatarHash.isEmpty())
        return;

    QString avatarUrl = getUserAvatarUrl(userId, avatarHash);
    QNetworkAccessManager *networkManager = new QNetworkAccessManager(this);
    QNetworkRequest request(avatarUrl);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, userId]()
            {
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray imageData = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(imageData))
            {
                // Create circular avatar
                QPixmap rounded(40, 40);
                rounded.fill(Qt::transparent);
                QPainter painter(&rounded);
                painter.setRenderHint(QPainter::Antialiasing);

                QPixmap scaled = pixmap.scaled(40, 40, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                painter.setBrush(QBrush(scaled));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(0, 0, 40, 40);

                m_userAvatars[userId] = rounded;

                // Don't refresh entire display - avatar will show on next message or reload
            }
        }
        reply->deleteLater(); });
}

void MainWindow::onChannelDoubleClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    Snowflake channelId = item->data(Qt::UserRole).value<Snowflake>();

    // Find the channel to check if it's a voice channel
    const QList<Channel> &channels = m_client->getChannels(m_selectedGuildId);
    const Channel *voiceChannel = nullptr;

    for (const Channel &channel : channels)
    {
        if (channel.id == channelId)
        {
            voiceChannel = &channel;
            break;
        }
    }

    // Only handle voice channels (type 2 = GUILD_VOICE)
    if (!voiceChannel || voiceChannel->type != 2)
        return;

    // Toggle voice connection
    if (m_isInVoice && m_currentVoiceChannelId == channelId)
    {
        // Already in this voice channel - disconnect
        m_audioManager->stopCapture();
        m_audioManager->stopPlayback();
        m_client->leaveVoiceChannel(m_selectedGuildId);
        m_isInVoice = false;
        m_currentVoiceChannelId = 0;
        qDebug() << "Left voice channel:" << voiceChannel->name;
    }
    else
    {
        // Not in this voice channel - join it
        if (m_isInVoice)
        {
            // Leave current voice channel first
            m_audioManager->stopCapture();
            m_audioManager->stopPlayback();
            m_client->leaveVoiceChannel(m_selectedGuildId);
        }

        m_client->joinVoiceChannel(m_selectedGuildId, channelId, m_isMuted, m_isDeafened);
        m_currentVoiceChannelId = channelId;
        m_isInVoice = true;
        qDebug() << "Joined voice channel:" << voiceChannel->name;
    }

    updateChannelList(); // Refresh to show connection status
}

void MainWindow::onVoiceReady()
{
    qDebug() << "Voice connection ready, starting audio I/O";

    // Start playback immediately
    if (!m_isDeafened)
    {
        m_audioManager->startPlayback();
    }

    // Start capture if not muted
    // Enable voice control buttons
    m_muteBtn->setEnabled(true);
    m_deafenBtn->setEnabled(true);
    m_muteBtn->setChecked(m_isMuted);
    m_deafenBtn->setChecked(m_isDeafened);

    if (!m_isMuted && !m_isDeafened)
    {
        m_audioManager->startCapture();
    }
}

void MainWindow::updateVoiceUI()
{
    // Update channel list to show voice connection status
    updateChannelList();
}

void MainWindow::onMuteToggled()
{
    if (!m_isInVoice)
        return;

    m_isMuted = m_muteBtn->isChecked();
    m_client->getVoiceClient()->setSelfMute(m_isMuted);

    if (m_isMuted)
    {
        m_audioManager->stopCapture();
        m_muteBtn->setToolTip("Unmute");
    }
    else
    {
        if (!m_isDeafened)
        {
            m_audioManager->startCapture();
        }
        m_muteBtn->setToolTip("Mute");
    }
}

void MainWindow::onDeafenToggled()
{
    if (!m_isInVoice)
        return;

    m_isDeafened = m_deafenBtn->isChecked();
    m_client->getVoiceClient()->setSelfDeaf(m_isDeafened);

    if (m_isDeafened)
    {
        // Deafening also mutes
        m_isMuted = true;
        m_muteBtn->setChecked(true);
        m_audioManager->stopCapture();
        m_audioManager->stopPlayback();
        m_deafenBtn->setToolTip("Undeafen");
    }
    else
    {
        m_audioManager->startPlayback();
        // Don't automatically unmute when undeafening
        m_deafenBtn->setToolTip("Deafen");
    }
}

void MainWindow::onLogoutClicked()
{
    // Confirm logout
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Logout",
        "Are you sure you want to logout?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    // Disconnect from voice if connected
    if (m_isInVoice)
    {
        m_audioManager->stopCapture();
        m_audioManager->stopPlayback();
        m_client->leaveVoiceChannel(m_selectedGuildId);
        m_isInVoice = false;
        m_currentVoiceChannelId = 0;
        m_isMuted = false;
        m_isDeafened = false;
    }

    // Disable and reset voice control buttons
    m_muteBtn->setEnabled(false);
    m_muteBtn->setChecked(false);
    m_muteBtn->setToolTip("Mute");
    m_deafenBtn->setEnabled(false);
    m_deafenBtn->setChecked(false);
    m_deafenBtn->setToolTip("Deafen");

    // Clear token
    m_tokenStorage.clearToken();

    // Clear UI state
    m_guildList->clear();
    m_channelList->clear();
    m_messageLog->clear();
    m_messageInput->clear();
    m_currentMessages.clear();
    m_selectedGuildId = 0;
    m_selectedChannelId = 0;
    m_usernameLabel->setText("Not logged in");

    // Re-add Home button to guild list
    QListWidgetItem *homeItem = new QListWidgetItem("DM");
    homeItem->setData(Qt::UserRole, 0);
    homeItem->setTextAlignment(Qt::AlignCenter);
    m_guildList->addItem(homeItem);

    // Show login dialog
    showLoginDialog();
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog settingsDialog(this);

    if (settingsDialog.exec() == QDialog::Accepted)
    {
        QString inputDevice = settingsDialog.getSelectedInputDevice();
        QString outputDevice = settingsDialog.getSelectedOutputDevice();

        qDebug() << "Audio settings saved - Input:" << inputDevice << "Output:" << outputDevice;

        // TODO: Apply device changes to AudioManager
        // For now, just log the selection
    }
}
void MainWindow::onCallButtonClicked()
{
    // Check if we're in a DM channel
    if (m_selectedGuildId != 0)
    {
        qDebug() << "Call button clicked but not in a DM channel";
        return;
    }

    if (m_selectedChannelId == 0)
    {
        qDebug() << "No DM channel selected";
        return;
    }

    if (m_isInCall || m_isInVoice)
    {
        // Leave the call
        qDebug() << "Leaving call in channel:" << m_selectedChannelId;

        // Stop timers
        m_ringingTimer->stop();
        m_noAnswerTimer->stop();

        // Stop ringing if we were ringing
        if (m_isRinging)
        {
            m_client->stopRinging(m_selectedChannelId);
        }

        // Leave voice
        m_client->leaveVoiceChannel(Snowflake(0)); // DM calls use null guild_id

        m_isInCall = false;
        m_isRinging = false;
        m_isInVoice = false;
        m_currentCallChannelId = 0;
        m_currentVoiceChannelId = 0;

        updateCallButton();
        updateVoiceUI();
    }
    else
    {
        // Start a call
        qDebug() << "Starting call in channel:" << m_selectedChannelId;

        m_isInCall = true;
        m_isRinging = true;
        m_currentCallChannelId = m_selectedChannelId;

        // Join voice (this will create the call if it doesn't exist)
        m_client->startCall(m_selectedChannelId);

        // Get recipients to ring
        const QList<Channel> &privateChannels = m_client->getPrivateChannels();
        for (const Channel &channel : privateChannels)
        {
            if (channel.id == m_selectedChannelId)
            {
                // Ring all recipients
                QList<Snowflake> recipientIds;
                for (const User &recipient : channel.recipients)
                {
                    recipientIds.append(recipient.id);
                }

                if (!recipientIds.isEmpty())
                {
                    m_client->ringCall(m_selectedChannelId, recipientIds);

                    // Start ringing timer (10 seconds)
                    m_ringingTimer->start();

                    // Start no-answer timer (5 minutes)
                    m_noAnswerTimer->start();
                }
                break;
            }
        }

        updateCallButton();
    }
}

void MainWindow::onCallCreated(Snowflake channelId, const QList<Snowflake> &ringing)
{
    qDebug() << "Call created in channel:" << channelId << "Ringing:" << ringing.size() << "users";

    if (channelId == m_selectedChannelId)
    {
        m_isInCall = true;
        m_currentCallChannelId = channelId;
        updateCallButton();
    }
}

void MainWindow::onCallUpdated(Snowflake channelId, const QList<Snowflake> &ringing)
{
    qDebug() << "Call updated in channel:" << channelId << "Ringing:" << ringing.size() << "users";

    if (channelId == m_currentCallChannelId)
    {
        // If ringing list is empty, someone answered
        if (ringing.isEmpty() && m_isRinging)
        {
            qDebug() << "Someone answered the call!";
            m_isRinging = false;
            m_ringingTimer->stop();
            m_noAnswerTimer->stop(); // Cancel no-answer timeout
            updateCallButton();
        }
    }
}

void MainWindow::onCallDeleted(Snowflake channelId)
{
    qDebug() << "Call deleted in channel:" << channelId;

    if (channelId == m_currentCallChannelId)
    {
        m_isInCall = false;
        m_isRinging = false;
        m_isInVoice = false;
        m_currentCallChannelId = 0;
        m_currentVoiceChannelId = 0;

        m_ringingTimer->stop();
        m_noAnswerTimer->stop();

        updateCallButton();
        updateVoiceUI();

        qDebug() << "Call ended";
    }
}

void MainWindow::onRingingTimeout()
{
    qDebug() << "Ringing timeout - stopping ring";

    if (m_isRinging)
    {
        m_isRinging = false;
        m_client->stopRinging(m_currentCallChannelId);
        updateCallButton();
    }
}

void MainWindow::onNoAnswerTimeout()
{
    qDebug() << "No answer timeout - disconnecting from call";

    if (m_isInCall)
    {
        // Nobody answered after 5 minutes, disconnect
        m_client->leaveVoiceChannel(Snowflake(0));

        m_isInCall = false;
        m_isRinging = false;
        m_isInVoice = false;
        m_currentCallChannelId = 0;
        m_currentVoiceChannelId = 0;

        updateCallButton();
        updateVoiceUI();

        qDebug() << "Auto-disconnected: no answer after 5 minutes";
    }
}

void MainWindow::updateCallButton()
{
    // Only show call button in DM channels
    if (m_selectedGuildId == 0 && m_selectedChannelId != 0)
    {
        m_callBtn->show();

        if (m_isInCall || m_isInVoice)
        {
            if (m_isRinging)
            {
                m_callBtn->setText("📞 Ringing...");
                m_callBtn->setStyleSheet(
                    "QPushButton { background-color: #FAA61A; color: white; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
                    "QPushButton:hover { background-color: #E09318; }");
            }
            else
            {
                m_callBtn->setText("📴 Leave Call");
                m_callBtn->setStyleSheet(
                    "QPushButton { background-color: #ED4245; color: white; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
                    "QPushButton:hover { background-color: #C03537; }");
            }
        }
        else
        {
            m_callBtn->setText("📞 Call");
            m_callBtn->setStyleSheet(
                "QPushButton { background-color: #3BA55D; color: white; border-radius: 4px; padding: 8px 16px; font-weight: bold; }"
                "QPushButton:hover { background-color: #2D7D46; }");
        }
    }
    else
    {
        m_callBtn->hide();
    }
}
