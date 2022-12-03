#ifndef DECODERVIDEOFRAME_H
#define DECODERVIDEOFRAME_H

#include "decoder.h"
#include "frame.hpp"

#include <utils/taskqueue.h>

struct AVFrame;

namespace Ffmpeg {

class VideoOutputRender;
class VideoRender;

class DecoderVideoFrame : public Decoder<Frame *>
{
    Q_OBJECT
public:
    explicit DecoderVideoFrame(QObject *parent = nullptr);
    ~DecoderVideoFrame();

    void stopDecoder() override;

    void pause(bool state) override;

    void setVideoOutputRenders(QVector<VideoOutputRender *> videoOutputRenders);
    void setVideoOutputRenders(QVector<VideoRender *> videoRenders);
    QVector<VideoRender *> videoRenders();

protected:
    void runDecoder() override;

private:
    void checkPause();
    void checkSeek();

    class DecoderVideoFramePrivate;
    QScopedPointer<DecoderVideoFramePrivate> d_ptr;
};

} // namespace Ffmpeg

#endif // DECODERVIDEOFRAME_H
