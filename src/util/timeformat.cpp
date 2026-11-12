#include "timeformat.h"

#include <QtMath>

QString formatDuration(double seconds)
{
    if (!qIsFinite(seconds) || seconds < 0.0)
        seconds = 0.0;

    const qint64 totalMs = qRound64(seconds * 1000.0);
    const int ms = int(totalMs % 1000);
    const qint64 totalSeconds = totalMs / 1000;
    const int sec = int(totalSeconds % 60);
    const int min = int((totalSeconds / 60) % 60);
    const int hour = int(totalSeconds / 3600);

    return QStringLiteral("%1:%2:%3.%4")
            .arg(hour, 2, 10, QChar(u'0'))
            .arg(min, 2, 10, QChar(u'0'))
            .arg(sec, 2, 10, QChar(u'0'))
            .arg(ms, 3, 10, QChar(u'0'));
}

QString formatFps(double fps)
{
    if (!qIsFinite(fps) || fps <= 0.0)
        return QStringLiteral("—");

    QString text = QString::number(fps, 'f', 3);
    while (text.contains(u'.') && text.endsWith(u'0'))
        text.chop(1);
    if (text.endsWith(u'.'))
        text.chop(1);
    return text;
}
