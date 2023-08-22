#ifndef SUBTITLE_H
#define SUBTITLE_H

#include "ffmepg_global.h"

#include <ffmpeg/subtitle/assdata.hpp>

#include <QObject>

struct AVSubtitle;
struct SwsContext;

namespace Ffmpeg {

class Ass;

class FFMPEG_EXPORT Subtitle : public QObject
{
    Q_OBJECT
public:
    enum Type { Unknown, Graphics, TEXT, ASS };

    explicit Subtitle(QObject *parent = nullptr);
    ~Subtitle() override;

    void setDefault(double pts, double duration, const QString &text);
    auto pts() -> double;
    auto duration() -> double;

    void parse(SwsContext *swsContext);
    [[nodiscard]] auto texts() const -> QByteArrayList;

    void setVideoResolutionRatio(const QSize &size);
    [[nodiscard]] auto videoResolutionRatio() const -> QSize;

    auto resolveAss(Ass *ass) -> bool;
    void setAssDataInfoList(const AssDataInfoList &list);
    [[nodiscard]] AssDataInfoList list() const;

    [[nodiscard]] auto generateImage() const -> QImage;
    [[nodiscard]] auto image() const -> QImage;

    [[nodiscard]] auto type() const -> Type;

    auto avSubtitle() -> AVSubtitle *;

private:
    class SubtitlePrivate;
    QScopedPointer<SubtitlePrivate> d_ptr;
};

} // namespace Ffmpeg

#endif // SUBTITLE_H
