#include "overlaytab.h"

#include "appsettings.h"

#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

class PreviewWidget : public QWidget
{
public:
    explicit PreviewWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedSize(480, 270);
    }

    void setMessage(const QString &message)
    {
        m_image = QImage{};
        m_message = message;
        m_showOverlayRect = false;
        setFixedSize(480, 270);
        update();
    }

    void setImage(const QImage &image)
    {
        m_image = image;
        m_message.clear();
        update();
    }

    void setScale(double scale)
    {
        if (m_image.isNull())
            return;

        m_scale = qBound(0.01, scale, 16.0);
        const QSize size(qMax(1, qRound(m_image.width() * m_scale)),
                         qMax(1, qRound(m_image.height() * m_scale)));
        setFixedSize(size);
        update();
    }

    void setOverlayRect(const QRect &rect, bool visible)
    {
        m_overlayRect = rect;
        m_showOverlayRect = visible;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.fillRect(rect(), QColor(32, 32, 32));

        if (m_image.isNull()) {
            painter.setPen(QColor(170, 170, 170));
            painter.drawText(rect(), Qt::AlignCenter, m_message);
            return;
        }

        painter.setRenderHint(QPainter::SmoothPixmapTransform, m_scale < 1.0);
        painter.save();
        painter.scale(m_scale, m_scale);
        painter.drawImage(QPointF(0, 0), m_image);
        painter.restore();

        if (m_showOverlayRect) {
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setPen(QPen(QColor(255, 70, 70), 1, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(m_overlayRect.adjusted(0, 0, -1, -1));
        }
    }

private:
    QImage m_image;
    QString m_message;
    QRect m_overlayRect;
    bool m_showOverlayRect = false;
    double m_scale = 1.0;
};

OverlayTab::OverlayTab(FfmpegRunner *runner, AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_runner(runner)
    , m_settings(settings)
{
    buildUi();
    connect(m_runner, &FfmpegRunner::finished, this, &OverlayTab::onFinished);
}

void OverlayTab::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *framesBox = new QGroupBox(QStringLiteral("Извлечённые кадры"), this);
    auto *framesGrid = new QGridLayout(framesBox);
    framesGrid->setColumnStretch(1, 1);

    m_framesDir = new QLineEdit(this);
    m_framesDir->setPlaceholderText(QStringLiteral("Папка с кадрами frame_000001.png …"));
    auto *browseFrames = new QPushButton(QStringLiteral("Обзор…"), this);
    m_info = new QLabel(QStringLiteral("Папка не выбрана."), this);
    m_info->setWordWrap(true);
    m_info->setStyleSheet(QStringLiteral("color:#555;"));

    framesGrid->addWidget(new QLabel(QStringLiteral("Папка:"), this), 0, 0);
    framesGrid->addWidget(m_framesDir, 0, 1);
    framesGrid->addWidget(browseFrames, 0, 2);
    framesGrid->addWidget(m_info, 1, 1, 1, 2);
    root->addWidget(framesBox);

    auto *overlayBox = new QGroupBox(QStringLiteral("Наложение"), this);
    auto *overlayLayout = new QVBoxLayout(overlayBox);

    auto *overlayRow = new QHBoxLayout;
    m_overlayPath = new QLineEdit(this);
    m_overlayPath->setPlaceholderText(QStringLiteral("Изображение PNG/JPG для наложения"));
    auto *browseOverlay = new QPushButton(QStringLiteral("Обзор…"), this);
    overlayRow->addWidget(new QLabel(QStringLiteral("Изображение:"), this));
    overlayRow->addWidget(m_overlayPath, 1);
    overlayRow->addWidget(browseOverlay);
    overlayLayout->addLayout(overlayRow);

    m_overlayInfo = new QLabel(QStringLiteral("Изображение не выбрано."), this);
    m_overlayInfo->setWordWrap(true);
    m_overlayInfo->setStyleSheet(QStringLiteral("color:#555;"));
    overlayLayout->addWidget(m_overlayInfo);

    auto *zoomRow = new QHBoxLayout;
    zoomRow->addWidget(new QLabel(QStringLiteral("Масштаб превью:"), this));

    auto *zoomOut = new QToolButton(this);
    zoomOut->setText(QStringLiteral("−"));
    zoomOut->setAutoRaise(true);
    zoomOut->setToolTip(QStringLiteral("Уменьшить масштаб"));

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(5, 800);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setSingleStep(5);
    m_zoomSlider->setPageStep(25);
    m_zoomSlider->setMinimumWidth(160);

    m_zoomSpin = new QSpinBox(this);
    m_zoomSpin->setRange(5, 800);
    m_zoomSpin->setValue(100);
    m_zoomSpin->setSuffix(QStringLiteral(" %"));
    m_zoomSpin->setToolTip(QStringLiteral("Масштаб превью относительно реального размера кадра"));

    auto *zoomIn = new QToolButton(this);
    zoomIn->setText(QStringLiteral("+"));
    zoomIn->setAutoRaise(true);
    zoomIn->setToolTip(QStringLiteral("Увеличить масштаб"));

    m_fitButton = new QToolButton(this);
    m_fitButton->setText(QStringLiteral("Вписать"));
    m_fitButton->setCheckable(true);
    m_fitButton->setChecked(true);
    m_fitButton->setToolTip(QStringLiteral("Вписать кадр в окно предпросмотра"));

    m_actualSizeButton = new QToolButton(this);
    m_actualSizeButton->setText(QStringLiteral("100%"));
    m_actualSizeButton->setToolTip(QStringLiteral("Показать кадр пиксель в пиксель"));

    zoomRow->addWidget(zoomOut);
    zoomRow->addWidget(m_zoomSlider, 1);
    zoomRow->addWidget(zoomIn);
    zoomRow->addWidget(m_zoomSpin);
    zoomRow->addWidget(m_fitButton);
    zoomRow->addWidget(m_actualSizeButton);
    overlayLayout->addLayout(zoomRow);

    m_previewScroll = new QScrollArea(this);
    m_previewScroll->setWidgetResizable(false);
    m_previewScroll->setAlignment(Qt::AlignCenter);
    m_previewScroll->setMinimumHeight(320);
    m_previewScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewScroll->setStyleSheet(
                QStringLiteral("QScrollArea { border:1px solid #999; background:#202020; }"));

    m_preview = new PreviewWidget(m_previewScroll);
    m_preview->setMessage(QStringLiteral("Предпросмотр кадра"));
    m_previewScroll->setWidget(m_preview);
    overlayLayout->addWidget(m_previewScroll, 1);

    m_previewScroll->viewport()->installEventFilter(this);
    m_preview->installEventFilter(this);

    auto *coords = new QFormLayout;
    auto *xRow = new QHBoxLayout;
    m_x = new QSpinBox(this);
    m_y = new QSpinBox(this);
    m_x->setRange(0, 0);
    m_y->setRange(0, 0);
    xRow->addWidget(new QLabel(QStringLiteral("X:"), this));
    xRow->addWidget(m_x);
    xRow->addSpacing(12);
    xRow->addWidget(new QLabel(QStringLiteral("Y:"), this));
    xRow->addWidget(m_y);
    xRow->addStretch(1);
    coords->addRow(QStringLiteral("Координаты:"), xRow);

    auto *rangeRow = new QHBoxLayout;
    m_startFrame = new QSpinBox(this);
    m_frameCount = new QSpinBox(this);
    m_startFrame->setRange(0, 0);
    m_frameCount->setRange(1, 1);
    rangeRow->addWidget(new QLabel(QStringLiteral("Первый кадр:"), this));
    rangeRow->addWidget(m_startFrame);
    rangeRow->addSpacing(12);
    rangeRow->addWidget(new QLabel(QStringLiteral("Количество кадров:"), this));
    rangeRow->addWidget(m_frameCount);
    rangeRow->addStretch(1);
    coords->addRow(QStringLiteral("Диапазон:"), rangeRow);
    overlayLayout->addLayout(coords);

    root->addWidget(overlayBox);

    auto *outputBox = new QGroupBox(QStringLiteral("Результат"), this);
    auto *outputLayout = new QHBoxLayout(outputBox);
    m_outputDir = new QLineEdit(this);
    m_outputDir->setPlaceholderText(QStringLiteral("Папка для кадров с наложением"));
    auto *browseOutput = new QPushButton(QStringLiteral("Обзор…"), this);
    outputLayout->addWidget(new QLabel(QStringLiteral("Папка:"), this));
    outputLayout->addWidget(m_outputDir, 1);
    outputLayout->addWidget(browseOutput);
    root->addWidget(outputBox);

    auto *hint = new QLabel(QStringLiteral("Наложение применяется к кадрам начиная с указанного "
                                          "номера включительно. Исходные кадры не изменяются: "
                                          "результат сохраняется в отдельную папку."), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#555;"));
    root->addWidget(hint);

    auto *start = new QPushButton(QStringLiteral("Наложить изображение"), this);
    start->setMinimumHeight(36);
    root->addWidget(start);

    connect(browseFrames, &QPushButton::clicked, this, &OverlayTab::browseFrames);
    connect(browseOverlay, &QPushButton::clicked, this, &OverlayTab::browseOverlay);
    connect(browseOutput, &QPushButton::clicked, this, &OverlayTab::browseOutput);
    connect(m_framesDir, &QLineEdit::editingFinished, this, &OverlayTab::scanFrames);
    connect(m_overlayPath, &QLineEdit::editingFinished, this, &OverlayTab::loadOverlay);
    connect(m_startFrame, &QSpinBox::valueChanged, this, [this] {
        updateRange();
        updatePreview();
    });
    connect(m_x, &QSpinBox::valueChanged, this, [this] { updatePreview(); });
    connect(m_y, &QSpinBox::valueChanged, this, [this] { updatePreview(); });
    connect(zoomOut, &QToolButton::clicked, this, &OverlayTab::zoomOut);
    connect(zoomIn, &QToolButton::clicked, this, &OverlayTab::zoomIn);
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_updatingZoomControls)
            return;
        setZoomPercent(value);
    });
    connect(m_zoomSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_updatingZoomControls)
            return;
        setZoomPercent(value);
    });
    connect(m_fitButton, &QToolButton::clicked, this, &OverlayTab::fitPreview);
    connect(m_actualSizeButton, &QToolButton::clicked, this, &OverlayTab::zoomActualSize);
    connect(start, &QPushButton::clicked, this, &OverlayTab::startOverlay);
}

void OverlayTab::browseFrames()
{
    const QString path = QFileDialog::getExistingDirectory(
                this, QStringLiteral("Выберите папку с кадрами"),
                m_framesDir->text().trimmed());
    if (path.isEmpty())
        return;

    m_framesDir->setText(QDir::toNativeSeparators(path));
    scanFrames();
}

void OverlayTab::browseOverlay()
{
    const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("Выберите изображение для наложения"), QString(),
                QStringLiteral("Изображения (*.png *.jpg *.jpeg *.bmp);;Все файлы (*.*)"));
    if (path.isEmpty())
        return;

    m_overlayPath->setText(QDir::toNativeSeparators(path));
    loadOverlay();
}

void OverlayTab::browseOutput()
{
    const QString path = QFileDialog::getExistingDirectory(
                this, QStringLiteral("Выберите папку для результата"),
                m_outputDir->text().trimmed());
    if (path.isEmpty())
        return;
    m_outputDir->setText(QDir::toNativeSeparators(path));
}

void OverlayTab::scanFrames()
{
    const QString directory = m_framesDir->text().trimmed();
    const QSize previousFrameSize = m_sequence.frameSize;
    m_cachedFrameNumber = -1;
    m_frameImage = QImage{};
    m_composite = QImage{};

    if (directory.isEmpty()) {
        m_sequence = FrameSequence{};
        m_fitMode = true;
        m_fitButton->setChecked(true);
        m_info->setText(QStringLiteral("Папка не выбрана."));
        updatePreview();
        return;
    }

    QString error;
    if (!FrameSequence::scan(directory, &m_sequence, &error)) {
        m_sequence = FrameSequence{};
        m_fitMode = true;
        m_fitButton->setChecked(true);
        m_info->setText(QStringLiteral("Ошибка: %1").arg(error));
        updatePreview();
        return;
    }

    if (m_sequence.frameSize != previousFrameSize) {
        m_fitMode = true;
        m_fitButton->setChecked(true);
    }

    m_info->setText(QStringLiteral("Найдено кадров: %1, номера %2–%3, шаблон %4, размер %5×%6.")
                    .arg(m_sequence.count)
                    .arg(m_sequence.firstNumber)
                    .arg(m_sequence.lastNumber())
                    .arg(m_sequence.displayPattern())
                    .arg(m_sequence.frameSize.width())
                    .arg(m_sequence.frameSize.height()));

    m_startFrame->blockSignals(true);
    m_startFrame->setRange(m_sequence.firstNumber, m_sequence.lastNumber());
    m_startFrame->setValue(m_sequence.firstNumber);
    m_startFrame->blockSignals(false);

    m_frameCount->blockSignals(true);
    m_frameCount->setRange(1, m_sequence.count);
    m_frameCount->setValue(m_sequence.count);
    m_frameCount->blockSignals(false);

    m_x->setRange(0, qMax(0, m_sequence.frameSize.width() - 1));
    m_y->setRange(0, qMax(0, m_sequence.frameSize.height() - 1));

    if (m_outputDir->text().trimmed().isEmpty()) {
        m_outputDir->setText(QDir::toNativeSeparators(
                                 QDir(directory).filePath(QStringLiteral("overlayed"))));
    }

    updatePreview();
}

void OverlayTab::loadOverlay()
{
    const QString path = m_overlayPath->text().trimmed();
    m_overlay = QImage{};

    if (path.isEmpty()) {
        m_overlayInfo->setText(QStringLiteral("Изображение не выбрано."));
        updatePreview();
        return;
    }

    m_overlay = QImage(path);
    if (m_overlay.isNull()) {
        m_overlayInfo->setText(QStringLiteral("Не удалось прочитать изображение."));
        updatePreview();
        return;
    }

    m_overlayInfo->setText(QStringLiteral("Размер наложения: %1×%2.")
                           .arg(m_overlay.width())
                           .arg(m_overlay.height()));
    updatePreview();
}

void OverlayTab::updateRange()
{
    if (!m_sequence.isValid())
        return;

    const int available = m_sequence.lastNumber() - m_startFrame->value() + 1;
    m_frameCount->setMaximum(qMax(1, available));
    if (m_frameCount->value() > available)
        m_frameCount->setValue(available);
}

void OverlayTab::updatePreview()
{
    m_composite = QImage{};

    if (!m_sequence.isValid()) {
        m_preview->setMessage(QStringLiteral("Предпросмотр кадра"));
        m_effectiveScale = 1.0;
        updateZoomControls(100);
        return;
    }

    const int frameNumber = m_startFrame->value();
    if (m_cachedFrameNumber != frameNumber || m_frameImage.isNull()) {
        m_frameImage = QImage(m_sequence.fileName(frameNumber));
        m_cachedFrameNumber = frameNumber;
    }

    if (m_frameImage.isNull()) {
        m_preview->setMessage(QStringLiteral("Не удалось открыть кадр"));
        m_effectiveScale = 1.0;
        updateZoomControls(100);
        return;
    }

    m_composite = m_frameImage.convertToFormat(QImage::Format_ARGB32);
    if (!m_overlay.isNull()) {
        QPainter painter(&m_composite);
        painter.drawImage(QPoint(m_x->value(), m_y->value()), m_overlay);
    }

    renderPreview();
}

void OverlayTab::renderPreview()
{
    if (m_composite.isNull()) {
        m_preview->setMessage(m_sequence.isValid()
                              ? QStringLiteral("Не удалось открыть кадр")
                              : QStringLiteral("Предпросмотр кадра"));
        m_effectiveScale = 1.0;
        updateZoomControls(100);
        return;
    }

    const QSize imageSize = m_composite.size();
    QSize viewportSize = m_previewScroll->viewport()->size();
    viewportSize.setWidth(qMax(40, viewportSize.width()));
    viewportSize.setHeight(qMax(40, viewportSize.height()));

    double scale = 1.0;
    if (m_fitMode) {
        const double scaleX = double(viewportSize.width()) / imageSize.width();
        const double scaleY = double(viewportSize.height()) / imageSize.height();
        scale = qMin(1.0, qMin(scaleX, scaleY));
    } else {
        scale = m_zoom;
    }
    scale = qBound(0.01, scale, 16.0);
    m_effectiveScale = scale;

    m_preview->setImage(m_composite);
    m_preview->setScale(scale);

    if (!m_overlay.isNull()) {
        const QRect rect(qRound(m_x->value() * scale),
                         qRound(m_y->value() * scale),
                         qMax(1, qRound(m_overlay.width() * scale)),
                         qMax(1, qRound(m_overlay.height() * scale)));
        m_preview->setOverlayRect(rect, true);
    } else {
        m_preview->setOverlayRect(QRect(), false);
    }

    updateZoomControls(qRound(scale * 100.0));
}

void OverlayTab::fitPreview()
{
    m_fitMode = true;
    m_fitButton->setChecked(true);
    renderPreview();
}

void OverlayTab::zoomActualSize()
{
    setZoomPercent(100);
}

void OverlayTab::zoomIn()
{
    setZoomPercent(qMin(800, currentZoomPercent() + 25));
}

void OverlayTab::zoomOut()
{
    setZoomPercent(qMax(5, currentZoomPercent() - 25));
}

void OverlayTab::setZoomPercent(int percent)
{
    percent = qBound(5, 800, percent);
    m_fitMode = false;
    m_zoom = percent / 100.0;
    m_fitButton->setChecked(false);
    renderPreview();
}

void OverlayTab::updateZoomControls(int percent)
{
    percent = qBound(5, 800, percent);
    m_updatingZoomControls = true;
    m_zoomSlider->setValue(percent);
    m_zoomSpin->setValue(percent);
    m_updatingZoomControls = false;
}

int OverlayTab::currentZoomPercent() const
{
    const double scale = m_fitMode ? m_effectiveScale : m_zoom;
    return qRound(scale * 100.0);
}

bool OverlayTab::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_previewScroll->viewport() && event->type() == QEvent::Resize) {
        if (m_fitMode)
            renderPreview();
    } else if (event->type() == QEvent::Wheel
               && (watched == m_previewScroll->viewport() || watched == m_preview)) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
            const int delta = wheel->angleDelta().y();
            if (delta > 0)
                zoomIn();
            else if (delta < 0)
                zoomOut();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void OverlayTab::startOverlay()
{
    if (m_runner->isRunning())
        return;

    // Re-scan the folder to pick up changes, but keep the user's frame range.
    const QString previousDirectory = m_framesDir->text().trimmed();
    const int previousStart = m_startFrame->value();
    const int previousCount = m_frameCount->value();
    scanFrames();
    if (m_sequence.isValid()
            && previousDirectory == m_framesDir->text().trimmed()
            && previousStart >= m_sequence.firstNumber
            && previousStart <= m_sequence.lastNumber()) {
        m_startFrame->setValue(previousStart);
        const int available = m_sequence.lastNumber() - previousStart + 1;
        m_frameCount->setValue(qBound(1, previousCount, available));
        updatePreview();
    }

    if (!m_sequence.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Наложение изображения"),
                             QStringLiteral("Выберите папку с непрерывной последовательностью кадров."));
        return;
    }

    const QString overlayPath = m_overlayPath->text().trimmed();
    if (overlayPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Наложение изображения"),
                             QStringLiteral("Выберите изображение для наложения."));
        return;
    }

    const QImage overlay(overlayPath);
    if (overlay.isNull()) {
        QMessageBox::critical(this, QStringLiteral("Наложение изображения"),
                              QStringLiteral("Не удалось прочитать изображение:\n%1").arg(overlayPath));
        return;
    }

    const QString outputDirectory = m_outputDir->text().trimmed();
    if (outputDirectory.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Наложение изображения"),
                             QStringLiteral("Укажите папку для результата."));
        return;
    }

    const QString absoluteInput = QDir(m_sequence.directory).absolutePath();
    const QString absoluteOutput = QDir(outputDirectory).absolutePath();
    if (absoluteInput.compare(absoluteOutput, Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this, QStringLiteral("Наложение изображения"),
                             QStringLiteral("Папка результата должна отличаться от папки с исходными "
                                            "кадрами."));
        return;
    }

    if (!QDir().mkpath(absoluteOutput)) {
        QMessageBox::critical(this, QStringLiteral("Наложение изображения"),
                              QStringLiteral("Не удалось создать папку результата."));
        return;
    }

    if (!QDir(absoluteOutput).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        const auto answer = QMessageBox::question(
                    this, QStringLiteral("Наложение изображения"),
                    QStringLiteral("В папке результата уже есть файлы. Существующие кадры с теми же "
                                   "именами будут перезаписаны. Продолжить?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    const int startNumber = m_startFrame->value();
    const int count = m_frameCount->value();
    const int firstFilterFrame = startNumber - m_sequence.firstNumber;
    const int lastFilterFrame = firstFilterFrame + count - 1;

    const QString outName = m_sequence.prefix
            + QStringLiteral("%0") + QString::number(m_sequence.padWidth) + QStringLiteral("d.")
            + m_sequence.extension;
    const QString outputPattern = QDir::fromNativeSeparators(
                QDir(absoluteOutput).filePath(outName));

    QStringList args{
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-progress"), QStringLiteral("pipe:1"),
        QStringLiteral("-nostats"),
        QStringLiteral("-framerate"), QStringLiteral("25"),
        QStringLiteral("-start_number"), QString::number(m_sequence.firstNumber),
        QStringLiteral("-i"), m_sequence.pattern(),
        QStringLiteral("-i"), overlayPath,
        QStringLiteral("-filter_complex"),
        QStringLiteral("[0:v]format=rgba[base];"
                       "[1:v]format=rgba[ov];"
                       "[base][ov]overlay=x=%1:y=%2:enable='between(n,%3,%4)':format=auto,"
                       "format=rgb24")
            .arg(m_x->value())
            .arg(m_y->value())
            .arg(firstFilterFrame)
            .arg(lastFilterFrame),
        QStringLiteral("-fps_mode"), QStringLiteral("passthrough"),
        QStringLiteral("-start_number"), QString::number(m_sequence.firstNumber)
    };

    if (m_sequence.extension == QStringLiteral("png"))
        args << QStringLiteral("-pred") << QStringLiteral("mixed");
    else
        args << QStringLiteral("-q:v") << QStringLiteral("2");

    args << outputPattern;

    FfmpegRunner::Job job;
    job.title = QStringLiteral("Наложение изображения на кадры");
    job.args = args;
    job.progressMode = FfmpegRunner::ProgressMode::Frames;
    job.total = m_sequence.count;

    m_outputDirectory = absoluteOutput;
    m_jobId = m_runner->run(job);
}

void OverlayTab::onFinished(quint64 id, FfmpegRunner::Result result, const QString &message)
{
    if (id != m_jobId || id == 0)
        return;

    m_jobId = 0;

    if (result == FfmpegRunner::Result::Success) {
        const QDir dir(m_outputDirectory);
        const QStringList files = dir.entryList(
                    QStringList{ m_sequence.prefix + QStringLiteral("*.") + m_sequence.extension },
                    QDir::Files);
        QMessageBox::information(
                    this, QStringLiteral("Наложение изображения"),
                    QStringLiteral("Готово. Кадров в результате: %1.\nПапка:\n%2")
                    .arg(files.size())
                    .arg(QDir::toNativeSeparators(m_outputDirectory)));
    } else if (result == FfmpegRunner::Result::Failed) {
        QMessageBox::critical(this, QStringLiteral("Ошибка наложения"),
                              QStringLiteral("FFmpeg сообщил об ошибке:\n\n%1").arg(message));
    }
}
