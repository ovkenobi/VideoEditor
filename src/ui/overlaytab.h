#pragma once

#include "ffmpeg/ffmpegrunner.h"
#include "media/framesequence.h"

#include <QImage>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QToolButton;
class PreviewWidget;
struct AppSettings;

class OverlayTab : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayTab(FfmpegRunner *runner, AppSettings *settings, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void browseFrames();
    void browseOverlay();
    void browseOutput();
    void scanFrames();
    void loadOverlay();
    void updateRange();
    void updatePreview();
    void fitPreview();
    void zoomActualSize();
    void zoomIn();
    void zoomOut();
    void startOverlay();
    void onFinished(quint64 id, FfmpegRunner::Result result, const QString &message);

private:
    void buildUi();
    void renderPreview();
    void setZoomPercent(int percent);
    void updateZoomControls(int percent);
    int currentZoomPercent() const;

    FfmpegRunner *m_runner = nullptr;
    AppSettings *m_settings = nullptr;

    QLineEdit *m_framesDir = nullptr;
    QLineEdit *m_overlayPath = nullptr;
    QLineEdit *m_outputDir = nullptr;
    QLabel *m_info = nullptr;
    QLabel *m_overlayInfo = nullptr;
    QScrollArea *m_previewScroll = nullptr;
    PreviewWidget *m_preview = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QSpinBox *m_zoomSpin = nullptr;
    QToolButton *m_fitButton = nullptr;
    QToolButton *m_actualSizeButton = nullptr;
    QSpinBox *m_x = nullptr;
    QSpinBox *m_y = nullptr;
    QSpinBox *m_startFrame = nullptr;
    QSpinBox *m_frameCount = nullptr;

    FrameSequence m_sequence;
    QImage m_overlay;
    QImage m_frameImage;
    QImage m_composite;
    int m_cachedFrameNumber = -1;

    bool m_fitMode = true;
    double m_zoom = 1.0;
    double m_effectiveScale = 1.0;
    bool m_updatingZoomControls = false;

    quint64 m_jobId = 0;
    QString m_outputDirectory;
};
