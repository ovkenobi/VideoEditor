#pragma once

#include "ffmpeg/ffmpegrunner.h"
#include "media/framesequence.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
struct AppSettings;

class ImagesToVideoTab : public QWidget
{
    Q_OBJECT

public:
    explicit ImagesToVideoTab(FfmpegRunner *runner, AppSettings *settings, QWidget *parent = nullptr);

private slots:
    void browseFrames();
    void browseOutput();
    void scanFrames();
    void updateFps();
    void startEncoding();
    void onFinished(quint64 id, FfmpegRunner::Result result, const QString &message);

private:
    void buildUi();
    double computedFps() const;

    FfmpegRunner *m_runner = nullptr;
    AppSettings *m_settings = nullptr;

    QLineEdit *m_framesDir = nullptr;
    QLineEdit *m_output = nullptr;
    QLabel *m_info = nullptr;
    QLabel *m_fpsLabel = nullptr;
    QDoubleSpinBox *m_duration = nullptr;

    FrameSequence m_sequence;
    quint64 m_jobId = 0;
    QString m_outputPath;
    double m_usedDuration = 0.0;
    double m_usedFps = 0.0;
};
