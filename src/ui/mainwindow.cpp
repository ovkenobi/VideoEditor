#include "mainwindow.h"

#include "ui/concattab.h"
#include "ui/extracttab.h"
#include "ui/imagestovideotab.h"
#include "ui/overlaytab.h"
#include "ui/settingsdialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(AppSettings::load())
    , m_runner(new FfmpegRunner(this))
{
    m_runner->setFfmpegPath(m_settings.ffmpegPath);

    connect(m_runner, &FfmpegRunner::logLine, this, &MainWindow::appendLog);
    connect(m_runner, &FfmpegRunner::started, this, &MainWindow::onStarted);
    connect(m_runner, &FfmpegRunner::progress, this, &MainWindow::onProgress);
    connect(m_runner, &FfmpegRunner::finished, this, &MainWindow::onFinished);

    buildUi();

    m_status->setText(QStringLiteral("Готов к работе. FFmpeg: %1").arg(m_settings.ffmpegPath));
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Видеоредактор FFmpeg"));
    resize(1150, 850);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    m_tabs = new QTabWidget(central);
    m_tabs->addTab(new ConcatTab(m_runner, &m_settings, m_tabs),
                   QStringLiteral("Склейка видео"));
    m_tabs->addTab(new ExtractTab(m_runner, &m_settings, m_tabs),
                   QStringLiteral("Извлечение кадров"));
    m_tabs->addTab(new ImagesToVideoTab(m_runner, &m_settings, m_tabs),
                   QStringLiteral("Кадры → видео"));
    m_tabs->addTab(new OverlayTab(m_runner, &m_settings, m_tabs),
                   QStringLiteral("Наложение изображения"));
    root->addWidget(m_tabs, 1);

    auto *jobRow = new QHBoxLayout;
    m_status = new QLabel(QStringLiteral("Готов к работе."), central);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_progress = new QProgressBar(central);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFixedWidth(300);
    m_cancel = new QPushButton(QStringLiteral("Отмена"), central);
    m_cancel->setEnabled(false);
    jobRow->addWidget(m_status, 1);
    jobRow->addWidget(m_progress);
    jobRow->addWidget(m_cancel);
    root->addLayout(jobRow);

    setCentralWidget(central);

    m_logDock = new QDockWidget(QStringLiteral("Журнал FFmpeg"), this);
    m_log = new QPlainTextEdit(m_logDock);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setPlaceholderText(QStringLiteral("Здесь появится команда FFmpeg и его вывод."));
    QFont logFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    logFont.setPointSize(9);
    m_log->setFont(logFont);
    m_logDock->setWidget(m_log);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    resizeDocks({ m_logDock }, { 180 }, Qt::Vertical);

    auto *fileMenu = menuBar()->addMenu(QStringLiteral("Файл"));
    auto *settingsAction = fileMenu->addAction(QStringLiteral("Настройки…"));
    settingsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(QStringLiteral("Выход"));
    quitAction->setShortcut(QKeySequence::Quit);

    auto *logMenu = menuBar()->addMenu(QStringLiteral("Журнал"));
    logMenu->addAction(m_logDock->toggleViewAction());
    auto *clearLogAction = logMenu->addAction(QStringLiteral("Очистить"));
    clearLogAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("Справка"));
    auto *aboutAction = helpMenu->addAction(QStringLiteral("О программе"));

    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    connect(clearLogAction, &QAction::triggered, m_log, &QPlainTextEdit::clear);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    connect(m_cancel, &QPushButton::clicked, m_runner, &FfmpegRunner::cancel);
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(m_settings, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_settings = dialog.settings();
    m_settings.save();
    applySettings();
}

void MainWindow::applySettings()
{
    m_runner->setFfmpegPath(m_settings.ffmpegPath);
    if (!m_runner->isRunning()) {
        m_status->setText(QStringLiteral("Готов к работе. FFmpeg: %1").arg(m_settings.ffmpegPath));
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(
                this, QStringLiteral("О программе"),
                QStringLiteral("<h3>Видеоредактор FFmpeg</h3>"
                               "<p>Учебная программа для простого редактирования видео.</p>"
                               "<p>Возможности:</p>"
                               "<ul>"
                               "<li>склейка двух видеофайлов;</li>"
                               "<li>извлечение всех кадров в PNG/JPG;</li>"
                               "<li>создание видео из набора изображений с заданной длительностью;</li>"
                               "<li>наложение изображения на диапазон кадров.</li>"
                               "</ul>"
                               "<p>Все операции с видео выполняются внешней программой FFmpeg.</p>"));
}

void MainWindow::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void MainWindow::onStarted(quint64 id, const QString &title)
{
    Q_UNUSED(id)
    m_currentTitle = title;
    m_tabs->setEnabled(false);
    m_cancel->setEnabled(true);
    m_progress->setRange(0, 0);
    m_status->setText(QStringLiteral("Выполняется: %1").arg(title));
}

void MainWindow::onProgress(quint64 id, int percent)
{
    Q_UNUSED(id)
    if (percent < 0) {
        m_progress->setRange(0, 0);
        return;
    }

    m_progress->setRange(0, 100);
    m_progress->setValue(percent);
    m_status->setText(QStringLiteral("Выполняется: %1 — %2%")
                      .arg(m_currentTitle)
                      .arg(percent));
}

void MainWindow::onFinished(quint64 id, FfmpegRunner::Result result, const QString &message)
{
    Q_UNUSED(id)
    m_tabs->setEnabled(true);
    m_cancel->setEnabled(false);
    m_progress->setRange(0, 100);

    switch (result) {
    case FfmpegRunner::Result::Success:
        m_progress->setValue(100);
        m_status->setText(QStringLiteral("Готово: %1").arg(m_currentTitle));
        break;
    case FfmpegRunner::Result::Cancelled:
        m_progress->setValue(0);
        m_status->setText(QStringLiteral("Отменено: %1").arg(m_currentTitle));
        break;
    case FfmpegRunner::Result::Failed:
        m_progress->setValue(0);
        m_status->setText(QStringLiteral("Ошибка: %1").arg(message.section(u'\n', 0, 0)));
        break;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_runner->isRunning()) {
        const auto answer = QMessageBox::question(
                    this, QStringLiteral("Выход"),
                    QStringLiteral("Операция ещё выполняется. Отменить её и выйти?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_runner->cancelAndWait();
    }

    event->accept();
}
