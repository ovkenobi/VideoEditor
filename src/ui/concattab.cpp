#include "concattab.h"

#include "appsettings.h"
#include "ffmpeg/ffprobe.h"
#include "util/timeformat.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
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
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QVBoxLayout>

namespace {

int evenDimension(int value)
{
    if (value < 2)
        return 2;
    return value % 2 == 0 ? value : value - 1;
}

QString fpsExpression(const MediaInfo &info)
{
    if (!info.fpsExpr.isEmpty() && info.fpsExpr != QStringLiteral("0/0"))
        return info.fpsExpr;
    const double fps = info.fps > 0.0 ? info.fps : 30.0;
    return QString::number(fps, 'f', 6);
}

bool isMp4Like(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mov")
            || suffix == QStringLiteral("m4v");
}

QString escapeConcatPath(QString path)
{
    path = QDir::fromNativeSeparators(path);
    path.replace(QStringLiteral("'"), QStringLiteral("'\\''"));
    return path;
}

} // namespace

ConcatTab::ConcatTab(FfmpegRunner *runner, AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_runner(runner)
    , m_settings(settings)
{
    buildUi();
    connect(m_runner, &FfmpegRunner::finished, this, &ConcatTab::onFinished);
}

void ConcatTab::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *sourceBox = new QGroupBox(QStringLiteral("Исходные видео"), this);
    auto *sourceGrid = new QGridLayout(sourceBox);
    sourceGrid->setColumnStretch(1, 1);

    m_inputA = new QLineEdit(this);
    m_inputA->setPlaceholderText(QStringLiteral("Первый видеофайл"));
    auto *browseA = new QPushButton(QStringLiteral("Обзор…"), this);
    m_infoA = new QLabel(QStringLiteral("Файл не выбран."), this);
    m_infoA->setWordWrap(true);
    m_infoA->setStyleSheet(QStringLiteral("color:#555;"));

    m_inputB = new QLineEdit(this);
    m_inputB->setPlaceholderText(QStringLiteral("Второй видеофайл"));
    auto *browseB = new QPushButton(QStringLiteral("Обзор…"), this);
    m_infoB = new QLabel(QStringLiteral("Файл не выбран."), this);
    m_infoB->setWordWrap(true);
    m_infoB->setStyleSheet(QStringLiteral("color:#555;"));

    auto *swap = new QPushButton(QStringLiteral("Поменять местами"), this);

    sourceGrid->addWidget(new QLabel(QStringLiteral("Видео 1:"), this), 0, 0);
    sourceGrid->addWidget(m_inputA, 0, 1);
    sourceGrid->addWidget(browseA, 0, 2);
    sourceGrid->addWidget(m_infoA, 1, 1, 1, 2);
    sourceGrid->addWidget(new QLabel(QStringLiteral("Видео 2:"), this), 2, 0);
    sourceGrid->addWidget(m_inputB, 2, 1);
    sourceGrid->addWidget(browseB, 2, 2);
    sourceGrid->addWidget(m_infoB, 3, 1, 1, 2);
    sourceGrid->addWidget(swap, 4, 1, 1, 1);
    root->addWidget(sourceBox);

    auto *resultBox = new QGroupBox(QStringLiteral("Результат"), this);
    auto *resultForm = new QFormLayout(resultBox);

    auto *outputRow = new QHBoxLayout;
    m_output = new QLineEdit(this);
    m_output->setPlaceholderText(QStringLiteral("Выходной видеофайл, например C:/video/result.mp4"));
    auto *browseOutput = new QPushButton(QStringLiteral("Обзор…"), this);
    outputRow->addWidget(m_output, 1);
    outputRow->addWidget(browseOutput);
    resultForm->addRow(QStringLiteral("Выходной файл:"), outputRow);

    m_mode = new QComboBox(this);
    m_mode->addItem(QStringLiteral("Авто (рекомендуется)"));
    m_mode->addItem(QStringLiteral("Без перекодирования"));
    m_mode->addItem(QStringLiteral("С перекодированием"));
    resultForm->addRow(QStringLiteral("Режим:"), m_mode);

    m_modeInfo = new QLabel(QStringLiteral("Выберите оба видеофайла."), this);
    m_modeInfo->setWordWrap(true);
    resultForm->addRow(QString(), m_modeInfo);

    root->addWidget(resultBox);
    root->addStretch(1);

    auto *start = new QPushButton(QStringLiteral("Склеить видео"), this);
    start->setMinimumHeight(36);
    root->addWidget(start);

    connect(browseA, &QPushButton::clicked, this, &ConcatTab::browseFileA);
    connect(browseB, &QPushButton::clicked, this, &ConcatTab::browseFileB);
    connect(browseOutput, &QPushButton::clicked, this, &ConcatTab::browseOutput);
    connect(swap, &QPushButton::clicked, this, &ConcatTab::swapInputs);
    connect(start, &QPushButton::clicked, this, &ConcatTab::startConcat);
    connect(m_inputA, &QLineEdit::editingFinished, this, [this] { probeInput(false); });
    connect(m_inputB, &QLineEdit::editingFinished, this, [this] { probeInput(true); });
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this] { updateModeState(); });
}

void ConcatTab::browseFileA()
{
    const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("Выберите первое видео"), QString(),
                QStringLiteral("Видео (*.mp4 *.mkv *.avi *.mov *.webm *.m4v *.ts);;Все файлы (*.*)"));
    if (path.isEmpty())
        return;
    m_inputA->setText(QDir::toNativeSeparators(path));
    probeInput(false);
}

void ConcatTab::browseFileB()
{
    const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("Выберите второе видео"), QString(),
                QStringLiteral("Видео (*.mp4 *.mkv *.avi *.mov *.webm *.m4v *.ts);;Все файлы (*.*)"));
    if (path.isEmpty())
        return;
    m_inputB->setText(QDir::toNativeSeparators(path));
    probeInput(true);
}

void ConcatTab::browseOutput()
{
    QString suggested = m_output->text().trimmed();
    if (suggested.isEmpty()) {
        const QString first = QFileInfo(m_inputA->text().trimmed()).absolutePath();
        suggested = QDir(first).filePath(QStringLiteral("result.mp4"));
    }

    const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("Сохранить результат"), suggested,
                QStringLiteral("MP4 (*.mp4);;Все файлы (*.*)"));
    if (path.isEmpty())
        return;
    m_output->setText(QDir::toNativeSeparators(path));
}

void ConcatTab::swapInputs()
{
    const QString a = m_inputA->text();
    m_inputA->setText(m_inputB->text());
    m_inputB->setText(a);

    const MediaInfo media = m_mediaA;
    m_mediaA = m_mediaB;
    m_mediaB = media;

    m_infoA->setText(m_mediaA.valid ? mediaSummary(m_mediaA) : QStringLiteral("Файл не выбран."));
    m_infoB->setText(m_mediaB.valid ? mediaSummary(m_mediaB) : QStringLiteral("Файл не выбран."));
    updateModeState();
}

void ConcatTab::probeInput(bool second)
{
    const QLineEdit *edit = second ? m_inputB : m_inputA;
    QLabel *label = second ? m_infoB : m_infoA;
    MediaInfo &target = second ? m_mediaB : m_mediaA;

    const QString path = edit->text().trimmed();
    if (path.isEmpty()) {
        target = MediaInfo{};
        label->setText(QStringLiteral("Файл не выбран."));
        updateModeState();
        return;
    }

    Ffprobe probe(m_settings->ffprobePath);
    target = probe.probe(path);
    label->setText(mediaSummary(target));
    updateModeState();
}

bool ConcatTab::parametersMatch() const
{
    if (!m_mediaA.valid || !m_mediaB.valid)
        return false;

    if (m_mediaA.videoCodec != m_mediaB.videoCodec
            || m_mediaA.width != m_mediaB.width
            || m_mediaA.height != m_mediaB.height
            || m_mediaA.pixFmt != m_mediaB.pixFmt) {
        return false;
    }

    if (m_mediaA.hasAudio != m_mediaB.hasAudio)
        return false;

    if (m_mediaA.hasAudio) {
        if (m_mediaA.audioCodec != m_mediaB.audioCodec
                || m_mediaA.sampleRate != m_mediaB.sampleRate
                || m_mediaA.channels != m_mediaB.channels) {
            return false;
        }
    }

    return true;
}

QString ConcatTab::targetDescription() const
{
    if (!m_mediaA.valid)
        return {};

    return QStringLiteral("%1×%2, %3 fps")
            .arg(evenDimension(m_mediaA.width))
            .arg(evenDimension(m_mediaA.height))
            .arg(formatFps(m_mediaA.fps));
}

void ConcatTab::updateModeState()
{
    const bool both = m_mediaA.valid && m_mediaB.valid;
    const bool compatible = both && parametersMatch();

    if (auto *model = qobject_cast<QStandardItemModel *>(m_mode->model())) {
        if (auto *item = model->item(1)) {
            item->setEnabled(compatible);
            if (!compatible && m_mode->currentIndex() == 1)
                m_mode->setCurrentIndex(0);
        }
    }

    if (!both) {
        m_modeInfo->setText(QStringLiteral("Выберите оба видеофайла."));
        return;
    }

    if (compatible) {
        m_modeInfo->setText(QStringLiteral("Кодек, разрешение, пиксельный формат и аудио совпадают: "
                                           "возможно быстрое склеивание без перекодирования. "
                                           "Разная частота кадров этому не мешает."));
    } else {
        m_modeInfo->setText(QStringLiteral("Параметры различаются. Будет выполнено перекодирование "
                                           "с приведением к %1.").arg(targetDescription()));
    }
}

void ConcatTab::startConcat()
{
    if (m_runner->isRunning())
        return;

    const QString pathA = m_inputA->text().trimmed();
    const QString pathB = m_inputB->text().trimmed();

    if (pathA.isEmpty() || pathB.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Склейка видео"),
                             QStringLiteral("Укажите оба исходных видеофайла."));
        return;
    }

    Ffprobe probe(m_settings->ffprobePath);
    m_mediaA = probe.probe(pathA);
    m_mediaB = probe.probe(pathB);
    m_infoA->setText(mediaSummary(m_mediaA));
    m_infoB->setText(mediaSummary(m_mediaB));
    updateModeState();

    if (!m_mediaA.valid) {
        QMessageBox::critical(this, QStringLiteral("Склейка видео"),
                              QStringLiteral("Не удалось прочитать первое видео:\n%1").arg(m_mediaA.error));
        return;
    }
    if (!m_mediaB.valid) {
        QMessageBox::critical(this, QStringLiteral("Склейка видео"),
                              QStringLiteral("Не удалось прочитать второе видео:\n%1").arg(m_mediaB.error));
        return;
    }

    QString output = m_output->text().trimmed();
    if (output.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Склейка видео"),
                             QStringLiteral("Укажите выходной файл."));
        return;
    }
    if (QFileInfo(output).suffix().isEmpty()) {
        output += QStringLiteral(".mp4");
        m_output->setText(QDir::toNativeSeparators(output));
    }

    const QString absoluteA = QFileInfo(pathA).absoluteFilePath();
    const QString absoluteB = QFileInfo(pathB).absoluteFilePath();
    const QString absoluteOutput = QFileInfo(output).absoluteFilePath();
    if (absoluteOutput.compare(absoluteA, Qt::CaseInsensitive) == 0
            || absoluteOutput.compare(absoluteB, Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this, QStringLiteral("Склейка видео"),
                             QStringLiteral("Выходной файл не должен совпадать с исходным."));
        return;
    }

    const QFileInfo outputInfo(absoluteOutput);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        QMessageBox::critical(this, QStringLiteral("Склейка видео"),
                              QStringLiteral("Не удалось создать папку для результата."));
        return;
    }

    if (outputInfo.exists()) {
        const auto answer = QMessageBox::question(
                    this, QStringLiteral("Склейка видео"),
                    QStringLiteral("Файл «%1» уже существует. Перезаписать?").arg(outputInfo.fileName()),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    const bool compatible = parametersMatch();
    const int mode = m_mode->currentIndex();
    if (mode == 1 && !compatible) {
        QMessageBox::warning(this, QStringLiteral("Склейка видео"),
                             QStringLiteral("Файлы имеют разные параметры. Выберите автоматический режим "
                                            "или перекодирование."));
        return;
    }

    const bool useCopy = (mode == 0 && compatible) || (mode == 1);

    QStringList args{
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-progress"), QStringLiteral("pipe:1"),
        QStringLiteral("-nostats")
    };

    if (useCopy) {
        m_tempDir = std::make_unique<QTemporaryDir>();
        if (!m_tempDir->isValid()) {
            QMessageBox::critical(this, QStringLiteral("Склейка видео"),
                                  QStringLiteral("Не удалось создать временную папку."));
            return;
        }

        const QString listPath = m_tempDir->filePath(QStringLiteral("concat.txt"));
        QFile listFile(listPath);
        if (!listFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, QStringLiteral("Склейка видео"),
                                  QStringLiteral("Не удалось создать список файлов для FFmpeg."));
            return;
        }

        const QString content = QStringLiteral("file '%1'\nfile '%2'\n")
                .arg(escapeConcatPath(absoluteA),
                     escapeConcatPath(absoluteB));
        listFile.write(content.toUtf8());
        listFile.close();

        args << QStringLiteral("-f") << QStringLiteral("concat")
             << QStringLiteral("-safe") << QStringLiteral("0")
             << QStringLiteral("-i") << listPath
             << QStringLiteral("-map") << QStringLiteral("0:v:0")
             << QStringLiteral("-map") << QStringLiteral("0:a?")
             << QStringLiteral("-c") << QStringLiteral("copy");
        if (isMp4Like(absoluteOutput))
            args << QStringLiteral("-movflags") << QStringLiteral("+faststart");
        args << absoluteOutput;
    } else {
        args << QStringLiteral("-i") << absoluteA
             << QStringLiteral("-i") << absoluteB;

        bool hasAudio = false;
        const QStringList filters = buildReencodeFilters(hasAudio);

        // The filter graph may reference a third (silent) input.
        if (m_mediaA.hasAudio != m_mediaB.hasAudio) {
            const double silentDuration = m_mediaA.hasAudio
                    ? m_mediaB.durationSec : m_mediaA.durationSec;
            args << QStringLiteral("-f") << QStringLiteral("lavfi")
                 << QStringLiteral("-t") << QString::number(silentDuration, 'f', 6)
                 << QStringLiteral("-i")
                 << QStringLiteral("anullsrc=r=48000:cl=stereo");
        }

        args << QStringLiteral("-filter_complex") << filters.join(u';')
             << QStringLiteral("-map") << QStringLiteral("[v]");
        if (hasAudio)
            args << QStringLiteral("-map") << QStringLiteral("[a]");

        args << QStringLiteral("-c:v") << QStringLiteral("libx264")
             << QStringLiteral("-preset") << QStringLiteral("medium")
             << QStringLiteral("-crf") << QStringLiteral("20")
             << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");

        if (hasAudio)
            args << QStringLiteral("-c:a") << QStringLiteral("aac")
                 << QStringLiteral("-b:a") << QStringLiteral("192k");

        if (isMp4Like(absoluteOutput))
            args << QStringLiteral("-movflags") << QStringLiteral("+faststart");

        args << absoluteOutput;
    }

    FfmpegRunner::Job job;
    job.title = useCopy
            ? QStringLiteral("Склейка видео без перекодирования")
            : QStringLiteral("Склейка видео с перекодированием");
    job.args = args;
    job.progressMode = FfmpegRunner::ProgressMode::Time;
    job.total = qRound64((m_mediaA.durationSec + m_mediaB.durationSec) * 1000.0);
    job.filesToRemoveOnFailure << absoluteOutput;

    m_outputPath = absoluteOutput;
    m_jobId = m_runner->run(job);
}

QStringList ConcatTab::buildReencodeFilters(bool &hasAudio) const
{
    const int width = evenDimension(m_mediaA.width);
    const int height = evenDimension(m_mediaA.height);
    const QString fps = fpsExpression(m_mediaA);

    auto videoChain = [&](int index) {
        return QStringLiteral("[%1:v]scale=%2:%3:force_original_aspect_ratio=decrease,"
                              "pad=%2:%3:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=%4,format=yuv420p[v%1]")
                .arg(index)
                .arg(width)
                .arg(height)
                .arg(fps);
    };

    auto audioChain = [](int index) {
        return QStringLiteral("[%1:a]aresample=48000,"
                              "aformat=sample_fmts=fltp:channel_layouts=stereo[a%1]")
                .arg(index);
    };

    QStringList chains;
    chains << videoChain(0) << videoChain(1);
    chains << QStringLiteral("[v0][v1]concat=n=2:v=1:a=0[v]");

    const bool audioA = m_mediaA.hasAudio;
    const bool audioB = m_mediaB.hasAudio;
    hasAudio = audioA || audioB;

    if (audioA && audioB) {
        chains << audioChain(0) << audioChain(1);
        chains << QStringLiteral("[a0][a1]concat=n=2:v=0:a=1[a]");
    } else if (audioA) {
        const double silence = m_mediaB.durationSec;
        chains << audioChain(0);
        chains << QStringLiteral("[2:a]atrim=duration=%1,asetpts=PTS-STARTPTS,"
                                 "aresample=48000,"
                                 "aformat=sample_fmts=fltp:channel_layouts=stereo[a1]")
                  .arg(QString::number(silence, 'f', 6));
        chains << QStringLiteral("[a0][a1]concat=n=2:v=0:a=1[a]");
    } else if (audioB) {
        const double silence = m_mediaA.durationSec;
        chains << QStringLiteral("[2:a]atrim=duration=%1,asetpts=PTS-STARTPTS,"
                                 "aresample=48000,"
                                 "aformat=sample_fmts=fltp:channel_layouts=stereo[a0]")
                  .arg(QString::number(silence, 'f', 6));
        chains << audioChain(1);
        chains << QStringLiteral("[a0][a1]concat=n=2:v=0:a=1[a]");
    }

    return chains;
}

void ConcatTab::onFinished(quint64 id, FfmpegRunner::Result result, const QString &message)
{
    if (id != m_jobId || id == 0)
        return;

    m_jobId = 0;

    if (result == FfmpegRunner::Result::Success) {
        QMessageBox::information(this, QStringLiteral("Склейка видео"),
                                 QStringLiteral("Видео успешно склеено:\n%1").arg(m_outputPath));
    } else if (result == FfmpegRunner::Result::Failed) {
        QMessageBox::critical(this, QStringLiteral("Ошибка склейки"),
                              QStringLiteral("FFmpeg сообщил об ошибке:\n\n%1").arg(message));
    }
}
