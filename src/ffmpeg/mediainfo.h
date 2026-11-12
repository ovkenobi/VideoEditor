#pragma once

#include <QString>
#include <QtGlobal>

struct MediaInfo
{
    QString path;
    bool valid = false;
    QString error;

    double durationSec = 0.0;
    int width = 0;
    int height = 0;

    QString fpsExpr;
    double fps = 0.0;

    QString videoCodec;
    QString pixFmt;

    bool hasAudio = false;
    QString audioCodec;
    int sampleRate = 0;
    int channels = 0;

    qint64 frameCount = 0;
};
