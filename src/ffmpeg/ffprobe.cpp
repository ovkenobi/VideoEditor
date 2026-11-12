#include "ffprobe.h"

#include "util/timeformat.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtMath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

double parseRational(const QString &value)
{
    const int slash = value.indexOf(u'/');
    bool okNum = false;
    bool okDen = false;
    const double num = value.left(slash).toDouble(&okNum);
    if (!okNum)
        return 0.0;

    if (slash < 0)
        return num;

    const double den = value.mid(slash + 1).toDouble(&okDen);
    if (!okDen || qFuzzyIsNull(den))
        return 0.0;

    return num / den;
}

QString rateExpression(const QJsonObject &stream)
{
    const QString avg = stream.value(QStringLiteral("avg_frame_rate")).toString();
    const QString raw = stream.value(QStringLiteral("r_frame_rate")).toString();

    if (!avg.isEmpty() && avg != QStringLiteral("0/0"))
        return avg;
    if (!raw.isEmpty() && raw != QStringLiteral("0/0"))
        return raw;
    return {};
}

qint64 parseFrameCount(const QJsonValue &value)
{
    if (value.isString()) {
        bool ok = false;
        const qint64 frames = value.toString().toLongLong(&ok);
        return ok ? frames : 0;
    }
    if (value.isDouble())
        return qint64(value.toDouble());
    return 0;
}

} // namespace

Ffprobe::Ffprobe(const QString &program)
    : m_program(program)
{
}

void Ffprobe::setProgram(const QString &program)
{
    m_program = program;
}

QString Ffprobe::program() const
{
    return m_program;
}

MediaInfo Ffprobe::probe(const QString &filePath) const
{
    MediaInfo info;
    info.path = filePath;

    if (!QFileInfo::exists(filePath)) {
        info.error = QStringLiteral("Файл не найден.");
        return info;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif

    const QStringList arguments{
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_format"), QStringLiteral("-show_streams"),
        filePath
    };

    process.start(m_program, arguments);
    if (!process.waitForStarted(5000)) {
        info.error = QStringLiteral("Не удалось запустить ffprobe: %1").arg(process.errorString());
        return info;
    }

    if (!process.waitForFinished(20000)) {
        process.kill();
        process.waitForFinished(2000);
        info.error = QStringLiteral("ffprobe не ответил за 20 секунд.");
        return info;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
        info.error = err.isEmpty()
                ? QStringLiteral("ffprobe завершился с кодом %1.").arg(process.exitCode())
                : err;
        return info;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        info.error = QStringLiteral("Не удалось разобрать ответ ffprobe: %1").arg(parseError.errorString());
        return info;
    }

    const QJsonObject root = document.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
    if (format.contains(QStringLiteral("duration")))
        info.durationSec = format.value(QStringLiteral("duration")).toString().toDouble();

    const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();

    QJsonObject video;
    QJsonObject audio;
    for (const QJsonValue &value : streams) {
        const QJsonObject stream = value.toObject();
        const QString type = stream.value(QStringLiteral("codec_type")).toString();

        if (type == QStringLiteral("video") && video.isEmpty()) {
            const QJsonObject disposition = stream.value(QStringLiteral("disposition")).toObject();
            if (disposition.value(QStringLiteral("attached_pic")).toInt() == 1)
                continue;
            video = stream;
        } else if (type == QStringLiteral("audio") && audio.isEmpty()) {
            audio = stream;
        }
    }

    if (video.isEmpty()) {
        info.error = QStringLiteral("В файле не найдена видеодорожка.");
        return info;
    }

    info.width = video.value(QStringLiteral("width")).toInt();
    info.height = video.value(QStringLiteral("height")).toInt();
    info.videoCodec = video.value(QStringLiteral("codec_name")).toString();
    info.pixFmt = video.value(QStringLiteral("pix_fmt")).toString();
    info.fpsExpr = rateExpression(video);
    info.fps = parseRational(info.fpsExpr);

    if (!audio.isEmpty()) {
        info.hasAudio = true;
        info.audioCodec = audio.value(QStringLiteral("codec_name")).toString();
        info.sampleRate = audio.value(QStringLiteral("sample_rate")).toString().toInt();
        info.channels = audio.value(QStringLiteral("channels")).toInt();
    }

    info.frameCount = parseFrameCount(video.value(QStringLiteral("nb_frames")));
    if (info.frameCount <= 0 && info.durationSec > 0.0 && info.fps > 0.0)
        info.frameCount = qRound64(info.durationSec * info.fps);

    info.valid = info.width > 0 && info.height > 0;
    if (!info.valid)
        info.error = QStringLiteral("Не удалось определить размеры видео.");

    return info;
}

QString mediaSummary(const MediaInfo &info)
{
    if (!info.valid)
        return info.error.isEmpty() ? QStringLiteral("Параметры не определены.") : info.error;

    QString text = QStringLiteral("%1, %2×%3, %4 fps, видео %5")
            .arg(formatDuration(info.durationSec))
            .arg(info.width)
            .arg(info.height)
            .arg(formatFps(info.fps))
            .arg(info.videoCodec.isEmpty() ? QStringLiteral("—") : info.videoCodec);

    if (info.hasAudio) {
        text += QStringLiteral(", аудио %1, %2 Гц, %3 кан.")
                .arg(info.audioCodec.isEmpty() ? QStringLiteral("—") : info.audioCodec)
                .arg(info.sampleRate)
                .arg(info.channels);
    } else {
        text += QStringLiteral(", без аудио");
    }

    if (info.frameCount > 0)
        text += QStringLiteral(", кадров ~%1").arg(info.frameCount);

    return text;
}
