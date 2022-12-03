#include "videodecoder.h"
#include "avcontextinfo.h"
#include "decodervideoframe.h"

#include <QDebug>
#include <QPixmap>

namespace Ffmpeg {

class VideoDecoder::VideoDecoderPrivate
{
public:
    VideoDecoderPrivate(QObject *parent)
        : owner(parent)
    {
        decoderVideoFrame = new DecoderVideoFrame(owner);
    }

    QObject *owner;

    DecoderVideoFrame *decoderVideoFrame;
};

VideoDecoder::VideoDecoder(QObject *parent)
    : Decoder<Packet *>(parent)
    , d_ptr(new VideoDecoderPrivate(this))
{}

VideoDecoder::~VideoDecoder() {}

void VideoDecoder::pause(bool state)
{
    d_ptr->decoderVideoFrame->pause(state);
}

void VideoDecoder::setSpeed(double speed)
{
    Decoder<Packet *>::setSpeed(speed);
    d_ptr->decoderVideoFrame->setSpeed(speed);
}

void VideoDecoder::setVideoOutputRenders(QVector<VideoOutputRender *> videoOutputRenders)
{
    d_ptr->decoderVideoFrame->setVideoOutputRenders(videoOutputRenders);
}

void VideoDecoder::setVideoOutputRenders(QVector<VideoRender *> videoOutputRenders)
{
    d_ptr->decoderVideoFrame->setVideoOutputRenders(videoOutputRenders);
}

QVector<VideoRender *> VideoDecoder::videoRenders()
{
    return d_ptr->decoderVideoFrame->videoRenders();
}

void VideoDecoder::runDecoder()
{
    d_ptr->decoderVideoFrame->startDecoder(m_formatContext, m_contextInfo);

    while (m_runing) {
        if (m_seek) {
            clear();
            d_ptr->decoderVideoFrame->seek(m_seekTime, m_latchPtr.lock());
            seekFinish();
        }
        if (m_queue.isEmpty()) {
            msleep(Sleep_Queue_Empty_Milliseconds);
            continue;
        }

        QScopedPointer<Packet> packetPtr(m_queue.dequeue());
        std::unique_ptr<Frame> framePtr(m_contextInfo->decodeFrame(packetPtr.data()));
        if (!framePtr) {
            continue;
        }

        d_ptr->decoderVideoFrame->append(framePtr.release());

        while (m_runing && d_ptr->decoderVideoFrame->size() > Max_Frame_Size && !m_seek) {
            msleep(Sleep_Queue_Full_Milliseconds);
        }
    }
    while (m_runing && d_ptr->decoderVideoFrame->size() != 0) {
        msleep(Sleep_Queue_Full_Milliseconds);
    }
    d_ptr->decoderVideoFrame->stopDecoder();
}

} // namespace Ffmpeg
