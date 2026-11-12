#include "ffmpegrunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

FfmpegRunner::FfmpegRunner(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &FfmpegRunner::onStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &FfmpegRunner::onStandardError);
    connect(m_process, &QProcess::finished,
            this, &FfmpegRunner::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &FfmpegRunner::onProcessError);
}

void FfmpegRunner::setFfmpegPath(const QString &path)
{
    m_ffmpegPath = path;
}

QString FfmpegRunner::ffmpegPath() const
{
    return m_ffmpegPath;
}

bool FfmpegRunner::isRunning() const
{
    return m_running;
}

quint64 FfmpegRunner::run(const Job &job)
{
    if (m_running) {
        emit logLine(QStringLiteral("Другая операция уже выполняется."));
        return 0;
    }

    if (m_ffmpegPath.trimmed().isEmpty()) {
        emit logLine(QStringLiteral("Путь к ffmpeg не задан."));
        emit finished(0, Result::Failed, QStringLiteral("Не задан путь к ffmpeg. Проверьте настройки."));
        return 0;
    }

    m_currentId = m_nextId++;
    m_job = job;
    m_running = true;
    m_cancelled = false;
    m_finalized = false;
    m_lastPercent = -1;
    m_stdoutBuffer.clear();
    m_stderrTail.clear();

    m_process->setWorkingDirectory(job.workingDirectory.isEmpty()
                                   ? QDir::currentPath()
                                   : job.workingDirectory);

    emit logLine(QString());
    emit logLine(QStringLiteral("$ %1").arg(formatCommand(m_ffmpegPath, job.args)));
    emit started(m_currentId, job.title);
    emit progress(m_currentId,
                  job.progressMode != ProgressMode::None && job.total > 0 ? 0 : -1);

    m_process->start(m_ffmpegPath, job.args);
    return m_currentId;
}

void FfmpegRunner::cancel()
{
    if (!m_running)
        return;

    m_cancelled = true;
    emit logLine(QStringLiteral("Отмена операции…"));
    m_process->kill();
}

void FfmpegRunner::cancelAndWait(int timeoutMs)
{
    if (!m_running)
        return;

    m_cancelled = true;
    emit logLine(QStringLiteral("Отмена операции…"));
    m_process->kill();
    if (!m_process->waitForFinished(timeoutMs)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void FfmpegRunner::onStandardOutput()
{
    m_stdoutBuffer += m_process->readAllStandardOutput();

    int newline = -1;
    while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
        QByteArray line = m_stdoutBuffer.left(newline);
        m_stdoutBuffer.remove(0, newline + 1);
        line.replace('\r', "");
        parseProgressLine(QString::fromUtf8(line));
    }
}

void FfmpegRunner::onStandardError()
{
    const QString text = QString::fromUtf8(m_process->readAllStandardError());
    const QStringList lines = text.split(u'\n', Qt::SkipEmptyParts);

    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        m_stderrTail.append(line);
        while (m_stderrTail.size() > 100)
            m_stderrTail.removeFirst();
        emit logLine(line);
    }
}

void FfmpegRunner::parseProgressLine(const QString &line)
{
    const int equals = line.indexOf(u'=');
    if (equals <= 0)
        return;

    const QString key = line.left(equals).trimmed();
    const QString value = line.mid(equals + 1).trimmed();

    if (key == QStringLiteral("out_time_us")) {
        if (m_job.progressMode == ProgressMode::Time && m_job.total > 0) {
            bool ok = false;
            const qint64 microseconds = value.toLongLong(&ok);
            if (ok) {
                const double percent = (double(microseconds) / 1000.0)
                        / double(m_job.total) * 100.0;
                emitPercent(int(percent));
            }
        }
    } else if (key == QStringLiteral("frame")) {
        if (m_job.progressMode == ProgressMode::Frames && m_job.total > 0) {
            bool ok = false;
            const qint64 frames = value.toLongLong(&ok);
            if (ok)
                emitPercent(int(double(frames) / double(m_job.total) * 100.0));
        }
    } else if (key == QStringLiteral("progress") && value == QStringLiteral("end")) {
        emitPercent(100);
    }
}

void FfmpegRunner::emitPercent(int percent)
{
    percent = qBound(0, percent, 100);
    if (percent == m_lastPercent)
        return;
    m_lastPercent = percent;
    emit progress(m_currentId, percent);
}

void FfmpegRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_running || m_finalized)
        return;

    onStandardOutput();
    onStandardError();

    if (m_cancelled) {
        for (const QString &file : m_job.filesToRemoveOnFailure)
            QFile::remove(file);
        finalize(Result::Cancelled, QStringLiteral("Операция отменена."));
        return;
    }

    if (status != QProcess::NormalExit || exitCode != 0) {
        for (const QString &file : m_job.filesToRemoveOnFailure)
            QFile::remove(file);

        QString message;
        if (!m_stderrTail.isEmpty()) {
            const int first = qMax(0, int(m_stderrTail.size()) - 8);
            message = m_stderrTail.mid(first).join(u'\n');
        } else {
            message = QStringLiteral("FFmpeg завершился с кодом %1.").arg(exitCode);
        }
        finalize(Result::Failed, message);
        return;
    }

    finalize(Result::Success, QStringLiteral("Готово."));
}

void FfmpegRunner::onProcessError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart)
        return;

    if (m_running && !m_finalized) {
        finalize(Result::Failed,
                 QStringLiteral("Не удалось запустить FFmpeg: %1").arg(m_process->errorString()));
    }
}

void FfmpegRunner::finalize(Result result, const QString &message)
{
    if (m_finalized)
        return;

    m_finalized = true;
    m_running = false;

    const quint64 id = m_currentId;
    if (result == Result::Success)
        emit progress(id, 100);

    emit finished(id, result, message);
    emit logLine(QStringLiteral("Завершено: %1").arg(message));
}

QString FfmpegRunner::formatCommand(const QString &program, const QStringList &args)
{
    auto quote = [](QString value) {
        if (value.contains(u' ') || value.contains(u'\t') || value.contains(u'"')) {
            value.replace(u'"', QStringLiteral("\\\""));
            return QStringLiteral("\"%1\"").arg(value);
        }
        return value;
    };

    QStringList parts;
    parts << quote(program);
    for (const QString &arg : args)
        parts << quote(arg);
    return parts.join(u' ');
}
