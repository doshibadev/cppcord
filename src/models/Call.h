#pragma once
#include <QString>
#include <QList>
#include "Snowflake.h"
#include "User.h"

struct Call
{
    Snowflake channelId;
    QList<Snowflake> ringing; // User IDs being rung
    QString region;
    QList<Snowflake> participants; // User IDs currently in the call
    bool isActive;
};
