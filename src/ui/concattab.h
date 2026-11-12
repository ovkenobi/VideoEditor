#pragma once

#include "ffmpeg/ffmpegrunner.h"
#include "ffmpeg/mediainfo.h"

#include <QTemporaryDir>
#include <QWidget>

#include <memory>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
struct AppSettings;

class ConcatTab : public QWidget
{
    Q_OBJECT

public:
    explicit ConcatTab(FfmpegRunner *runner, AppSettings *settings, QWidget *parent = nullptr);

private slots:
    void browseFileA();
    void browseFileB();
    void browseOutput();
    void swapInputs();
    void startConcat();
    void onFinished(quint64 id, FfmpegRunner::Result result, const QString &message);

private:
    void buildUi();
    void probeInput(bool second);
    void updateModeState();
    bool parametersMatch() const;
    QString targetDescription() const;
    QStringList buildReencodeFilters(bool &hasAudio) const;

    FfmpegRunner *m_runner = nullptr;
    AppSettings *m_settings = nullptr;

    QLineEdit *m_inputA = nullptr;
    QLineEdit *m_inputB = nullptr;
    QLineEdit *m_output = nullptr;
    QLabel *m_infoA = nullptr;
    QLabel *m_infoB = nullptr;
    QComboBox *m_mode = nullptr;
    QLabel *m_modeInfo = nullptr;

    MediaInfo m_mediaA;
    MediaInfo m_mediaB;
    quint64 m_jobId = 0;
    QString m_outputPath;
    std::unique_ptr<QTemporaryDir> m_tempDir;
};
