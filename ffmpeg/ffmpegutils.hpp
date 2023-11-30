#ifndef FFMPEGUTILS_HPP
#define FFMPEGUTILS_HPP

#include "ffmepg_global.h"

#include <QSize>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/hwcontext.h>
}

struct AVCodec;

namespace Ffmpeg {

class Packet;
class Frame;
class AVContextInfo;
class FormatContext;

using Metadatas = QMap<QString, QString>;

void FFMPEG_EXPORT printFfmpegInfo();

void calculatePts(Frame *frame, AVContextInfo *contextInfo, FormatContext *formatContext);
void calculatePts(Packet *packet, AVContextInfo *contextInfo);

auto getCurrentHWDeviceTypes() -> QVector<AVHWDeviceType>;

auto getPixelFormat(const AVCodec *codec, AVHWDeviceType type) -> AVPixelFormat;

auto compareAVRational(const AVRational &a, const AVRational &b) -> bool;

auto getMetaDatas(AVDictionary *metadata) -> Metadatas;

struct CodecInfo
{
    AVMediaType mediaType = AVMEDIA_TYPE_UNKNOWN;
    AVCodecID id = AV_CODEC_ID_NONE;
    QString name;
    QSize size = QSize(-1, -1);
};

auto FFMPEG_EXPORT getFileCodecInfo(const QString &filePath) -> QVector<CodecInfo>;

auto FFMPEG_EXPORT getCodecQuantizer(const QString &codecname) -> QPair<int, int>;

auto FFMPEG_EXPORT getCurrentSupportCodecs(AVMediaType mediaType, bool encoder) -> QStringList;

} // namespace Ffmpeg

#endif // FFMPEGUTILS_HPP
