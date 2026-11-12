#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class FfmpegRunner : public QObject
{
    Q_OBJECT

public:
    enum class ProgressMode {
        None,
        Time,
        Frames
    };
    Q_ENUM(ProgressMode)

    enum class Result {
        Success,
        Failed,
        Cancelled
    };
    Q_ENUM(Result)

    struct Job
    {
        QString title;
        QStringList args;
        QString workingDirectory;
        ProgressMode progressMode = ProgressMode::None;
        qint64 total = 0; // milliseconds for Time, frames for Frames
        QStringList filesToRemoveOnFailure;
    };

    explicit FfmpegRunner(QObject *parent = nullptr);

    void setFfmpegPath(const QString &path);
    QString ffmpegPath() const;

    bool isRunning() const;

    quint64 run(const Job &job);
    void cancel();
    void cancelAndWait(int timeoutMs = 3000);

signals:
    void started(quint64 id, const QString &title);
    void progress(quint64 id, int percent);
    void logLine(const QString &line);
    void finished(quint64 id, FfmpegRunner::Result result, const QString &message);

private slots:
    void onStandardOutput();
    void onStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    void parseProgressLine(const QString &line);
    void emitPercent(int percent);
    void finalize(Result result, const QString &message);
    static QString formatCommand(const QString &program, const QStringList &args);

    QProcess *m_process = nullptr;
    QString m_ffmpegPath;

    bool m_running = false;
    bool m_cancelled = false;
    bool m_finalized = false;

    quint64 m_nextId = 1;
    quint64 m_currentId = 0;
    Job m_job;
    QByteArray m_stdoutBuffer;
    QStringList m_stderrTail;
    int m_lastPercent = -1;
};
