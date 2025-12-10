#pragma once
#include <QString>
#include <QList>
#include <QMap>
#include "Snowflake.h"
#include "Channel.h"

struct Guild {
    Snowflake id;
    QString name;
    QString icon;
    Snowflake ownerId;
    
    // Storing channels mapped by ID for easy lookup, or List?
    // Ordered list is better for UI display (using position)
    // Map is better for lookups.
    // Let's use a list for now, easy to iterate.
    QList<Channel> channels;
    
    QString getIconUrl() const {
        if (icon.isEmpty()) return "";
        return QString("https://cdn.discordapp.com/icons/%1/%2.png").arg(id).arg(icon);
    }
};
