#ifndef SSLSERVERMANAGE_H
#define SSLSERVERMANAGE_H

#include "AbstractManage.h"
#ifndef QT_NO_SSL
class SslServerHelper;
class QSslSocket;
#include <QSsl>
#include <QList>
#include <QSslSocket>
#include <QHostAddress>
#include <QPointer>




class  SslServerManage: public AbstractManage
{
    Q_OBJECT
    Q_DISABLE_COPY( SslServerManage )

public:
    SslServerManage(const int &handleMaxThreadCount = 2);

    ~SslServerManage();

    bool listen( const QHostAddress &                                   address,
                 const quint16 &                                        port,
                 const QString &                                        crtFilePath,
                 const QString &                                        keyFilePath,
                 const QList< QPair< QString, QSsl::EncodingFormat > > &caFileList = {},    // [ { filePath, format } ]
                 const QSslSocket::PeerVerifyMode &                     peerVerifyMode = QSslSocket::VerifyNone );

private:
    bool isRunning();

    bool onStart();

    void onFinish();

private:
    QPointer< SslServerHelper > tcpServer_;

    QHostAddress listenAddress_ = QHostAddress::Any;
    quint16      listenPort_    = 0;

    QSharedPointer< QSslConfiguration > sslConfiguration_;
};

#endif
#endif // SSLSERVERMANAGE_H
