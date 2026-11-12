#include "framesequence.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMap>
#include <QRegularExpression>

namespace {

struct Candidate
{
    QString prefix;
    QString extension;
    int padWidth = 0;
    QMap<int, QString> files;
    QSize frameSize;
};

QSize readFrameSize(const QString &path)
{
    QImageReader reader(path);
    QSize size = reader.size();
    if (size.isValid())
        return size;

    QImage image(path);
    return image.size();
}

} // namespace

bool FrameSequence::isValid() const
{
    return !directory.isEmpty() && !prefix.isEmpty() && !extension.isEmpty()
            && count > 0 && padWidth > 0 && frameSize.isValid();
}

int FrameSequence::lastNumber() const
{
    return firstNumber + count - 1;
}

QString FrameSequence::pattern() const
{
    const QString base = QDir::fromNativeSeparators(directory);
    return base + u'/' + prefix + u'%'
            + QStringLiteral("%1").arg(padWidth, 2, 10, QChar(u'0'))
            + u'd' + u'.' + extension;
}

QString FrameSequence::displayPattern() const
{
    return prefix + u'%'
            + QStringLiteral("%1").arg(padWidth, 2, 10, QChar(u'0'))
            + u'd' + u'.' + extension;
}

QString FrameSequence::fileName(int number) const
{
    return QDir(directory).filePath(prefix
                                    + QStringLiteral("%1").arg(number, padWidth, 10, QChar(u'0'))
                                    + u'.' + extension);
}

bool FrameSequence::scan(const QString &directory, FrameSequence *result, QString *error)
{
    if (result)
        *result = FrameSequence{};

    const QDir dir(directory);
    if (!dir.exists()) {
        if (error)
            *error = QStringLiteral("Папка не существует.");
        return false;
    }

    static const QRegularExpression filePattern(
                QStringLiteral("^(.*?)(\\d+)\\.(png|jpe?g|bmp)$"),
                QRegularExpression::CaseInsensitiveOption);

    QMap<QString, Candidate> candidates;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QRegularExpressionMatch match = filePattern.match(entry.fileName());
        if (!match.hasMatch())
            continue;

        const QString prefix = match.captured(1);
        const QString numberText = match.captured(2);
        const QString extension = match.captured(3).toLower();

        if (prefix.contains(u'%'))
            continue; // Such names are not supported by ffmpeg image2 patterns.

        const QString key = prefix.toLower() + u'|' + extension + u'|'
                + QString::number(numberText.size());
        Candidate &candidate = candidates[key];
        candidate.prefix = prefix;
        candidate.extension = extension;
        candidate.padWidth = numberText.size();
        candidate.files.insert(numberText.toInt(), entry.fileName());
    }

    if (candidates.isEmpty()) {
        if (error)
            *error = QStringLiteral("В папке не найдены изображения вида name_000001.png/jpg.");
        return false;
    }

    int bestRunLength = 0;
    int bestRunStart = 0;
    Candidate bestCandidate;
    QString bestStartFile;

    for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it) {
        const Candidate &candidate = it.value();
        if (candidate.files.isEmpty())
            continue;

        int currentRunStart = 0;
        int currentRunLength = 0;
        int previous = -1;
        int candidateBestStart = 0;
        int candidateBestLength = 0;

        for (auto numberIt = candidate.files.constBegin();
             numberIt != candidate.files.constEnd(); ++numberIt) {
            const int number = numberIt.key();
            if (previous < 0 || number == previous + 1) {
                if (currentRunLength == 0)
                    currentRunStart = number;
                ++currentRunLength;
            } else {
                currentRunStart = number;
                currentRunLength = 1;
            }

            if (currentRunLength > candidateBestLength) {
                candidateBestLength = currentRunLength;
                candidateBestStart = currentRunStart;
            }
            previous = number;
        }

        if (candidateBestLength > bestRunLength) {
            bestRunLength = candidateBestLength;
            bestRunStart = candidateBestStart;
            bestCandidate = candidate;
            bestStartFile = candidate.files.value(candidateBestStart);
        }
    }

    if (bestRunLength <= 0) {
        if (error)
            *error = QStringLiteral("Не удалось определить непрерывную последовательность кадров.");
        return false;
    }

    if (result) {
        result->directory = dir.absolutePath();
        result->prefix = bestCandidate.prefix;
        result->extension = bestCandidate.extension;
        result->firstNumber = bestRunStart;
        result->count = bestRunLength;
        result->padWidth = bestCandidate.padWidth;
        result->frameSize = readFrameSize(dir.filePath(bestStartFile));

        if (!result->frameSize.isValid()) {
            *result = FrameSequence{};
            if (error)
                *error = QStringLiteral("Не удалось прочитать размеры первого кадра.");
            return false;
        }
    }

    return true;
}
