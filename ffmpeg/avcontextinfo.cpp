#include "avcontextinfo.h"
#include "codeccontext.h"
#include "frame.hpp"
#include "hardwaredecode.hpp"

#include <QDebug>
#include <QMetaEnum>
#include <QTime>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#define Error_Index -1

namespace Ffmpeg {

struct AVContextInfo::AVContextInfoPrivate
{
    QScopedPointer<CodecContext> codecCtx; //解码器上下文
    AVStream *stream;                      //流
    int streamIndex = Error_Index;         // 索引
    AVContextInfo::MediaType mediaType;
    QScopedPointer<HardWareDecode> hardWareDecode;
    bool gpuDecode = false;
};

QString AVContextInfo::mediaTypeString(MediaType type)
{
    return QMetaEnum::fromType<MediaType>().valueToKey(type);
}

AVContextInfo::AVContextInfo(MediaType mediaType, QObject *parent)
    : QObject(parent)
    , d_ptr(new AVContextInfoPrivate)
{
    d_ptr->mediaType = mediaType;
}

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
    if (d_ptr->mediaType == Video) {
        qDebug() << "FPS: " << av_q2d(stream->avg_frame_rate);
        qDebug() << "Total number of frames: " << stream->nb_frames;
        qDebug() << "Resolution of resolution: " << stream->codecpar->width << "x"
                 << stream->codecpar->height;
    }
}

AVStream *AVContextInfo::stream()
{
    return d_ptr->stream;
}

bool AVContextInfo::findDecoder(bool useGpu)
{
    Q_ASSERT(d_ptr->stream != nullptr);
    d_ptr->gpuDecode = useGpu;
    const char *typeStr = av_get_media_type_string(d_ptr->stream->codecpar->codec_type);
    AVCodec *codec = avcodec_find_decoder(d_ptr->stream->codecpar->codec_id);
    if (!codec) {
        qWarning() << tr("%1 Codec not found.").arg(typeStr);
        return false;
    }
    d_ptr->codecCtx.reset(new CodecContext(codec));
    if (!d_ptr->codecCtx->setParameters(d_ptr->stream->codecpar)) {
        return false;
    }
    d_ptr->codecCtx->setTimebase(d_ptr->stream->time_base);
    connect(d_ptr->codecCtx.data(),
            &CodecContext::error,
            this,
            &AVContextInfo::error,
            Qt::UniqueConnection);
    if (d_ptr->mediaType == Video && d_ptr->gpuDecode) {
        d_ptr->hardWareDecode.reset(new HardWareDecode);
        d_ptr->hardWareDecode->initPixelFormat(codec);
        d_ptr->hardWareDecode->initHardWareDevice(d_ptr->codecCtx.data());
    }

    //用于初始化pCodecCtx结构
    if (!d_ptr->codecCtx->open(codec)) {
        return false;
    }

    qInfo() << tr("Decoder name: ") << codec->name;

    return true;
}

bool AVContextInfo::sendPacket(Packet *packet)
{
    return d_ptr->codecCtx->sendPacket(packet);
}

bool AVContextInfo::receiveFrame(Frame *frame)
{
    return d_ptr->codecCtx->receiveFrame(frame);
}

bool AVContextInfo::decodeSubtitle2(Subtitle *subtitle, Packet *packet)
{
    return d_ptr->codecCtx->decodeSubtitle2(subtitle, packet);
}

Frame *AVContextInfo::decodeFrame(Packet *packet)
{
    if (!sendPacket(packet)) {
        return nullptr;
    }
    std::unique_ptr<Frame> framePtr(new Frame);
    if (!receiveFrame(framePtr.get())) { // 一个packet一个视频帧
        return nullptr;
    }
    if (d_ptr->gpuDecode && d_ptr->mediaType == Video) {
        bool ok = false;
        framePtr.reset(d_ptr->hardWareDecode->transforFrame(framePtr.get(), ok));
        if (!ok) {
            return nullptr;
        }
    }
    return framePtr.release();
}

void AVContextInfo::flush()
{
    d_ptr->codecCtx->flush();
}

double AVContextInfo::timebase()
{
    return av_q2d(d_ptr->stream->time_base);
}

double AVContextInfo::fps()
{
    return av_q2d(d_ptr->stream->avg_frame_rate);
}

qint64 AVContextInfo::fames()
{
    return d_ptr->stream->nb_frames;
}

QSize AVContextInfo::resolutionRatio()
{
    return {d_ptr->stream->codecpar->width, d_ptr->stream->codecpar->height};
}

bool AVContextInfo::isGpuDecode()
{
    return d_ptr->gpuDecode;
}

} // namespace Ffmpeg
