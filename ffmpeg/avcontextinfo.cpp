#include "avcontextinfo.h"
#include "codeccontext.h"
#include "frame.hpp"
#include "packet.h"

#include <gpu/hardwaredecode.hpp>
#include <gpu/hardwareencode.hpp>

#include <QDebug>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

#define Error_Index -1

namespace Ffmpeg {

class AVContextInfo::AVContextInfoPrivate
{
public:
    AVContextInfoPrivate(AVContextInfo *q)
        : q_ptr(q)
    {}

    void printCodecpar()
    {
        auto codecpar = stream->codecpar;
        qInfo() << "start_time: " << stream->start_time;
        qInfo() << "duration: " << stream->duration;
        qInfo() << "nb_frames: " << stream->nb_frames;
        qInfo() << "format: " << codecpar->format;
        qInfo() << "bit_rate: " << codecpar->bit_rate;
        switch (codecpar->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            qInfo() << "avg_frame_rate: " << av_q2d(stream->avg_frame_rate);
            qInfo() << "sample_aspect_ratio: " << av_q2d(stream->sample_aspect_ratio);
            qInfo() << "Resolution of resolution: " << codecpar->width << "x" << codecpar->height;
            qInfo() << "color_range: " << av_color_range_name(codecpar->color_range);
            qInfo() << "color_primaries: " << av_color_primaries_name(codecpar->color_primaries);
            qInfo() << "color_trc: " << av_color_transfer_name(codecpar->color_trc);
            qInfo() << "color_space: " << av_color_space_name(codecpar->color_space);
            qInfo() << "chroma_location: " << av_chroma_location_name(codecpar->chroma_location);
            qInfo() << "video_delay: " << codecpar->video_delay;
            break;
        case AVMEDIA_TYPE_AUDIO:
            qInfo() << "channels: " << codecpar->channels;
            qInfo() << "channel_layout: " << codecpar->channel_layout;
            qInfo() << "sample_rate: " << codecpar->sample_rate;
            qInfo() << "frame_size: " << codecpar->frame_size;
            break;
        default: break;
        }
    }

    void printMetaData()
    {
        QMap<QString, QString> maps;
        AVDictionaryEntry *tag = nullptr;
        while (nullptr != (tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            maps.insert(tag->key, QString::fromUtf8(tag->value));
        }
        qDebug() << maps;
    }

    AVContextInfo *q_ptr;

    QScopedPointer<CodecContext> codecCtx; //解码器上下文
    AVStream *stream = nullptr;            //流
    int streamIndex = Error_Index;         // 索引
    QScopedPointer<HardWareDecode> hardWareDecodePtr;
    QScopedPointer<HardWareEncode> hardWareEncodePtr;
    GpuType gpuType = GpuType::NotUseGpu;
};

AVContextInfo::AVContextInfo(QObject *parent)
    : QObject(parent)
    , d_ptr(new AVContextInfoPrivate(this))
{}

AVContextInfo::~AVContextInfo() {}

CodecContext *AVContextInfo::codecCtx()
{
    return d_ptr->codecCtx.data();
}

void AVContextInfo::resetIndex()
{
    d_ptr->streamIndex = Error_Index;
}

void AVContextInfo::setIndex(int index)
{
    d_ptr->streamIndex = index;
}

int AVContextInfo::index()
{
    return d_ptr->streamIndex;
}

bool AVContextInfo::isIndexVaild()
{
    return d_ptr->streamIndex != Error_Index;
}

void AVContextInfo::setStream(AVStream *stream)
{
    d_ptr->stream = stream;
    //    d_ptr->printCodecpar();
    //    d_ptr->printMetaData();
}

AVStream *AVContextInfo::stream()
{
    return d_ptr->stream;
}

bool AVContextInfo::initDecoder(const AVRational &frameRate)
{
    Q_ASSERT(d_ptr->stream != nullptr);
    const char *typeStr = av_get_media_type_string(d_ptr->stream->codecpar->codec_type);
    auto codec = avcodec_find_decoder(d_ptr->stream->codecpar->codec_id);
    if (!codec) {
        qWarning() << tr("%1 Codec not found.").arg(typeStr);
        return false;
    }
    d_ptr->codecCtx.reset(new CodecContext(codec));
    if (!d_ptr->codecCtx->setParameters(d_ptr->stream->codecpar)) {
        return false;
    }
    auto avCodecCtx = d_ptr->codecCtx->avCodecCtx();
    avCodecCtx->pkt_timebase = d_ptr->stream->time_base;
    d_ptr->codecCtx->setThreadCount(4);
    if (d_ptr->stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        avCodecCtx->framerate = frameRate;
    }

    qInfo() << tr("Decoder name: ") << codec->name;

    return true;
}

bool AVContextInfo::initEncoder(AVCodecID codecId)
{
    auto encodec = avcodec_find_encoder(codecId);
    if (!encodec) {
        qWarning() << tr("%1 Encoder not found.").arg(avcodec_get_name(codecId));
        return false;
    }
    d_ptr->codecCtx.reset(new CodecContext(encodec));
    return true;
}

bool AVContextInfo::initEncoder(const QString &name)
{
    auto encodec = avcodec_find_encoder_by_name(name.toLocal8Bit().constData());
    if (!encodec) {
        qWarning() << tr("%1 Encoder not found.").arg(name);
        return false;
    }
    d_ptr->codecCtx.reset(new CodecContext(encodec));
    return true;
}

bool AVContextInfo::openCodec(GpuType type)
{
    d_ptr->gpuType = type;
    if (mediaType() == AVMEDIA_TYPE_VIDEO) {
        switch (d_ptr->gpuType) {
        case GpuDecode:
            d_ptr->hardWareDecodePtr.reset(new HardWareDecode);
            d_ptr->hardWareDecodePtr->initPixelFormat(d_ptr->codecCtx->codec());
            d_ptr->hardWareDecodePtr->initHardWareDevice(d_ptr->codecCtx.data());
            break;
        case GpuEncode:
            d_ptr->hardWareEncodePtr.reset(new HardWareEncode);
            d_ptr->hardWareEncodePtr->initEncoder(d_ptr->codecCtx->codec());
            d_ptr->hardWareEncodePtr->initHardWareDevice(d_ptr->codecCtx.data());
            break;
        default: break;
        }
    }

    //用于初始化pCodecCtx结构
    if (!d_ptr->codecCtx->open()) {
        return false;
    }
    return true;
}

bool AVContextInfo::decodeSubtitle2(Subtitle *subtitle, Packet *packet)
{
    return d_ptr->codecCtx->decodeSubtitle2(subtitle, packet);
}

QVector<Frame *> AVContextInfo::decodeFrame(Packet *packet)
{
    QVector<Frame *> frames{};
    if (!d_ptr->codecCtx->sendPacket(packet)) {
        return frames;
    }
    std::unique_ptr<Frame> framePtr(new Frame);
    while (d_ptr->codecCtx->receiveFrame(framePtr.get())) {
        if (d_ptr->gpuType == GpuDecode && mediaType() == AVMEDIA_TYPE_VIDEO) {
            bool ok = false;
            framePtr.reset(d_ptr->hardWareDecodePtr->transFromGpu(framePtr.release(), ok));
            if (!ok) {
                return frames;
            }
        }
        frames.append(framePtr.release());
        framePtr.reset(new Frame);
    }
    return frames;
}

QVector<Packet *> AVContextInfo::encodeFrame(QSharedPointer<Frame> framePtr)
{
    QVector<Packet *> packets{};
    auto frame_tmp_ptr = framePtr;
    if (d_ptr->gpuType == GpuEncode && mediaType() == AVMEDIA_TYPE_VIDEO
        && framePtr->avFrame() != nullptr) {
        bool ok = false;
        frame_tmp_ptr = d_ptr->hardWareEncodePtr->transToGpu(d_ptr->codecCtx.data(), framePtr, ok);
        if (!ok) {
            return packets;
        }
    }
    if (!d_ptr->codecCtx->sendFrame(frame_tmp_ptr.data())) {
        return packets;
    }
    std::unique_ptr<Packet> packetPtr(new Packet);
    while (d_ptr->codecCtx->receivePacket(packetPtr.get())) {
        packets.append(packetPtr.release());
        packetPtr.reset(new Packet);
    }
    return packets;
}

double AVContextInfo::calTimebase() const
{
    return av_q2d(d_ptr->stream->time_base);
}

AVRational AVContextInfo::timebase() const
{
    return d_ptr->stream->time_base;
}

double AVContextInfo::fps() const
{
    return av_q2d(d_ptr->stream->avg_frame_rate);
}

qint64 AVContextInfo::fames() const
{
    return d_ptr->stream->nb_frames;
}

QSize AVContextInfo::resolutionRatio() const
{
    return {d_ptr->stream->codecpar->width, d_ptr->stream->codecpar->height};
}

AVMediaType AVContextInfo::mediaType() const
{
    return d_ptr->stream->codecpar->codec_type;
}

QString AVContextInfo::mediaTypeString() const
{
    return av_get_media_type_string(mediaType());
}

bool AVContextInfo::isDecoder() const
{
    return d_ptr->codecCtx->isDecoder();
}

AVContextInfo::GpuType AVContextInfo::gpuType() const
{
    return d_ptr->gpuType;
}

AVPixelFormat AVContextInfo::pixfmt() const
{
    if (d_ptr->gpuType == GpuEncode && mediaType() == AVMEDIA_TYPE_VIDEO
        && d_ptr->hardWareEncodePtr->isVaild()) {
        return d_ptr->hardWareEncodePtr->swFormat();
    }
    return d_ptr->codecCtx->avCodecCtx()->pix_fmt;
}

} // namespace Ffmpeg
