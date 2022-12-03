#include "videorender.hpp"

#include <ffmpeg/frame.hpp>

#include <QDebug>
#include <QElapsedTimer>

extern "C" {
#include <libavformat/avformat.h>
}

namespace Ffmpeg {

float fps(int deltaTime) // ms
{
    static float avgDuration = 0.f;
    static float alpha = 1.f / 100.f; // 采样数设置为100
    static int frameCount = 0;

    ++frameCount;
    if (1 == frameCount) {
        avgDuration = static_cast<float>(deltaTime);
    } else {
        avgDuration = avgDuration * (1 - alpha) + deltaTime * alpha;
    }
    return (1.f / avgDuration * 1000);
}

void printFps()
{
    static QElapsedTimer timer;
    if (timer.isValid()) {
        qDebug() << "FPS: " << fps(timer.elapsed());
    }
    timer.restart();
}

class VideoRender::VideoRenderPrivate
{
public:
    VideoRenderPrivate() {}

    void flushFPS()
    {
        if (timer.isValid()) {
            fps = Ffmpeg::fps(timer.elapsed());
        }
        timer.restart();
    }

    float fps = 0;
    QElapsedTimer timer;
};

VideoRender::VideoRender()
    : d_ptr(new VideoRenderPrivate)
{}

VideoRender::~VideoRender() {}

void VideoRender::setFrame(QSharedPointer<Frame> frame)
{
    if (!isSupportedOutput_pix_fmt(AVPixelFormat(frame->avFrame()->format))) {
        frame = convertSupported_pix_fmt(frame);
    }
    updateFrame(frame);
    //qDebug() << frame->avFrame()->format;

    d_ptr->flushFPS();
}

float VideoRender::fps()
{
    return d_ptr->fps;
}

} // namespace Ffmpeg
