#pragma once
#include <QString>
#include <QList>
#include "Snowflake.h"
#include "User.h"

enum class ChannelType
{
    GUILD_TEXT = 0,
    DM = 1,
    GUILD_VOICE = 2,
    GROUP_DM = 3,
    GUILD_CATEGORY = 4,
    // Add others as needed
    UNKNOWN = -1
};

struct Channel
{
    Snowflake id;
    int type;
    Snowflake guildId; // 0 if DM
    int position;
    QString name;
    QString topic;
    Snowflake lastMessageId;

    // For DMs
    QList<User> recipients;

    bool isText() const
    {
        return type == (int)ChannelType::GUILD_TEXT || type == (int)ChannelType::DM || type == (int)ChannelType::GROUP_DM;
    }

    bool isVoice() const
    {
        return type == (int)ChannelType::GUILD_VOICE;
    }

    bool isCategory() const
    {
        return type == (int)ChannelType::GUILD_CATEGORY;
    }

    bool isDm() const
    {
        return type == (int)ChannelType::DM || type == (int)ChannelType::GROUP_DM;
    }
};
