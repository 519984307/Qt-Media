#include "mainwindow.hpp"

#include <ffmpeg/averror.h>
#include <ffmpeg/avversion.hpp>
#include <ffmpeg/ffmpegutils.hpp>
#include <ffmpeg/transcode.hpp>

#include <QtWidgets>

#define BUTTON_SIZE QSize(100, 35)

class MainWindow::MainWindowPrivate
{
public:
    MainWindowPrivate(QWidget *parent)
        : owner(parent)
    {
        transcode = new Ffmpeg::Transcode(owner);

        inTextEdit = new QTextEdit(owner);
        outTextEdit = new QTextEdit(owner);

        audioCodecCbx = new QComboBox(owner);
        audioCodecCbx->setView(new QListView(audioCodecCbx));
        audioCodecCbx->setMaxVisibleItems(10);
        audioCodecCbx->setStyleSheet("QComboBox {combobox-popup:0;}");
        for (int i = AV_CODEC_ID_MP2; i <= AV_CODEC_ID_CODEC2; i++) {
            auto codecID = static_cast<AVCodecID>(i);
            if (!Ffmpeg::Utils::isSupportAudioEncoder(codecID)) {
                continue;
            }
            audioCodecCbx->addItem(avcodec_get_name(codecID), codecID);
        }
        audioCodecCbx->setCurrentIndex(audioCodecCbx->findData(AV_CODEC_ID_AAC));
        videoCodecCbx = new QComboBox(owner);
        videoCodecCbx->setView(new QListView(videoCodecCbx));
        videoCodecCbx->setMaxVisibleItems(10);
        videoCodecCbx->setStyleSheet("QComboBox {combobox-popup:0;}");
        for (int i = AV_CODEC_ID_MPEG1VIDEO; i <= AV_CODEC_ID_VVC; i++) {
            auto codecID = static_cast<AVCodecID>(i);
            if (!Ffmpeg::Utils::isSupportVideoEncoder(codecID)) {
                continue;
            }
            videoCodecCbx->addItem(avcodec_get_name(static_cast<AVCodecID>(i)), i);
        }
        videoCodecCbx->setCurrentIndex(videoCodecCbx->findData(AV_CODEC_ID_H264));

        quailtySbx = new QSpinBox(owner);
        quailtySbx->setRange(2, 31);
        quailtySbx->setToolTip(tr("smaller -> better"));
        crfSbx = new QSpinBox(owner);
        crfSbx->setRange(0, 51);
        crfSbx->setToolTip(tr("smaller -> better"));
        crfSbx->setValue(18);
        presetCbx = new QComboBox(owner);
        presetCbx->setView(new QListView(presetCbx));
        presetCbx->addItems(transcode->presets());
        presetCbx->setCurrentText(transcode->preset());
        tuneCbx = new QComboBox(owner);
        tuneCbx->setView(new QListView(tuneCbx));
        tuneCbx->addItems(transcode->tunes());
        tuneCbx->setCurrentText(transcode->tune());
        profileCbx = new QComboBox(owner);
        profileCbx->setView(new QListView(profileCbx));
        profileCbx->addItems(transcode->profiles());
        profileCbx->setCurrentText(transcode->profile());

        widthLineEdit = new QLineEdit(owner);
        widthLineEdit->setValidator(new QIntValidator(0, INT_MAX, widthLineEdit));
        heightLineEdit = new QLineEdit(owner);
        heightLineEdit->setValidator(new QIntValidator(0, INT_MAX, heightLineEdit));
        keepAspectRatioCkb = new QCheckBox(tr("keepAspectRatio"), owner);
        keepAspectRatioCkb->setChecked(true);

        videoMinBitrateLineEdit = new QLineEdit(owner);
        videoMaxBitrateLineEdit = new QLineEdit(owner);

        startButton = new QToolButton(owner);
        startButton->setText(QObject::tr("Start"));
        startButton->setMinimumSize(BUTTON_SIZE);
        progressBar = new QProgressBar(owner);
        progressBar->setRange(0, 100);
        fpsLabel = new QLabel(owner);
        fpsLabel->setToolTip(QObject::tr("Video Encoder FPS."));
        fpsTimer = new QTimer(owner);
    }

    QGroupBox *initVideoSetting()
    {
        auto layout1 = new QHBoxLayout;
        layout1->addWidget(new QLabel(tr("Width:"), owner));
        layout1->addWidget(widthLineEdit);
        layout1->addWidget(new QLabel(tr("height:"), owner));
        layout1->addWidget(heightLineEdit);
        layout1->addWidget(keepAspectRatioCkb);
        auto layout2 = new QHBoxLayout;
        layout2->addWidget(new QLabel(tr("Min Bitrate:"), owner));
        layout2->addWidget(videoMinBitrateLineEdit);
        layout2->addWidget(new QLabel(tr("Max Bitrate:"), owner));
        layout2->addWidget(videoMaxBitrateLineEdit);

        auto groupBox = new QGroupBox(tr("Video"), owner);
        auto layout = new QVBoxLayout(groupBox);
        layout->addLayout(layout1);
        layout->addLayout(layout2);
        return groupBox;
    }

    QGroupBox *invalidSetting()
    {
        auto groupBox = new QGroupBox(tr("Invalid setting"), owner);
        auto layout = new QHBoxLayout(groupBox);
        layout->addWidget(new QLabel(tr("Quality:"), owner));
        layout->addWidget(quailtySbx);
        layout->addWidget(new QLabel(tr("Crf:"), owner));
        layout->addWidget(crfSbx);
        layout->addWidget(new QLabel(tr("Preset:"), owner));
        layout->addWidget(presetCbx);
        layout->addWidget(new QLabel(tr("Tune:"), owner));
        layout->addWidget(tuneCbx);
        layout->addWidget(new QLabel(tr("Profile:"), owner));
        layout->addWidget(profileCbx);
        return groupBox;
    }

    void initInputFileAttribute(const QString &filePath)
    {
        bool audioSet = false;
        bool videoSet = false;
        auto codecs = Ffmpeg::Utils::getFileCodecInfo(filePath);
        for (const auto &codec : qAsConst(codecs)) {
            if (audioSet && videoSet) {
                break;
            }

            switch (codec.mediaType) {
            case AVMEDIA_TYPE_AUDIO:
                if (!audioSet) {
                    auto index = audioCodecCbx->findData(codec.codecID);
                    if (index > 0) {
                        audioCodecCbx->setCurrentIndex(index);
                        audioSet = true;
                    }
                }
                break;
            case AVMEDIA_TYPE_VIDEO:
                if (!videoSet) {
                    auto index = videoCodecCbx->findData(codec.codecID);
                    if (index > 0) {
                        videoCodecCbx->setCurrentIndex(index);
                        videoSet = true;
                    }
                    widthLineEdit->blockSignals(true);
                    heightLineEdit->blockSignals(true);
                    widthLineEdit->setText(QString::number(codec.size.width()));
                    heightLineEdit->setText(QString::number(codec.size.height()));
                    originalSize = codec.size;
                    widthLineEdit->blockSignals(false);
                    heightLineEdit->blockSignals(false);
                    calBitrate();
                }
                break;
            default: break;
            }
        }
    }

    void calBitrate()
    {
        auto w = widthLineEdit->text().toInt();
        auto h = heightLineEdit->text().toInt();
        videoMinBitrateLineEdit->setText(QString::number(w * h));
        videoMaxBitrateLineEdit->setText(QString::number(w * h * 4));
    }

    QWidget *owner;

    Ffmpeg::Transcode *transcode;

    QTextEdit *inTextEdit;
    QTextEdit *outTextEdit;

    QComboBox *audioCodecCbx;
    QComboBox *videoCodecCbx;
    QSpinBox *quailtySbx;
    QSpinBox *crfSbx;
    QComboBox *presetCbx;
    QComboBox *tuneCbx;
    QComboBox *profileCbx;

    QLineEdit *widthLineEdit;
    QLineEdit *heightLineEdit;
    QSize originalSize = QSize(-1, -1);
    QCheckBox *keepAspectRatioCkb;
    QLineEdit *videoMinBitrateLineEdit;
    QLineEdit *videoMaxBitrateLineEdit;

    QToolButton *startButton;
    QProgressBar *progressBar;
    QLabel *fpsLabel;
    QTimer *fpsTimer;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , d_ptr(new MainWindowPrivate(this))
{
    Ffmpeg::printFfmpegInfo();

    setupUI();
    buildConnect();
    resize(700, 430);
}

MainWindow::~MainWindow() {}

void MainWindow::onError(const Ffmpeg::AVError &avError)
{
    const QString str = tr("Error[%1]:%2.")
                            .arg(QString::number(avError.errorCode()), avError.errorString());
    qWarning() << str;
}

void MainWindow::onVideoEncoderChanged()
{
    auto codecID = static_cast<AVCodecID>(d_ptr->videoCodecCbx->currentData().toInt());
    auto quantizer = Ffmpeg::Utils::getCodecQuantizer(codecID);
    d_ptr->quailtySbx->setRange(quantizer.first, quantizer.second);
}

void MainWindow::onOpenInputFile()
{
    const QString path = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation)
                             .value(0, QDir::homePath());
    const QString filePath
        = QFileDialog::getOpenFileName(this,
                                       tr("Open File"),
                                       path,
                                       tr("Audio Video (*.mp3 *.mp4 *.mkv *.rmvb)"));
    if (filePath.isEmpty()) {
        return;
    }

    d_ptr->inTextEdit->setPlainText(filePath);

    d_ptr->initInputFileAttribute(filePath);
}

void MainWindow::onCheckInputFile()
{
    auto filePath = d_ptr->inTextEdit->toPlainText();
    if (filePath.isEmpty()) {
        qWarning() << "filePath.isEmpty()";
        return;
    }
    if (QFile::exists(filePath)) {
        d_ptr->initInputFileAttribute(filePath);
        return;
    }
    QUrl url(filePath);
    if (!url.isValid()) {
        qWarning() << "!url.isValid()";
        return;
    }
    d_ptr->initInputFileAttribute(url.toEncoded());
}

void MainWindow::onOpenOutputFile()
{
    const QString path = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation)
                             .value(0, QDir::homePath());
    const QString filePath
        = QFileDialog::getSaveFileName(this,
                                       tr("Save File"),
                                       path,
                                       tr("Audio Video (*.mp3 *.mp4 *.mkv *.rmvb)"));
    if (filePath.isEmpty()) {
        return;
    }

    d_ptr->outTextEdit->setPlainText(filePath);
}

void MainWindow::onStart()
{
    if (d_ptr->startButton->text() == tr("Start")) {
        auto inPath = d_ptr->inTextEdit->toPlainText();
        //        if (!QFile::exists(inPath)) {
        //            QMessageBox::warning(this, tr("Not exist"), tr("Input file path does not exist!"));
        //            return;
        //        }
        auto outPath = d_ptr->outTextEdit->toPlainText();
        if (inPath.isEmpty() || outPath.isEmpty()) {
            return;
        }

        d_ptr->transcode->setInFilePath(inPath);
        d_ptr->transcode->setOutFilePath(outPath);
        d_ptr->transcode->setAudioEncodecID(
            static_cast<AVCodecID>(d_ptr->audioCodecCbx->currentData().toInt()));
        d_ptr->transcode->setVideoEnCodecID(
            static_cast<AVCodecID>(d_ptr->videoCodecCbx->currentData().toInt()));
        d_ptr->transcode->setSize(
            {d_ptr->widthLineEdit->text().toInt(), d_ptr->heightLineEdit->text().toInt()});
        d_ptr->transcode->setQuailty(d_ptr->quailtySbx->value());
        d_ptr->transcode->setMinBitrate(d_ptr->videoMinBitrateLineEdit->text().toInt());
        d_ptr->transcode->setMaxBitrate(d_ptr->videoMaxBitrateLineEdit->text().toInt());
        d_ptr->transcode->setCrf(d_ptr->crfSbx->value());
        d_ptr->transcode->setPreset(d_ptr->presetCbx->currentText());
        d_ptr->transcode->setProfile(d_ptr->profileCbx->currentText());
        d_ptr->transcode->startTranscode();
        d_ptr->startButton->setText(tr("Stop"));

        auto filename = QFile::exists(inPath) ? QFileInfo(inPath).fileName()
                                              : QFileInfo(QUrl(inPath).toString()).fileName();
        setWindowTitle(filename);

        d_ptr->fpsTimer->start(1000);

    } else if (d_ptr->startButton->text() == tr("Stop")) {
        d_ptr->transcode->stopTranscode();
        d_ptr->progressBar->setValue(0);
        d_ptr->startButton->setText(tr("Start"));

        d_ptr->fpsTimer->stop();
    }
}

void MainWindow::setupUI()
{
    auto inBtn = new QToolButton(this);
    inBtn->setText(tr("Open In"));
    inBtn->setMinimumSize(BUTTON_SIZE);
    connect(inBtn, &QToolButton::clicked, this, &MainWindow::onOpenInputFile);
    auto checkInBtn = new QToolButton(this);
    checkInBtn->setText(tr("Check In File"));
    checkInBtn->setToolTip(tr("Enter the path manually and use it"));
    checkInBtn->setMinimumSize(BUTTON_SIZE);
    connect(checkInBtn, &QToolButton::clicked, this, &MainWindow::onCheckInputFile);
    auto outBtn = new QToolButton(this);
    outBtn->setText(tr("Open Out"));
    outBtn->setMinimumSize(BUTTON_SIZE);
    connect(outBtn, &QToolButton::clicked, this, &MainWindow::onOpenOutputFile);

    auto inLayout = new QVBoxLayout;
    inLayout->addWidget(inBtn);
    inLayout->addWidget(checkInBtn);

    auto editLayout = new QGridLayout;
    editLayout->addWidget(d_ptr->inTextEdit, 0, 0, 1, 1);
    editLayout->addLayout(inLayout, 0, 1, 1, 1);
    editLayout->addWidget(d_ptr->outTextEdit, 1, 0, 1, 1);
    editLayout->addWidget(outBtn, 1, 1, 1, 1);

    auto groupLayout1 = new QHBoxLayout;
    groupLayout1->addWidget(new QLabel(tr("Audio Codec ID:"), this));
    groupLayout1->addWidget(d_ptr->audioCodecCbx);
    groupLayout1->addWidget(new QLabel(tr("Video Codec ID:"), this));
    groupLayout1->addWidget(d_ptr->videoCodecCbx);
    auto groupBox = new QGroupBox(tr("Encoder Settings"), this);
    auto groupLayout = new QVBoxLayout(groupBox);
    groupLayout->addLayout(groupLayout1);
    groupLayout->addWidget(d_ptr->invalidSetting());
    groupLayout->addWidget(d_ptr->initVideoSetting());

    auto useGpuCheckBox = new QCheckBox(tr("GPU"), this);
    useGpuCheckBox->setToolTip(tr("GPU"));
    useGpuCheckBox->setChecked(true);
    connect(useGpuCheckBox, &QCheckBox::clicked, this, [this, useGpuCheckBox] {
        d_ptr->transcode->setUseGpu(useGpuCheckBox->isChecked());
    });
    d_ptr->transcode->setUseGpu(useGpuCheckBox->isChecked());
    auto displayLayout = new QHBoxLayout;
    displayLayout->addWidget(d_ptr->startButton);
    displayLayout->addWidget(useGpuCheckBox);
    displayLayout->addWidget(d_ptr->progressBar);
    displayLayout->addWidget(d_ptr->fpsLabel);

    auto widget = new QWidget(this);
    auto layout = new QVBoxLayout(widget);
    layout->addLayout(editLayout);
    layout->addWidget(groupBox);
    layout->addLayout(displayLayout);
    setCentralWidget(widget);
}

void MainWindow::buildConnect()
{
    connect(d_ptr->transcode, &Ffmpeg::Transcode::error, this, &MainWindow::onError);

    connect(d_ptr->videoCodecCbx,
            &QComboBox::currentTextChanged,
            this,
            &MainWindow::onVideoEncoderChanged);

    connect(d_ptr->widthLineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (!d_ptr->keepAspectRatioCkb->isChecked() || !d_ptr->originalSize.isValid()) {
            return;
        }
        auto multiple = d_ptr->originalSize.width() * 1.0 / text.toInt();
        int height = d_ptr->originalSize.height() / multiple;
        d_ptr->heightLineEdit->blockSignals(true);
        d_ptr->heightLineEdit->setText(QString::number(height));
        d_ptr->heightLineEdit->blockSignals(false);
        d_ptr->calBitrate();
    });
    connect(d_ptr->heightLineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (!d_ptr->keepAspectRatioCkb->isChecked() || !d_ptr->originalSize.isValid()) {
            return;
        }
        auto multiple = d_ptr->originalSize.height() * 1.0 / text.toInt();
        int width = d_ptr->originalSize.width() / multiple;
        d_ptr->widthLineEdit->blockSignals(true);
        d_ptr->widthLineEdit->setText(QString::number(width));
        d_ptr->widthLineEdit->blockSignals(false);
        d_ptr->calBitrate();
    });
    connect(d_ptr->keepAspectRatioCkb, &QCheckBox::stateChanged, this, [this] {
        if (!d_ptr->keepAspectRatioCkb->isChecked() || !d_ptr->originalSize.isValid()) {
            return;
        }
        auto multiple = d_ptr->originalSize.width() * 1.0 / d_ptr->widthLineEdit->text().toInt();
        int height = d_ptr->originalSize.height() / multiple;
        d_ptr->heightLineEdit->blockSignals(true);
        d_ptr->heightLineEdit->setText(QString::number(height));
        d_ptr->heightLineEdit->blockSignals(false);
        d_ptr->calBitrate();
    });

    connect(d_ptr->startButton, &QToolButton::clicked, this, &MainWindow::onStart);
    connect(d_ptr->transcode, &Ffmpeg::Transcode::progressChanged, this, [this](qreal value) {
        d_ptr->progressBar->setValue(value * 100);
    });
    connect(d_ptr->transcode, &Ffmpeg::Transcode::finished, this, [this] {
        if (d_ptr->startButton->text() == tr("Stop")) {
            d_ptr->startButton->click();
        }
    });
    connect(d_ptr->fpsTimer, &QTimer::timeout, this, [this] {
        auto str = QString("FPS: %1").arg(QString::number(d_ptr->transcode->fps(), 'f', 2));
        d_ptr->fpsLabel->setText(str);
    });
}
