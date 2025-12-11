#pragma once

#include <QString>
#include <QJsonObject>
#include "Types.h"

struct VoiceState
{
    Snowflake guildId;
    Snowflake channelId; // null if not in a voice channel
    Snowflake userId;
    QString sessionId;
    bool deaf = false;
    bool mute = false;
    bool selfDeaf = false;
    bool selfMute = false;
    bool selfStream = false;
    bool selfVideo = false;
    bool suppress = false;

    static VoiceState fromJson(const QJsonObject &json)
    {
        VoiceState state;
        state.guildId = json["guild_id"].toString();
        state.channelId = json["channel_id"].toString();
        state.userId = json["user_id"].toString();
        state.sessionId = json["session_id"].toString();
        state.deaf = json["deaf"].toBool();
        state.mute = json["mute"].toBool();
        state.selfDeaf = json["self_deaf"].toBool();
        state.selfMute = json["self_mute"].toBool();
        state.selfStream = json["self_stream"].toBool(false);
        state.selfVideo = json["self_video"].toBool(false);
        state.suppress = json["suppress"].toBool(false);
        return state;
    }
};

struct VoiceServerUpdate
{
    QString token;
    Snowflake guildId;
    QString endpoint; // e.g., "sweetwater-12345.discord.media:2048"

    static VoiceServerUpdate fromJson(const QJsonObject &json)
    {
        VoiceServerUpdate update;
        update.token = json["token"].toString();
        update.guildId = json["guild_id"].toString();
        update.endpoint = json["endpoint"].toString();
        return update;
    }
};
