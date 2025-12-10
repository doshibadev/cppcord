#pragma once
#include <QString>
#include "Snowflake.h"

struct User {
    Snowflake id;
    QString username;
    QString discriminator;
    QString avatar;
    QString email;
    bool mfaEnabled = false;
    bool verified = false;

    QString getTag() const {
        if (discriminator == "0") return username; // New username system
        return username + "#" + discriminator;
    }
};
