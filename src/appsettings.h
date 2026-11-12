#pragma once

#include <QString>

struct AppSettings
{
    QString ffmpegPath;
    QString ffprobePath;

    static AppSettings load();
    void save() const;

    static QString defaultFfmpegPath();
    static QString defaultFfprobePath();
};
