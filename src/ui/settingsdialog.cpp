#include "settingsdialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString resolveTool(const QString &raw)
{
    const QString path = raw.trimmed();
    if (path.isEmpty())
        return {};

    const QFileInfo info(path);
    if (info.exists() && info.isFile())
        return info.absoluteFilePath();

    const QString found = QStandardPaths::findExecutable(path);
    if (!found.isEmpty())
        return found;

    return {};
}

} // namespace

SettingsDialog::SettingsDialog(const AppSettings &settings, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Настройки"));
    setMinimumWidth(620);

    m_ffmpegEdit = new QLineEdit(settings.ffmpegPath, this);
    m_ffprobeEdit = new QLineEdit(settings.ffprobePath, this);

    buildUi();
}

void SettingsDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    auto *ffmpegRow = new QHBoxLayout;
    auto *browseFfmpegButton = new QPushButton(QStringLiteral("Обзор…"), this);
    ffmpegRow->addWidget(m_ffmpegEdit, 1);
    ffmpegRow->addWidget(browseFfmpegButton);
    form->addRow(QStringLiteral("ffmpeg.exe:"), ffmpegRow);

    auto *ffprobeRow = new QHBoxLayout;
    auto *browseFfprobeButton = new QPushButton(QStringLiteral("Обзор…"), this);
    ffprobeRow->addWidget(m_ffprobeEdit, 1);
    ffprobeRow->addWidget(browseFfprobeButton);
    form->addRow(QStringLiteral("ffprobe.exe:"), ffprobeRow);

    auto *buttonsRow = new QHBoxLayout;
    auto *autoButton = new QPushButton(QStringLiteral("Автоопределение"), this);
    auto *checkButton = new QPushButton(QStringLiteral("Проверить ffmpeg"), this);
    buttonsRow->addWidget(autoButton);
    buttonsRow->addWidget(checkButton);
    buttonsRow->addStretch(1);
    form->addRow(QString(), buttonsRow);

    auto *hint = new QLabel(
                QStringLiteral("FFmpeg и ffprobe должны находиться рядом с программой, в PATH "
                               "или быть указаны вручную. Все операции с видео выполняет FFmpeg."),
                this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#555;"));
    form->addRow(QString(), hint);

    root->addLayout(form);
    root->addStretch(1);

    auto *buttonBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttonBox);

    connect(browseFfmpegButton, &QPushButton::clicked, this, &SettingsDialog::browseFfmpeg);
    connect(browseFfprobeButton, &QPushButton::clicked, this, &SettingsDialog::browseFfprobe);
    connect(autoButton, &QPushButton::clicked, this, &SettingsDialog::autoDetect);
    connect(checkButton, &QPushButton::clicked, this, &SettingsDialog::checkFfmpeg);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

AppSettings SettingsDialog::settings() const
{
    AppSettings result;
    result.ffmpegPath = m_ffmpegEdit->text().trimmed();
    result.ffprobePath = m_ffprobeEdit->text().trimmed();
    return result;
}

void SettingsDialog::browseTool(QLineEdit *edit, const QString &title)
{
    const QString start = edit->text().trimmed();
    const QString path = QFileDialog::getOpenFileName(
                this, title, start,
                QStringLiteral("Исполняемые файлы (*.exe);;Все файлы (*.*)"));
    if (path.isEmpty())
        return;
    edit->setText(QFileInfo(path).absoluteFilePath());
}

void SettingsDialog::browseFfmpeg()
{
    browseTool(m_ffmpegEdit, QStringLiteral("Выберите ffmpeg.exe"));
}

void SettingsDialog::browseFfprobe()
{
    browseTool(m_ffprobeEdit, QStringLiteral("Выберите ffprobe.exe"));
}

void SettingsDialog::autoDetect()
{
    m_ffmpegEdit->setText(AppSettings::defaultFfmpegPath());
    m_ffprobeEdit->setText(AppSettings::defaultFfprobePath());
}

void SettingsDialog::checkFfmpeg()
{
    checkTool(m_ffmpegEdit->text(), QStringLiteral("ffmpeg"));
}

void SettingsDialog::checkTool(const QString &path, const QString &title)
{
    const QString resolved = resolveTool(path);
    if (resolved.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Проверка FFmpeg"),
                              QStringLiteral("Не удалось найти %1 по указанному пути.").arg(title));
        return;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif

    process.start(resolved, QStringList{ QStringLiteral("-hide_banner"), QStringLiteral("-version") });
    if (!process.waitForStarted(5000)) {
        QMessageBox::critical(this, QStringLiteral("Проверка FFmpeg"),
                              QStringLiteral("Не удалось запустить %1:\n%2")
                              .arg(title, process.errorString()));
        return;
    }

    if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished(2000);
        QMessageBox::critical(this, QStringLiteral("Проверка FFmpeg"),
                              QStringLiteral("%1 не ответил за 10 секунд.").arg(title));
        return;
    }

    const QString output = QString::fromUtf8(process.readAll()).trimmed();
    const QString firstLine = output.section(u'\n', 0, 0).trimmed();

    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        QMessageBox::information(this, QStringLiteral("Проверка FFmpeg"),
                                 QStringLiteral("%1 работает.\n\n%2")
                                 .arg(title, firstLine));
    } else {
        QMessageBox::critical(this, QStringLiteral("Проверка FFmpeg"),
                              QStringLiteral("%1 завершился с ошибкой:\n\n%2")
                              .arg(title, output.left(2000)));
    }
}

void SettingsDialog::accept()
{
    const QString ffmpeg = resolveTool(m_ffmpegEdit->text());
    const QString ffprobe = resolveTool(m_ffprobeEdit->text());

    if (ffmpeg.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Настройки"),
                             QStringLiteral("Не найден ffmpeg.exe. Укажите правильный путь."));
        return;
    }
    if (ffprobe.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Настройки"),
                             QStringLiteral("Не найден ffprobe.exe. Укажите правильный путь."));
        return;
    }

    m_ffmpegEdit->setText(ffmpeg);
    m_ffprobeEdit->setText(ffprobe);
    QDialog::accept();
}
