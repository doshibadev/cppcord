#include "MainWindow.h"
#include "LoginDialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QMessageBox>
#include <QListWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_client(new DiscordClient(this)),
      m_selectedGuildId(0),
      m_selectedChannelId(0),
      m_isLoadingMessages(false),
      m_hasMoreMessages(true)
{
    setWindowTitle("CPPCord - Discord Client");
    resize(1200, 800);

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
    m_guildList->setIconSize(QSize(48, 48));
    m_guildList->setStyleSheet(
        "QListWidget { background-color: #202225; border: none; }"
        "QListWidget::item { padding: 8px; margin: 4px; border-radius: 24px; }"
        "QListWidget::item:selected { background-color: #5865F2; }"
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
    QHBoxLayout *userInfoLayout = new QHBoxLayout(userInfoWidget);
    // Add user info labels here later
    // userInfoLayout->addWidget(new QLabel("User Info"));
    channelLayout->addWidget(userInfoWidget);

    mainLayout->addWidget(channelPanel);

    // 3. Chat Area (Right)
    QWidget *chatPanel = new QWidget(m_centralWidget);
    QVBoxLayout *chatLayout = new QVBoxLayout(chatPanel);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);

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
    connect(m_scrollToBottomBtn, &QPushButton::clicked, this, &MainWindow::scrollToBottom);

    // Scroll detection for loading more messages and showing/hiding scroll button
    connect(m_messageLog->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::onScrollValueChanged);

    connect(m_client, &DiscordClient::loginSuccess, [this]()
            {
        // Clear lists or init state
        updateGuildList(); });

    connect(m_client, &DiscordClient::guildCreated, [this](const Guild &guild)
            {
        // Add guild to the list
        QListWidgetItem *item = new QListWidgetItem(guild.name.left(2).toUpper());
        item->setData(Qt::UserRole, QVariant::fromValue(guild.id));
        item->setToolTip(guild.name);
        item->setTextAlignment(Qt::AlignCenter);
        m_guildList->addItem(item);
        qDebug() << "Added guild to UI:" << guild.name << "(ID:" << guild.id << ")"; });

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
        scrollToBottom(); });

    connect(m_client, &DiscordClient::newMessage, this, &MainWindow::addMessage);

    connect(m_client, &DiscordClient::apiError, [](const QString &error)
            { QMessageBox::warning(nullptr, "API Error", error); });

    connect(m_client, &DiscordClient::tokenInvalidated, this, &MainWindow::handleTokenInvalidated);
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
}

void MainWindow::onChannelSelected(QListWidgetItem *item)
{
    if (!item)
        return;
    bool ok;
    m_selectedChannelId = item->data(Qt::UserRole).toULongLong(&ok);

    if (ok)
    {
        // Reset message state for new channel
        m_currentMessages.clear();
        m_hasMoreMessages = true;
        m_isLoadingMessages = false;

        m_messageLog->clear();
        m_messageLog->setText("Loading messages...");
        m_client->getChannelMessages(m_selectedChannelId);
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
        // Show DMs
        const QList<Channel> &dms = m_client->getPrivateChannels();
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

                    if (c.type == 4)
                    { // Category
                        QListWidgetItem *item = new QListWidgetItem(c.name.toUpper());
                        item->setFlags(Qt::NoItemFlags); // Not selectable
                        // Styling for category
                        m_channelList->addItem(item);
                    }
                    else
                    {
                        // Indent if under category (not tracking parent yet)
                        QString icon = (c.type == 2) ? "🔊 " : "# ";
                        QListWidgetItem *item = new QListWidgetItem(icon + c.name);
                        item->setData(Qt::UserRole, QString::number(c.id));
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
    qDebug() << "addMessage called - Channel ID:" << message.channelId << "Selected:" << m_selectedChannelId;
    qDebug() << "Message content:" << message.content;

    // Only add if it's for the current channel
    if (message.channelId != m_selectedChannelId)
    {
        qDebug() << "Channel mismatch, ignoring message";
        return;
    }

    // Check if message already exists
    for (const Message &existing : m_currentMessages)
    {
        if (existing.id == message.id)
        {
            qDebug() << "Duplicate message, ignoring";
            return;
        }
    }

    qDebug() << "Adding message to list";
    m_currentMessages.append(message);

    // Keep only last 200 messages in memory to avoid bloat
    if (m_currentMessages.size() > 200)
    {
        m_currentMessages.removeFirst();
    }

    // Check if user is at bottom before adding
    QScrollBar *scrollBar = m_messageLog->verticalScrollBar();
    bool wasAtBottom = (scrollBar->value() >= scrollBar->maximum() - 10);

    // Add message to display
    QString timestamp = message.timestamp.toString("hh:mm AP");
    m_messageLog->append(QString("[%1] %2: %3")
                             .arg(timestamp)
                             .arg(message.author.username)
                             .arg(message.content));

    // Auto-scroll to bottom only if user was already at bottom
    if (wasAtBottom)
    {
        scrollToBottom();
    }
}

void MainWindow::displayMessages()
{
    m_messageLog->clear();

    for (const Message &msg : m_currentMessages)
    {
        QString timestamp = msg.timestamp.toString("hh:mm AP");
        m_messageLog->append(QString("[%1] %2: %3")
                                 .arg(timestamp)
                                 .arg(msg.author.username)
                                 .arg(msg.content));
    }
}
