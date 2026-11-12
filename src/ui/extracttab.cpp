#include "extracttab.h"

#include "appsettings.h"
#include "ffmpeg/ffprobe.h"
#include "util/timeformat.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

ExtractTab::ExtractTab(FfmpegRunner *runner, AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_runner(runner)
    , m_settings(settings)
{
    buildUi();
    connect(m_runner, &FfmpegRunner::finished, this, &ExtractTab::onFinished);
}

void ExtractTab::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *sourceBox = new QGroupBox(QStringLiteral("Исходное видео"), this);
    auto *sourceGrid = new QGridLayout(sourceBox);
    sourceGrid->setColumnStretch(1, 1);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("Видеофайл, из которого извлекаются кадры"));
    auto *browseInput = new QPushButton(QStringLiteral("Обзор…"), this);
    m_info = new QLabel(QStringLiteral("Файл не выбран."), this);
    m_info->setWordWrap(true);
    m_info->setStyleSheet(QStringLiteral("color:#555;"));

    sourceGrid->addWidget(new QLabel(QStringLiteral("Видео:"), this), 0, 0);
    sourceGrid->addWidget(m_input, 0, 1);
    sourceGrid->addWidget(browseInput, 0, 2);
    sourceGrid->addWidget(m_info, 1, 1, 1, 2);
    root->addWidget(sourceBox);

    auto *outputBox = new QGroupBox(QStringLiteral("Кадры"), this);
    auto *outputForm = new QFormLayout(outputBox);

    auto *dirRow = new QHBoxLayout;
    m_outputDir = new QLineEdit(this);
    m_outputDir->setPlaceholderText(QStringLiteral("Папка для извлечённых кадров"));
    auto *browseOutput = new QPushButton(QStringLiteral("Обзор…"), this);
    dirRow->addWidget(m_outputDir, 1);
    dirRow->addWidget(browseOutput);
    outputForm->addRow(QStringLiteral("Папка:"), dirRow);

    m_prefix = new QLineEdit(QStringLiteral("frame"), this);
    m_prefix->setPlaceholderText(QStringLiteral("Префикс имён файлов"));
    outputForm->addRow(QStringLiteral("Префикс:"), m_prefix);

    m_format = new QComboBox(this);
    m_format->addItem(QStringLiteral("PNG (без потерь)"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("JPG (компактнее)"), QStringLiteral("jpg"));
    outputForm->addRow(QStringLiteral("Формат:"), m_format);

    m_jpegQuality = new QSpinBox(this);
    m_jpegQuality->setRange(2, 31);
    m_jpegQuality->setValue(2);
    m_jpegQuality->setToolTip(QStringLiteral("Меньше значение — выше качество и больше размер."));
    outputForm->addRow(QStringLiteral("Качество JPG:"), m_jpegQuality);

    m_estimate = new QLabel(QStringLiteral("Кадры будут пронумерованы начиная с 000001."), this);
    m_estimate->setWordWrap(true);
    outputForm->addRow(QString(), m_estimate);

    root->addWidget(outputBox);
    root->addStretch(1);

    auto *start = new QPushButton(QStringLiteral("Извлечь все кадры"), this);
    start->setMinimumHeight(36);
    root->addWidget(start);

    connect(browseInput, &QPushButton::clicked, this, &ExtractTab::browseInput);
    connect(browseOutput, &QPushButton::clicked, this, &ExtractTab::browseOutput);
    connect(m_input, &QLineEdit::editingFinished, this, &ExtractTab::inputChanged);
    connect(m_format, &QComboBox::currentIndexChanged, this, &ExtractTab::updateFormatState);
    connect(start, &QPushButton::clicked, this, &ExtractTab::startExtraction);

    updateFormatState();
}

void ExtractTab::browseInput()
{
    const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("Выберите видео"), QString(),
                QStringLiteral("Видео (*.mp4 *.mkv *.avi *.mov *.webm *.m4v *.ts);;Все файлы (*.*)"));
    if (path.isEmpty())
        return;

    m_input->setText(QDir::toNativeSeparators(path));
    inputChanged();

    if (m_outputDir->text().trimmed().isEmpty()) {
        const QFileInfo info(path);
        m_outputDir->setText(QDir::toNativeSeparators(
                                 QDir(info.absolutePath()).filePath(info.completeBaseName() + QStringLiteral("_frames"))));
    }
}

void ExtractTab::browseOutput()
{
    const QString path = QFileDialog::getExistingDirectory(
                this, QStringLiteral("Выберите папку для кадров"),
                m_outputDir->text().trimmed());
    if (path.isEmpty())
        return;
    m_outputDir->setText(QDir::toNativeSeparators(path));
}

void ExtractTab::inputChanged()
{
    const QString path = m_input->text().trimmed();
    if (path.isEmpty()) {
        m_media = MediaInfo{};
        m_info->setText(QStringLiteral("Файл не выбран."));
        m_estimate->setText(QStringLiteral("Кадры будут пронумерованы начиная с 000001."));
        return;
    }

    Ffprobe probe(m_settings->ffprobePath);
    m_media = probe.probe(path);
    showMediaInfo();
}

void ExtractTab::showMediaInfo()
{
    if (!m_media.valid) {
        m_info->setText(m_media.error.isEmpty()
                        ? QStringLiteral("Не удалось прочитать видео.")
                        : m_media.error);
        m_estimate->setText(QStringLiteral("Кадры будут пронумерованы начиная с 000001."));
        return;
    }

    m_info->setText(mediaSummary(m_media));

    if (m_media.frameCount > 0) {
        m_estimate->setText(QStringLiteral("Ожидается примерно %1 кадров. "
                                           "Точное количество будет видно после завершения.")
                            .arg(m_media.frameCount));
    } else {
        m_estimate->setText(QStringLiteral("Количество кадров заранее неизвестно; "
                                           "будут извлечены все кадры видео."));
    }
}

void ExtractTab::updateFormatState()
{
    const bool jpeg = m_format->currentData().toString() == QStringLiteral("jpg");
    m_jpegQuality->setEnabled(jpeg);
}

void ExtractTab::startExtraction()
{
    if (m_runner->isRunning())
        return;

    const QString input = m_input->text().trimmed();
    if (input.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Извлечение кадров"),
                             QStringLiteral("Выберите видеофайл."));
        return;
    }

    Ffprobe probe(m_settings->ffprobePath);
    m_media = probe.probe(input);
    showMediaInfo();
    if (!m_media.valid) {
        QMessageBox::critical(this, QStringLiteral("Извлечение кадров"),
                              QStringLiteral("Не удалось прочитать видео:\n%1").arg(m_media.error));
        return;
    }

    const QString directory = m_outputDir->text().trimmed();
    if (directory.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Извлечение кадров"),
                             QStringLiteral("Укажите папку для кадров."));
        return;
    }

    if (!QDir().mkpath(directory)) {
        QMessageBox::critical(this, QStringLiteral("Извлечение кадров"),
                              QStringLiteral("Не удалось создать папку:\n%1").arg(directory));
        return;
    }

    QString prefix = m_prefix->text().trimmed();
    prefix.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|%]")), QStringLiteral("_"));
    if (prefix.isEmpty())
        prefix = QStringLiteral("frame");

    const QString extension = m_format->currentData().toString();
    const QDir outputDir(directory);
    if (!outputDir.entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        const auto answer = QMessageBox::question(
                    this, QStringLiteral("Извлечение кадров"),
                    QStringLiteral("В папке уже есть файлы. Существующие кадры с теми же именами "
                                   "будут перезаписаны. Продолжить?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    const QString relativePattern = prefix + QStringLiteral("_%06d.") + extension;
    const QString absolutePattern = QDir::fromNativeSeparators(
                QDir(QFileInfo(directory).absoluteFilePath()).filePath(relativePattern));

    QStringList args{
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-progress"), QStringLiteral("pipe:1"),
        QStringLiteral("-nostats"),
        QStringLiteral("-i"), input,
        QStringLiteral("-fps_mode"), QStringLiteral("passthrough")
    };

    if (extension == QStringLiteral("png"))
        args << QStringLiteral("-pred") << QStringLiteral("mixed");
    else
        args << QStringLiteral("-q:v") << QString::number(m_jpegQuality->value());

    args << absolutePattern;

    FfmpegRunner::Job job;
    job.title = QStringLiteral("Извлечение всех кадров");
    job.args = args;
    job.progressMode = m_media.frameCount > 0
            ? FfmpegRunner::ProgressMode::Frames
            : FfmpegRunner::ProgressMode::None;
    job.total = m_media.frameCount;

    m_outputDirectory = QDir(directory).absolutePath();
    m_outputPrefix = prefix + u'_';
    m_outputExtension = extension;
    m_jobId = m_runner->run(job);
}

void ExtractTab::onFinished(quint64 id, FfmpegRunner::Result result, const QString &message)
{
    if (id != m_jobId || id == 0)
        return;

    m_jobId = 0;

    if (result == FfmpegRunner::Result::Success) {
        const QDir dir(m_outputDirectory);
        const QStringList files = dir.entryList(
                    QStringList{ m_outputPrefix + QStringLiteral("*.") + m_outputExtension },
                    QDir::Files);
        QMessageBox::information(
                    this, QStringLiteral("Извлечение кадров"),
                    QStringLiteral("Кадры извлечены: %1 файлов.\nПапка:\n%2")
                    .arg(files.size())
                    .arg(QDir::toNativeSeparators(m_outputDirectory)));
    } else if (result == FfmpegRunner::Result::Failed) {
        QMessageBox::critical(this, QStringLiteral("Ошибка извлечения кадров"),
                              QStringLiteral("FFmpeg сообщил об ошибке:\n\n%1").arg(message));
    }
}
