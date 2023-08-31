#include "decodersubtitleframe.hpp"
#include "clock.hpp"
#include "codeccontext.h"

#include <event/pauseevent.hpp>
#include <event/seekevent.hpp>
#include <subtitle/ass.hpp>
#include <videorender/videorender.hpp>

#include <QDebug>
#include <QImage>
#include <QWaitCondition>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

namespace Ffmpeg {

class DecoderSubtitleFrame::DecoderSubtitleFramePrivate
{
public:
    explicit DecoderSubtitleFramePrivate(DecoderSubtitleFrame *q)
        : q_ptr(q)
    {
        clock = new Clock(q);
    }

    void processEvent(Ass *ass, bool &firstFrame)
    {
        while (q_ptr->m_runing.load() && q_ptr->m_eventQueue.size() > 0) {
            qDebug() << "DecoderSubtitleFrame::processEvent";
            auto eventPtr = q_ptr->m_eventQueue.take();
            switch (eventPtr->type()) {
            case Event::EventType::Pause: {
                auto pauseEvent = static_cast<PauseEvent *>(eventPtr.data());
                auto paused = pauseEvent->paused();
                clock->setPaused(paused);
                if (paused) {
                    firstFrame = false;
                }
            } break;
            case Event::EventType::Seek: {
                auto seekEvent = static_cast<SeekEvent *>(eventPtr.data());
                auto position = seekEvent->position();
                q_ptr->clear();
                clock->reset(position);
                ass->flushASSEvents();
                firstFrame = false;
            }
            default: break;
            }
        }
    }

    DecoderSubtitleFrame *q_ptr;

    Clock *clock;

    QMutex mutex;
    QWaitCondition waitCondition;
    QSize videoResolutionRatio = QSize(1280, 720);

    QMutex mutex_render;
    QVector<VideoRender *> videoRenders = {};
};

DecoderSubtitleFrame::DecoderSubtitleFrame(QObject *parent)
    : Decoder<SubtitlePtr>(parent)
    , d_ptr(new DecoderSubtitleFramePrivate(this))
{}

DecoderSubtitleFrame::~DecoderSubtitleFrame()
{
    stopDecoder();
}

void DecoderSubtitleFrame::setVideoResolutionRatio(const QSize &size)
{
    if (!size.isValid()) {
        return;
    }
    d_ptr->videoResolutionRatio = size;
}

void DecoderSubtitleFrame::setVideoRenders(QVector<VideoRender *> videoRenders)
{
    QMutexLocker locker(&d_ptr->mutex_render);
    d_ptr->videoRenders = videoRenders;
}

void DecoderSubtitleFrame::runDecoder()
{
    quint64 dropNum = 0;
    auto ctx = m_contextInfo->codecCtx()->avCodecCtx();
    QScopedPointer<Ass> assPtr(new Ass);
    if (ctx->subtitle_header) {
        assPtr->init(ctx->subtitle_header, ctx->subtitle_header_size);
    }
    assPtr->setWindowSize(d_ptr->videoResolutionRatio);
    SwsContext *swsContext = nullptr;
    bool firstFrame = false;
    while (m_runing.load()) {
        d_ptr->processEvent(assPtr.data(), firstFrame);

        auto subtitlePtr(m_queue.take());
        if (subtitlePtr.isNull()) {
            continue;
        } else if (!firstFrame) {
            qDebug() << "Subtitle firstFrame: "
                     << QTime::fromMSecsSinceStartOfDay(subtitlePtr->pts() / 1000)
                            .toString("hh:mm:ss.zzz");
            firstFrame = true;
            d_ptr->clock->reset(subtitlePtr->pts());
        }
        subtitlePtr->setVideoResolutionRatio(d_ptr->videoResolutionRatio);
        subtitlePtr->parse(&swsContext);
        if (subtitlePtr->type() == Subtitle::Type::ASS) {
            subtitlePtr->resolveAss(assPtr.data());
            subtitlePtr->generateImage();
        }
        d_ptr->clock->update(subtitlePtr->pts(), av_gettime_relative());
        qint64 delay = 0;
        if (!d_ptr->clock->getDelayWithMaster(delay)) {
            continue;
        }
        auto delayDuration = delay + subtitlePtr->duration();
        if (!Clock::adjustDelay(delayDuration)) {
            qDebug() << "Subtitle Delay: " << delay;
            dropNum++;
            continue;
        }
        if (Clock::adjustDelay(delay)) {
            if (delay > 0) {
                QMutexLocker locker(&d_ptr->mutex);
                d_ptr->waitCondition.wait(&d_ptr->mutex, delay / 1000);
            }
        }

        renderFrame(subtitlePtr);
    }
    sws_freeContext(swsContext);
    qInfo() << "Subtitle Drop Num:" << dropNum;
}

void DecoderSubtitleFrame::renderFrame(const QSharedPointer<Subtitle> &subtitlePtr)
{
    QMutexLocker locker(&d_ptr->mutex_render);
    for (auto render : d_ptr->videoRenders) {
        render->setSubTitleFrame(subtitlePtr);
    }
}

} // namespace Ffmpeg
