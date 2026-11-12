#pragma once

#include <QSize>
#include <QString>

struct FrameSequence
{
    QString directory;
    QString prefix;
    QString extension;
    int firstNumber = 0;
    int count = 0;
    int padWidth = 6;
    QSize frameSize;

    bool isValid() const;
    int lastNumber() const;

    QString pattern() const;
    QString displayPattern() const;
    QString fileName(int number) const;

    static bool scan(const QString &directory, FrameSequence *result, QString *error = nullptr);
};
