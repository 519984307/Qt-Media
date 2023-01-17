#ifndef HARDWAREENCODE_HPP
#define HARDWAREENCODE_HPP

#include <QObject>

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVCodec;

namespace Ffmpeg {

class Frame;
class CodecContext;
class HardWareEncode : public QObject
{
public:
    explicit HardWareEncode(QObject *parent = nullptr);
    ~HardWareEncode();

    bool initEncoder(const AVCodec *encoder);
    bool initHardWareDevice(CodecContext *codecContext);
    Frame *transToGpu(CodecContext *codecContext, Frame *in, bool &ok, bool &needFree);

    AVPixelFormat swFormat() const;

    bool isVaild();

private:
    class HardWareEncodePrivate;
    QScopedPointer<HardWareEncodePrivate> d_ptr;
};

} // namespace Ffmpeg

#endif // HARDWAREENCODE_HPP
