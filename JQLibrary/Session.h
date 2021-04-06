#ifndef SESSION_H
#define SESSION_H

#include <QObject>
#include <QPointer>
#include <QSslCertificate>
#include <functional>


class QIODevice;
class QThreadPool;
class QHostAddress;
class QTimer;
class QImage;
class QTcpServer;
class QLocalServer;
class QSslKey;
class QSslConfiguration;
class  Session: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY( Session )

public:
    Session( const QPointer< QIODevice > &socket );

    ~Session();

    inline void setHandleAcceptedCallback(const std::function< void(const QPointer< Session > &) > &callback) { handleAcceptedCallback_ = callback; }

    inline QPointer< QIODevice > ioDevice() { return ioDevice_; }

    QString requestSourceIp() const;

    QString requestMethod() const;

    QString requestUrl() const;

    QString requestCrlf() const;

    QMap< QString, QString > requestHeader() const;

    QByteArray requestBody() const;

    QString requestUrlPath() const;

    QStringList requestUrlPathSplitToList() const;

    QMap< QString, QString > requestUrlQuery() const;

    int replyHttpCode() const;

    qint64 replyBodySize() const;

#ifndef QT_NO_SSL
    QSslCertificate peerCertificate() const;
#endif

public slots:
    void replyText(const QString &replyData, const int &httpStatusCode = 200);

    void replyRedirects(const QUrl &targetUrl, const int &httpStatusCode = 200);

    void replyJsonObject(const QJsonObject &jsonObject, const int &httpStatusCode = 200);

    void replyJsonArray(const QJsonArray &jsonArray, const int &httpStatusCode = 200);

    void replyFile(const QString &filePath, const int &httpStatusCode = 200);

    void replyFile(const QString &fileName, const QByteArray &fileData, const int &httpStatusCode = 200);

    void replyImage(const QImage &image, const QString &format = "PNG", const int &httpStatusCode = 200);

    void replyImage(const QString &imageFilePath, const int &httpStatusCode = 200);

    void replyBytes(const QByteArray &bytes, const QString &contentType = "application/octet-stream", const int &httpStatusCode = 200);

    void replyOptions();

private:
    void inspectionBufferSetup1();

    void inspectionBufferSetup2();

    void onBytesWritten(const qint64 &written);

private:
    static QAtomicInt remainSession_;

    QPointer< QIODevice >                                ioDevice_;
    std::function< void( const QPointer< Session > & ) > handleAcceptedCallback_;
    QSharedPointer< QTimer >                             autoCloseTimer_;

    QByteArray receiveBuffer_;

    QString                  requestSourceIp_;
    QString                  requestMethod_;
    QString                  requestUrl_;
    QString                  requestCrlf_;
    QByteArray               requestBody_;
    QMap< QString, QString > requestHeader_;

    bool   headerAcceptedFinished_  = false;
    bool   contentAcceptedFinished_ = false;
    qint64 contentLength_           = -1;

    int        replyHttpCode_ = -1;
    QByteArray replyBuffer_;
    qint64     replyBodySize_ = -1;

    qint64                      waitWrittenByteCount_ = -1;
    QSharedPointer< QIODevice > replyIoDevice_;
};


#endif // SESSION_H
