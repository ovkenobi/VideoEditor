#pragma once

#include "appsettings.h"
#include "ffmpeg/ffmpegrunner.h"

#include <QMainWindow>

class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openSettings();
    void showAbout();
    void appendLog(const QString &line);
    void onStarted(quint64 id, const QString &title);
    void onProgress(quint64 id, int percent);
    void onFinished(quint64 id, FfmpegRunner::Result result, const QString &message);

private:
    void buildUi();
    void applySettings();

    AppSettings m_settings;
    FfmpegRunner *m_runner = nullptr;

    QTabWidget *m_tabs = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QDockWidget *m_logDock = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_cancel = nullptr;

    QString m_currentTitle;
};
