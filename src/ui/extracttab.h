#pragma once

#include "ffmpeg/ffmpegrunner.h"
#include "ffmpeg/mediainfo.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
struct AppSettings;

class ExtractTab : public QWidget
{
    Q_OBJECT

public:
    explicit ExtractTab(FfmpegRunner *runner, AppSettings *settings, QWidget *parent = nullptr);

private slots:
    void browseInput();
    void browseOutput();
    void inputChanged();
    void updateFormatState();
    void startExtraction();
    void onFinished(quint64 id, FfmpegRunner::Result result, const QString &message);

private:
    void buildUi();
    void showMediaInfo();

    FfmpegRunner *m_runner = nullptr;
    AppSettings *m_settings = nullptr;

    QLineEdit *m_input = nullptr;
    QLineEdit *m_outputDir = nullptr;
    QLineEdit *m_prefix = nullptr;
    QComboBox *m_format = nullptr;
    QSpinBox *m_jpegQuality = nullptr;
    QLabel *m_info = nullptr;
    QLabel *m_estimate = nullptr;

    MediaInfo m_media;
    quint64 m_jobId = 0;
    QString m_outputDirectory;
    QString m_outputPrefix;
    QString m_outputExtension;
};
