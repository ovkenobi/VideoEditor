#pragma once

#include "mediainfo.h"

#include <QString>

class Ffprobe
{
public:
    explicit Ffprobe(const QString &program = QStringLiteral("ffprobe"));

    void setProgram(const QString &program);
    QString program() const;

    MediaInfo probe(const QString &filePath) const;

private:
    QString m_program;
};

QString mediaSummary(const MediaInfo &info);
