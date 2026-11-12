#pragma once

#include "appsettings.h"

#include <QDialog>

class QLineEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings &settings, QWidget *parent = nullptr);

    AppSettings settings() const;

public slots:
    void accept() override;

private slots:
    void browseFfmpeg();
    void browseFfprobe();
    void autoDetect();
    void checkFfmpeg();

private:
    void buildUi();
    void browseTool(QLineEdit *edit, const QString &title);
    void checkTool(const QString &path, const QString &title);

    QLineEdit *m_ffmpegEdit = nullptr;
    QLineEdit *m_ffprobeEdit = nullptr;
};
