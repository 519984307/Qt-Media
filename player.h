#ifndef PLAYER_H
#define PLAYER_H

#include <QThread>

class PlayerPrivate;
class Player : public QThread
{
    Q_OBJECT
public:
    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    bool isOpen();
    QString lastError() const;

    void stop();
    void pause(bool status = true);

public slots:
    void onSetFilePath(const QString &filepath);
    void onPlay();

signals:
    void readyRead(const QPixmap& pixmap);
    void error(const QString& e);

protected:
    void run() override;

private:
    bool initAvCode();
    void playVideo();

    QScopedPointer<PlayerPrivate> d_ptr;
};

#endif // PLAYER_H
