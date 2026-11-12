#include "appsettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

QString findTool(const QString &exeName)
{
    const QString appDirPath = QDir(QCoreApplication::applicationDirPath()).filePath(exeName);
    if (QFileInfo::exists(appDirPath))
        return QDir::toNativeSeparators(appDirPath);

    const QString cwdPath = QDir::current().filePath(exeName);
    if (QFileInfo::exists(cwdPath))
        return QDir::toNativeSeparators(cwdPath);

    const QString found = QStandardPaths::findExecutable(exeName);
    if (!found.isEmpty())
        return found;

    return exeName;
}

bool usablePath(const QString &path)
{
    return !path.trimmed().isEmpty()
            && (QFileInfo::exists(path) || path == QStringLiteral("ffmpeg")
                || path == QStringLiteral("ffmpeg.exe") || path == QStringLiteral("ffprobe")
                || path == QStringLiteral("ffprobe.exe"));
}

} // namespace

QString AppSettings::defaultFfmpegPath()
{
    return findTool(QStringLiteral("ffmpeg.exe"));
}

QString AppSettings::defaultFfprobePath()
{
    return findTool(QStringLiteral("ffprobe.exe"));
}

AppSettings AppSettings::load()
{
    QSettings store(QStringLiteral("OC"), QStringLiteral("VideoEditor"));

    AppSettings settings;
    settings.ffmpegPath = store.value(QStringLiteral("ffmpeg/path")).toString();
    settings.ffprobePath = store.value(QStringLiteral("ffprobe/path")).toString();

    if (!usablePath(settings.ffmpegPath))
        settings.ffmpegPath = defaultFfmpegPath();
    if (!usablePath(settings.ffprobePath))
        settings.ffprobePath = defaultFfprobePath();

    return settings;
}

void AppSettings::save() const
{
    QSettings store(QStringLiteral("OC"), QStringLiteral("VideoEditor"));
    store.setValue(QStringLiteral("ffmpeg/path"), ffmpegPath);
    store.setValue(QStringLiteral("ffprobe/path"), ffprobePath);
}
