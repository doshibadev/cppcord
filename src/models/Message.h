#pragma once
#include <QString>
#include <QDateTime>
#include "Snowflake.h"
#include "User.h"

struct Message {
    Snowflake id;
    Snowflake channelId;
    Snowflake guildId; // Optional, might be 0 for DMs
    User author;
    QString content;
    QDateTime timestamp;
    // We can add embeds/attachments later
};
