#include "MainWindow.h"
#include "LoginDialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QMessageBox>
#include <QListWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_client(new DiscordClient(this))
    , m_selectedGuildId(0)
    , m_selectedChannelId(0)
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
    
    m_messageLog = new QTextEdit(chatPanel);
    m_messageLog->setReadOnly(true);
    chatLayout->addWidget(m_messageLog);
    
    m_messageInput = new QLineEdit(chatPanel);
    m_messageInput->setPlaceholderText("Message...");
    chatLayout->addWidget(m_messageInput);
    
    mainLayout->addWidget(chatPanel);

    setCentralWidget(m_centralWidget);
    
    // Add login button to menu or toolbar later. For now, trigger via menu?
    // Or just pop up login if not logged in.
    QTimer::singleShot(100, this, &MainWindow::showLoginDialog);
}

void MainWindow::connectSignals()
{
    connect(m_guildList, &QListWidget::itemClicked, this, &MainWindow::onGuildSelected);
    connect(m_channelList, &QListWidget::itemClicked, this, &MainWindow::onChannelSelected);
    
    connect(m_client, &DiscordClient::loginSuccess, [this]() {
        // Clear lists or init state
        updateGuildList();
    });

    connect(m_client, &DiscordClient::guildCreated, [this](const Guild &guild) {
        // Determine if we should update list
        // Yes, just add item
        QListWidgetItem *item = new QListWidgetItem(guild.name);
        item->setData(Qt::UserRole, QString::number(guild.id));
        item->setToolTip(guild.name);
        m_guildList->addItem(item);
    });
    
    connect(m_client, &DiscordClient::channelCreated, [this](const Channel &channel) {
        if (m_selectedGuildId == 0 && channel.isDm()) {
            updateChannelList();
        }
    });

    connect(m_client, &DiscordClient::messagesLoaded, [this](Snowflake channelId, const QList<Message> &messages) {
        if (channelId != m_selectedChannelId) return; // Ignore if switched channel
        
        m_messageLog->clear();
        for (const Message &msg : messages) {
            QString timestamp = msg.timestamp.toString("hh:mm AP");
            m_messageLog->append(QString("[%1] %2: %3").arg(timestamp).arg(msg.author.username).arg(msg.content));
        }
    });

    connect(m_client, &DiscordClient::messageReceived, [this](const QString &message) {
        m_messageLog->append(message);
    });

    connect(m_client, &DiscordClient::apiError, [](const QString &error) {
        QMessageBox::warning(nullptr, "API Error", error);
    });
}

void MainWindow::onGuildSelected(QListWidgetItem *item)
{
    if (!item) return;
    bool ok;
    Snowflake guildId = item->data(Qt::UserRole).toULongLong(&ok);
    if (!ok) guildId = 0; // "DM" item might set int 0
    
    m_selectedGuildId = guildId;
    
    if (m_selectedGuildId == 0) {
        m_currentTitle->setText("Direct Messages");
    } else {
        m_currentTitle->setText(item->text());
    }
    
    updateChannelList();
}

void MainWindow::onChannelSelected(QListWidgetItem *item)
{
    if (!item) return;
    bool ok;
    m_selectedChannelId = item->data(Qt::UserRole).toULongLong(&ok);
    
    if (ok) {
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
    
    if (m_selectedGuildId == 0) {
        // Show DMs
        const QList<Channel>& dms = m_client->getPrivateChannels();
        for (const Channel &dm : dms) {
            QString name = dm.name;
            if (name.isEmpty()) name = "DM " + QString::number(dm.id); // Placeholder
            
            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, QString::number(dm.id));
            m_channelList->addItem(item);
        }
    } else {
        // Show Guild Channels
        // Find guild
        const QList<Guild>& guilds = m_client->getGuilds();
        for (const Guild &g : guilds) {
            if (g.id == m_selectedGuildId) {
                for (const Channel &c : g.channels) {
                    if (c.type == 4) { // Category
                        QListWidgetItem *item = new QListWidgetItem(c.name.toUpper());
                        item->setFlags(Qt::NoItemFlags); // Not selectable
                        // Styling for category
                        m_channelList->addItem(item);
                    } else {
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

    connect(dialog, &LoginDialog::loginRequested, [this](const QString &email, const QString &password) {
        m_client->login(email, password);
    });

    connect(dialog, &LoginDialog::mfaSubmitted, [this](const QString &code, const QString &ticket) {
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
