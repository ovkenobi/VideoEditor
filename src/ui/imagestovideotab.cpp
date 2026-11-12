#include "imagestovideotab.h"

#include "appsettings.h"
#include "util/timeformat.h"

#include <QDir>
#include <QDoubleSpinBox>
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
#include <QVBoxLayout>

namespace {

int evenDimension(int value)
{
    if (value < 2)
        return 2;
    return value % 2 == 0 ? value : value - 1;
}

bool isMp4Like(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mov")
            || suffix == QStringLiteral("m4v");
}

} // namespace

ImagesToVideoTab::ImagesToVideoTab(FfmpegRunner *runner, AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_runner(runner)
    , m_settings(settings)
{
    buildUi();
    connect(m_runner, &FfmpegRunner::finished, this, &ImagesToVideoTab::onFinished);
}

void ImagesToVideoTab::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *framesBox = new QGroupBox(QStringLiteral("Исходные кадры"), this);
    auto *framesGrid = new QGridLayout(framesBox);
    framesGrid->setColumnStretch(1, 1);

    m_framesDir = new QLineEdit(this);
    m_framesDir->setPlaceholderText(QStringLiteral("Папка с изображениями frame_000001.png …"));
    auto *browseFrames = new QPushButton(QStringLiteral("Обзор…"), this);
    m_info = new QLabel(QStringLiteral("Папка не выбрана."), this);
    m_info->setWordWrap(true);
    m_info->setStyleSheet(QStringLiteral("color:#555;"));

    framesGrid->addWidget(new QLabel(QStringLiteral("Папка:"), this), 0, 0);
    framesGrid->addWidget(m_framesDir, 0, 1);
    framesGrid->addWidget(browseFrames, 0, 2);
    framesGrid->addWidget(m_info, 1, 1, 1, 2);
    root->addWidget(framesBox);

    auto *paramsBox = new QGroupBox(QStringLiteral("Параметры видео"), this);
    auto *paramsForm = new QFormLayout(paramsBox);

    m_duration = new QDoubleSpinBox(this);
    m_duration->setRange(0.01, 86400.0);
    m_duration->setDecimals(3);
    m_duration->setSingleStep(1.0);
    m_duration->setValue(10.0);
    m_duration->setSuffix(QStringLiteral(" с"));
    paramsForm->addRow(QStringLiteral("Длительность видео:"), m_duration);

    m_fpsLabel = new QLabel(this);
    m_fpsLabel->setWordWrap(true);
    paramsForm->addRow(QStringLiteral("Частота кадров:"), m_fpsLabel);

    auto *outputRow = new QHBoxLayout;
    m_output = new QLineEdit(this);
    m_output->setPlaceholderText(QStringLiteral("Выходной видеофайл"));
    auto *browseOutput = new QPushButton(QStringLiteral("Обзор…"), this);
    outputRow->addWidget(m_output, 1);
    outputRow->addWidget(browseOutput);
    paramsForm->addRow(QStringLiteral("Выходной файл:"), outputRow);

    auto *hint = new QLabel(QStringLiteral("Частота кадров рассчитывается как количество кадров, "
                                          "делённое на заданную длительность. Итоговый файл "
                                          "ограничивается этой длительностью."), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#555;"));
    paramsForm->addRow(QString(), hint);

    root->addWidget(paramsBox);
    root->addStretch(1);

    auto *start = new QPushButton(QStringLiteral("Создать видео из кадров"), this);
    start->setMinimumHeight(36);
    root->addWidget(start);

    connect(browseFrames, &QPushButton::clicked, this, &ImagesToVideoTab::browseFrames);
    connect(browseOutput, &QPushButton::clicked, this, &ImagesToVideoTab::browseOutput);
    connect(m_framesDir, &QLineEdit::editingFinished, this, &ImagesToVideoTab::scanFrames);
    connect(m_duration, &QDoubleSpinBox::valueChanged, this, [this] { updateFps(); });
    connect(start, &QPushButton::clicked, this, &ImagesToVideoTab::startEncoding);

    updateFps();
}

void ImagesToVideoTab::browseFrames()
{
    const QString path = QFileDialog::getExistingDirectory(
                this, QStringLiteral("Выберите папку с кадрами"),
                m_framesDir->text().trimmed());
    if (path.isEmpty())
        return;

    m_framesDir->setText(QDir::toNativeSeparators(path));
    scanFrames();

    if (m_output->text().trimmed().isEmpty() && m_sequence.isValid()) {
        m_output->setText(QDir::toNativeSeparators(
                              QDir(path).filePath(QStringLiteral("video.mp4"))));
    }
}

void ImagesToVideoTab::browseOutput()
{
    QString suggested = m_output->text().trimmed();
    if (suggested.isEmpty()) {
        const QString dir = m_framesDir->text().trimmed();
        suggested = QDir(dir.isEmpty() ? QDir::currentPath() : dir)
                .filePath(QStringLiteral("video.mp4"));
    }

    const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("Сохранить видео"), suggested,
                QStringLiteral("MP4 (*.mp4);;Все файлы (*.*)"));
    if (path.isEmpty())
        return;
    m_output->setText(QDir::toNativeSeparators(path));
}

void ImagesToVideoTab::scanFrames()
{
    const QString directory = m_framesDir->text().trimmed();
    if (directory.isEmpty()) {
        m_sequence = FrameSequence{};
        m_info->setText(QStringLiteral("Папка не выбрана."));
        updateFps();
        return;
    }

    QString error;
    if (!FrameSequence::scan(directory, &m_sequence, &error)) {
        m_sequence = FrameSequence{};
        m_info->setText(QStringLiteral("Ошибка: %1").arg(error));
        updateFps();
        return;
    }

    m_info->setText(QStringLiteral("Найдено кадров: %1, номера %2–%3, шаблон %4, размер %5×%6.")
                    .arg(m_sequence.count)
                    .arg(m_sequence.firstNumber)
                    .arg(m_sequence.lastNumber())
                    .arg(m_sequence.displayPattern())
                    .arg(m_sequence.frameSize.width())
                    .arg(m_sequence.frameSize.height()));
    updateFps();
}

double ImagesToVideoTab::computedFps() const
{
    if (!m_sequence.isValid())
        return 0.0;
    const double duration = m_duration->value();
    if (duration <= 0.0)
        return 0.0;
    return double(m_sequence.count) / duration;
}

void ImagesToVideoTab::updateFps()
{
    if (!m_sequence.isValid()) {
        m_fpsLabel->setText(QStringLiteral("Сначала выберите папку с кадрами."));
        return;
    }

    const double fps = computedFps();
    m_fpsLabel->setText(QStringLiteral("%1 кадр/с (кадров: %2, длительность: %3)")
                        .arg(formatFps(fps))
                        .arg(m_sequence.count)
                        .arg(formatDuration(m_duration->value())));
}

void ImagesToVideoTab::startEncoding()
{
    if (m_runner->isRunning())
        return;

    scanFrames();
    if (!m_sequence.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Кадры → видео"),
                             QStringLiteral("Выберите папку с непрерывной последовательностью кадров."));
        return;
    }

    const double duration = m_duration->value();
    if (duration <= 0.0) {
        QMessageBox::warning(this, QStringLiteral("Кадры → видео"),
                             QStringLiteral("Укажите положительную длительность видео."));
        return;
    }

    const double fps = computedFps();
    if (fps < 0.01) {
        QMessageBox::warning(this, QStringLiteral("Кадры → видео"),
                             QStringLiteral("Слишком маленькая частота кадров (%1). "
                                            "Уменьшите длительность.").arg(formatFps(fps)));
        return;
    }

    if (fps > 240.0) {
        const auto answer = QMessageBox::question(
                    this, QStringLiteral("Кадры → видео"),
                    QStringLiteral("Получается очень высокая частота кадров: %1 fps.\n"
                                   "Продолжить?").arg(formatFps(fps)),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    QString output = m_output->text().trimmed();
    if (output.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Кадры → видео"),
                             QStringLiteral("Укажите выходной видеофайл."));
        return;
    }
    if (QFileInfo(output).suffix().isEmpty()) {
        output += QStringLiteral(".mp4");
        m_output->setText(QDir::toNativeSeparators(output));
    }

    const QString absoluteOutput = QFileInfo(output).absoluteFilePath();
    const QFileInfo outputInfo(absoluteOutput);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        QMessageBox::critical(this, QStringLiteral("Кадры → видео"),
                              QStringLiteral("Не удалось создать папку для результата."));
        return;
    }

    if (outputInfo.exists()) {
        const auto answer = QMessageBox::question(
                    this, QStringLiteral("Кадры → видео"),
                    QStringLiteral("Файл «%1» уже существует. Перезаписать?").arg(outputInfo.fileName()),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    const int width = evenDimension(m_sequence.frameSize.width());
    const int height = evenDimension(m_sequence.frameSize.height());
    const QString fpsText = QString::number(fps, 'f', 6);

    QStringList args{
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-progress"), QStringLiteral("pipe:1"),
        QStringLiteral("-nostats")
    };

    if (m_sequence.count == 1) {
        // A single still image has to be looped for the requested duration.
        args << QStringLiteral("-loop") << QStringLiteral("1")
             << QStringLiteral("-framerate") << QStringLiteral("25")
             << QStringLiteral("-i") << m_sequence.fileName(m_sequence.firstNumber);
    } else {
        args << QStringLiteral("-framerate") << fpsText
             << QStringLiteral("-start_number") << QString::number(m_sequence.firstNumber)
             << QStringLiteral("-i") << m_sequence.pattern();
    }

    args << QStringLiteral("-vf")
         << QStringLiteral("scale=%1:%2:force_original_aspect_ratio=decrease,"
                           "pad=%1:%2:(ow-iw)/2:(oh-ih)/2:color=black,format=yuv420p")
            .arg(width)
            .arg(height)
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("medium")
         << QStringLiteral("-crf") << QStringLiteral("20")
         << QStringLiteral("-t") << QString::number(duration, 'f', 6);

    if (isMp4Like(absoluteOutput))
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart");

    args << absoluteOutput;

    FfmpegRunner::Job job;
    job.title = QStringLiteral("Создание видео из кадров");
    job.args = args;
    job.progressMode = FfmpegRunner::ProgressMode::Time;
    job.total = qRound64(duration * 1000.0);
    job.filesToRemoveOnFailure << absoluteOutput;

    m_outputPath = absoluteOutput;
    m_usedDuration = duration;
    m_usedFps = fps;
    m_jobId = m_runner->run(job);
}

void ImagesToVideoTab::onFinished(quint64 id, FfmpegRunner::Result result, const QString &message)
{
    if (id != m_jobId || id == 0)
        return;

    m_jobId = 0;

    if (result == FfmpegRunner::Result::Success) {
        QMessageBox::information(
                    this, QStringLiteral("Кадры → видео"),
                    QStringLiteral("Видео создано:\n%1\n\nДлительность: %2\n"
                                   "Частота: %3 fps\nКадров: %4")
                    .arg(m_outputPath)
                    .arg(formatDuration(m_usedDuration))
                    .arg(formatFps(m_usedFps))
                    .arg(m_sequence.count));
    } else if (result == FfmpegRunner::Result::Failed) {
        QMessageBox::critical(this, QStringLiteral("Ошибка создания видео"),
                              QStringLiteral("FFmpeg сообщил об ошибке:\n\n%1").arg(message));
    }
}
