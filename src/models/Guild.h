#pragma once
#include <QString>
#include <QList>
#include <QMap>
#include "Snowflake.h"
#include "Channel.h"

// Discord permission bits
namespace Permissions
{
    const quint64 VIEW_CHANNEL = 1ULL << 10;
    const quint64 READ_MESSAGE_HISTORY = 1ULL << 16;
    const quint64 ADMINISTRATOR = 1ULL << 3;
}

struct Role
{
    Snowflake id;
    QString name;
    quint64 permissions;
    int position;
};

struct Guild
{
    Snowflake id;
    QString name;
    QString icon;
    Snowflake ownerId;
    QList<Snowflake> memberRoles; // Current user's roles in this guild
    QMap<Snowflake, Role> roles;  // All roles in the guild (roleId -> Role)

    // Storing channels mapped by ID for easy lookup, or List?
    // Ordered list is better for UI display (using position)
    // Map is better for lookups.
    // Let's use a list for now, easy to iterate.
    QList<Channel> channels;

    QString getIconUrl() const
    {
        if (icon.isEmpty())
            return "";
        return QString("https://cdn.discordapp.com/icons/%1/%2.png").arg(id).arg(icon);
    }
};
