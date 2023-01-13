#ifndef PACKET_H
#define PACKET_H

#include "ffmepg_global.h"

#include <QtCore>

struct AVPacket;
struct AVRational;

namespace Ffmpeg {

class FFMPEG_EXPORT Packet
{
public:
    explicit Packet();
    Packet(const Packet &other);
    Packet &operator=(const Packet &other);
    ~Packet();

    bool isKey();

    void unref();

    void setPts(double pts);
    double pts();

    void setDuration(double duration);
    double duration();

    void setStreamIndex(int index);
    int streamIndex() const;

    void rescaleTs(const AVRational &srcTimeBase, const AVRational &dstTimeBase);

    AVPacket *avPacket();

private:
    class PacketPrivate;
    QScopedPointer<PacketPrivate> d_ptr;
};

} // namespace Ffmpeg

#endif // PACKET_H
