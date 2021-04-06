#include "Session.h"
#include <QIODevice>
#include <QAbstractSocket>
#include <QSslSocket>
#include <QTimer>
#include <QHostAddress>
#include <QUrl>
#include <QThread>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QImage>
#include "ReplayHeader.h"

#ifndef QT_NO_SSL
#   include <QSslKey>
#   include <QSslConfiguration>
#endif


#define JQHTTPSERVER_SESSION_PROTECTION( functionName, ... )                             \
    auto this_ = this;                                                                   \
    if ( !this_ || ( contentLength_ < -1 ) || ( waitWrittenByteCount_ < -1 ) )           \
    {                                                                                    \
        qDebug().noquote() << QStringLiteral( "JQHttpServer::Session::" ) + functionName \
                                  + ": current session this is null";                    \
        return __VA_ARGS__;                                                              \
    }

#define JQHTTPSERVER_SESSION_REPLY_PROTECTION( functionName, ... )                                            \
    JQHTTPSERVER_SESSION_PROTECTION( functionName, __VA_ARGS__ )                                              \
    if ( ( replyHttpCode_ >= 0 ) && ( QThread::currentThread() != this->thread() ) )                          \
    {                                                                                                         \
        qDebug().noquote() << QStringLiteral( "JQHttpServer::Session::" ) + functionName + ": already reply"; \
        return __VA_ARGS__;                                                                                   \
    }

#define JQHTTPSERVER_SESSION_REPLY_PROTECTION2( functionName, ... )                                    \
    if ( ioDevice_.isNull() )                                                                          \
    {                                                                                                  \
        qDebug().noquote() << QStringLiteral( "JQHttpServer::Session::" ) + functionName + ": error1"; \
        this->deleteLater();                                                                           \
        return __VA_ARGS__;                                                                            \
    }
QAtomicInt Session::remainSession_ = 0;

Session::Session(const QPointer< QIODevice > &socket):
    ioDevice_( socket ),
    autoCloseTimer_( new QTimer )
{
    ++remainSession_;
//    qDebug() << "remainSession:" << remainSession_ << this;

    if ( qobject_cast< QAbstractSocket * >( socket ) )
    {
        requestSourceIp_ = ( qobject_cast< QAbstractSocket * >( socket ) )->peerAddress().toString().replace( "::ffff:", "" );
    }

    connect( ioDevice_.data(), &QIODevice::readyRead, [ this ]()
    {
        autoCloseTimer_->stop();
        this->receiveBuffer_.append( this->ioDevice_->readAll() );
        this->inspectionBufferSetup1();

        autoCloseTimer_->start();
    } );

#ifndef QT_NO_SSL
    if ( qobject_cast< QSslSocket * >( socket ) )
    {
        connect( qobject_cast< QSslSocket * >( socket ),
                 &QSslSocket::encryptedBytesWritten,
                 std::bind( &Session::onBytesWritten, this, std::placeholders::_1 ) );
    }
    else
#endif
    {
        connect( ioDevice_.data(),
                 &QIODevice::bytesWritten,
                 std::bind( &Session::onBytesWritten, this, std::placeholders::_1 ) );
    }

    autoCloseTimer_->setInterval( 30 * 1000 );
    autoCloseTimer_->setSingleShot( true );
    autoCloseTimer_->start();

    connect( autoCloseTimer_.data(), &QTimer::timeout, this, &QObject::deleteLater );
}

Session::~Session()
{
    --remainSession_;
//    qDebug() << "remainSession:" << remainSession_ << this;

    if ( !ioDevice_.isNull() )
    {
        delete ioDevice_.data();
    }
}

QString Session::requestSourceIp() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "requestSource", { } )

    return requestSourceIp_;
}

QString Session::requestMethod() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "requestMethod", { } )

    return requestMethod_;
}

QString Session::requestUrl() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "requestUrl", { } )

    return requestUrl_;
}

QString Session::requestCrlf() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "requestCrlf", { } )

    return requestCrlf_;
}

QMap< QString, QString > Session::requestHeader() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "requestHeader", { } )

    return requestHeader_;
}

QByteArray Session::requestBody() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "requestBody", { } )

    return requestBody_;
}

QString Session::requestUrlPath() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "requestUrlPath", { } )

    QString result;
    const auto indexForQueryStart = requestUrl_.indexOf( "?" );

    if ( indexForQueryStart >= 0 )
    {
        result = requestUrl_.mid( 0, indexForQueryStart );
    }
    else
    {
        result = requestUrl_;
    }

    if ( result.startsWith( "//" ) )
    {
        result = result.mid( 1 );
    }

    if ( result.endsWith( "/" ) )
    {
        result = result.mid( 0, result.size() - 1 );
    }

    result.replace( "%5B", "[" );
    result.replace( "%5D", "]" );
    result.replace( "%7B", "{" );
    result.replace( "%7D", "}" );
    result.replace( "%5E", "^" );

    return result;
}

QStringList Session::requestUrlPathSplitToList() const
{
    auto list = this->requestUrlPath().split( "/" );

    while ( !list.isEmpty() && list.first().isEmpty() )
    {
        list.pop_front();
    }

    while ( !list.isEmpty() && list.last().isEmpty() )
    {
        list.pop_back();
    }

    return list;
}

QMap< QString, QString > Session::requestUrlQuery() const
{
    const auto indexForQueryStart = requestUrl_.indexOf( "?" );
    if ( indexForQueryStart < 0 ) { return { }; }

    QMap< QString, QString > result;

    auto lines = QUrl::fromEncoded( requestUrl_.mid( indexForQueryStart + 1 ).toUtf8() ).toString().split( "&" );

    for ( const auto &line_: lines )
    {
        auto line = line_;
        line.replace( "%5B", "[" );
        line.replace( "%5D", "]" );
        line.replace( "%7B", "{" );
        line.replace( "%7D", "}" );
        line.replace( "%5E", "^" );

        auto indexOf = line.indexOf( "=" );
        if ( indexOf > 0 )
        {
            result[ line.mid( 0, indexOf ) ] = line.mid( indexOf + 1 );
        }
    }

    return result;
}

int Session::replyHttpCode() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "replyHttpCode", -1 )

    return replyHttpCode_;
}

qint64 Session::replyBodySize() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "replyBodySize", -1 )

    return replyBodySize_;
}

#ifndef QT_NO_SSL
QSslCertificate Session::peerCertificate() const
{
    JQHTTPSERVER_SESSION_PROTECTION( "peerCertificate", QSslCertificate() )

    if ( !qobject_cast< QSslSocket * >( ioDevice_ ) ) { return QSslCertificate(); }

    return qobject_cast< QSslSocket * >( ioDevice_ )->peerCertificate();
}
#endif

void Session::replyText(const QString &replyData, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyText" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = httpStatusCode;
        replyBodySize_ = replyData.toUtf8().size();

        QMetaObject::invokeMethod(
            this,
            "replyText",
            Qt::QueuedConnection,
            Q_ARG( QString, replyData ),
            Q_ARG( int, httpStatusCode ) );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyText" )

    const auto &&data = replyTextFormat
                            .arg(
                                QString::number( httpStatusCode ),
                                "text;charset=UTF-8",
                                QString::number( replyBodySize_ ),
                                replyData )
                            .toUtf8();

    waitWrittenByteCount_ = data.size();
    ioDevice_->write( data );
}

void Session::replyRedirects(const QUrl &targetUrl, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyRedirects" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = httpStatusCode;

        QMetaObject::invokeMethod( this, "replyRedirects", Qt::QueuedConnection, Q_ARG( QUrl, targetUrl ), Q_ARG( int, httpStatusCode ) );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyRedirects" )

    const auto &&buffer = QString( "<head>\n<meta http-equiv=\"refresh\" content=\"0;URL=%1/\" />\n</head>" ).arg( targetUrl.toString() );
    replyBodySize_ = buffer.toUtf8().size();

    const auto &&data = replyRedirectsFormat
                            .arg(
                                QString::number( httpStatusCode ),
                                "text;charset=UTF-8",
                                QString::number( replyBodySize_ ),
                                buffer )
                            .toUtf8();

    waitWrittenByteCount_ = data.size();
    ioDevice_->write( data );
}

void Session::replyJsonObject(const QJsonObject &jsonObject, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyJsonObject" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = httpStatusCode;
        replyBuffer_ = QJsonDocument( jsonObject ).toJson( QJsonDocument::Compact );
        replyBodySize_ = replyBuffer_.size();

        QMetaObject::invokeMethod( this, "replyJsonObject", Qt::QueuedConnection, Q_ARG( QJsonObject, jsonObject ), Q_ARG( int, httpStatusCode ) );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyJsonObject" )

    const auto &&buffer = replyTextFormat
                              .arg(
                                  QString::number( httpStatusCode ),
                                  "application/json;charset=UTF-8",
                                  QString::number( replyBodySize_ ),
                                  QString( replyBuffer_ ) )
                              .toUtf8();

    waitWrittenByteCount_ = buffer.size();
    ioDevice_->write( buffer );
}

void Session::replyJsonArray(const QJsonArray &jsonArray, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyJsonArray" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = httpStatusCode;
        replyBuffer_ = QJsonDocument( jsonArray ).toJson( QJsonDocument::Compact );
        replyBodySize_ = replyBuffer_.size();

        QMetaObject::invokeMethod( this, "replyJsonArray", Qt::QueuedConnection, Q_ARG( QJsonArray, jsonArray ), Q_ARG( int, httpStatusCode ) );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyJsonArray" )

    const auto &&buffer = replyTextFormat
                              .arg(
                                  QString::number( httpStatusCode ),
                                  "application/json;charset=UTF-8",
                                  QString::number( replyBodySize_ ),
                                  QString( replyBuffer_ ) )
                              .toUtf8();

    waitWrittenByteCount_ = buffer.size();
    ioDevice_->write( buffer );
}

void Session::replyFile(const QString &filePath, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyFile" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = httpStatusCode;

        QMetaObject::invokeMethod( this, "replyFile", Qt::QueuedConnection, Q_ARG( QString, filePath ), Q_ARG( int, httpStatusCode ) );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyFile" )

    replyIoDevice_.reset( new QFile( filePath ) );
    QPointer< QFile > file = ( qobject_cast< QFile * >( replyIoDevice_.data() ) );

    if ( !file->open( QIODevice::ReadOnly ) )
    {
        qDebug() << "Session::replyFile: open file error:" << filePath;
        replyIoDevice_.clear();
        this->deleteLater();
        return;
    }

    replyBodySize_ = file->size();

    const auto &&data = replyFileFormat
                            .arg(
                                QString::number( httpStatusCode ),
                                QFileInfo( filePath ).fileName(),
                                QString::number( replyBodySize_ ) )
                            .toUtf8();

    waitWrittenByteCount_ = data.size() + file->size();
    ioDevice_->write( data );
}

void Session::replyFile(const QString &fileName, const QByteArray &fileData, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyFile" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = httpStatusCode;

        QMetaObject::invokeMethod( this, "replyFile", Qt::QueuedConnection, Q_ARG( QString, fileName ), Q_ARG( QByteArray, fileData ), Q_ARG( int, httpStatusCode ) );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyFile" )

    auto buffer = new QBuffer;
    buffer->setData( fileData );

    if ( !buffer->open( QIODevice::ReadWrite ) )
    {
        qDebug() << "Session::replyFile: open buffer error";
        delete buffer;
        this->deleteLater();
        return;
    }

    replyIoDevice_.reset( buffer );
    replyIoDevice_->seek( 0 );

    replyBodySize_ = fileData.size();

    const auto &&data =
        replyFileFormat
            .arg( QString::number( httpStatusCode ), fileName, QString::number( replyBodySize_ ) )
            .toUtf8();

    waitWrittenByteCount_ = data.size() + fileData.size();
    ioDevice_->write( data );
}

void Session::replyImage(const QImage &image, const QString &format, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyImage" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = httpStatusCode;

        QMetaObject::invokeMethod( this, "replyImage", Qt::QueuedConnection, Q_ARG( QImage, image ), Q_ARG( QString, format ), Q_ARG( int, httpStatusCode ) );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyImage" )

    auto buffer = new QBuffer;

    if ( !buffer->open( QIODevice::ReadWrite ) )
    {
        qDebug() << "Session::replyImage: open buffer error";
        delete buffer;
        this->deleteLater();
        return;
    }

    if ( !image.save( buffer, format.toLatin1().constData() ) )
    {
        qDebug() << "Session::replyImage: save image to buffer error";
        delete buffer;
        this->deleteLater();
        return;
    }

    replyIoDevice_.reset( buffer );
    replyIoDevice_->seek( 0 );

    replyBodySize_ = buffer->buffer().size();

    const auto &&data =
        replyImageFormat
            .arg( QString::number( httpStatusCode ), format.toLower(), QString::number( replyBodySize_ ) )
            .toUtf8();

    waitWrittenByteCount_ = data.size() + buffer->buffer().size();
    ioDevice_->write( data );
}

void Session::replyImage(const QString &imageFilePath, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyImage" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = httpStatusCode;

        QMetaObject::invokeMethod( this, "replyImage", Qt::QueuedConnection, Q_ARG( QString, imageFilePath ), Q_ARG( int, httpStatusCode ) );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyImage" )

    auto file = new QFile( imageFilePath );

    if ( !file->open( QIODevice::ReadOnly ) )
    {
        qDebug() << "Session::replyImage: open buffer error";
        delete file;
        this->deleteLater();
        return;
    }

    replyIoDevice_.reset( file );
    replyIoDevice_->seek( 0 );

    replyBodySize_ = file->size();

    const auto &&data =
        replyImageFormat
            .arg( QString::number( httpStatusCode ), QFileInfo( imageFilePath ).suffix(), QString::number( replyBodySize_ ) )
            .toUtf8();

    waitWrittenByteCount_ = data.size() + file->size();
    ioDevice_->write( data );
}

void Session::replyBytes(const QByteArray &bytes, const QString &contentType, const int &httpStatusCode)
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyBytes" )

    if ( QThread::currentThread() != this->thread( ))
    {
        replyHttpCode_ = httpStatusCode;

        QMetaObject::invokeMethod(this, "replyBytes", Qt::QueuedConnection, Q_ARG(QByteArray, bytes), Q_ARG(QString, contentType), Q_ARG(int, httpStatusCode));
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyBytes" )

    auto buffer = new QBuffer;
    buffer->setData( bytes );

    if ( !buffer->open( QIODevice::ReadWrite ) )
    {
        qDebug() << "Session::replyBytes: open buffer error";
        delete buffer;
        this->deleteLater();
        return;
    }

    replyIoDevice_.reset( buffer );
    replyIoDevice_->seek( 0 );

    replyBodySize_ = buffer->buffer().size();

    const auto &&data =
        replyBytesFormat
            .arg( QString::number( httpStatusCode ), contentType, QString::number( replyBodySize_ ) )
            .toUtf8();

    waitWrittenByteCount_ = data.size() + buffer->buffer().size();
    ioDevice_->write(data);
}

void Session::replyOptions()
{
    JQHTTPSERVER_SESSION_REPLY_PROTECTION( "replyOptions" )

    if ( QThread::currentThread() != this->thread() )
    {
        replyHttpCode_ = 200;

        QMetaObject::invokeMethod( this, "replyOptions", Qt::QueuedConnection );
        return;
    }

    JQHTTPSERVER_SESSION_REPLY_PROTECTION2( "replyOptions" )

    replyBodySize_ = 0;

    const auto &&buffer = replyOptionsFormat.toUtf8();

    waitWrittenByteCount_ = buffer.size();
    ioDevice_->write( buffer );
}

void Session::inspectionBufferSetup1()
{
    if ( !headerAcceptedFinished_ )
    {
        forever
        {
            static QByteArray splitFlag( "\r\n" );

            auto splitFlagIndex = receiveBuffer_.indexOf( splitFlag );

            // 没有获取到分割标记，意味着数据不全
            if ( splitFlagIndex == -1 )
            {
                // 没有获取到 method 但是缓冲区内已经有了数据，这可能是一个无效的连接
                if ( requestMethod_.isEmpty() && ( receiveBuffer_.size() > 4 ) )
                {
//                    qDebug() << "Session::inspectionBuffer: error0";
                    this->deleteLater();
                    return;
                }

                return;
            }

            // 如果未获取到 method 并且已经定位到了分割标记符，那么直接放弃这个连接
            if ( requestMethod_.isEmpty() && ( splitFlagIndex == 0 ) )
            {
//                qDebug() << "Session::inspectionBuffer: error1";
                this->deleteLater();
                return;
            }

            // 如果没有获取到 method 则先尝试分析 method
            if ( requestMethod_.isEmpty() )
            {
                auto requestLineDatas = receiveBuffer_.mid( 0, splitFlagIndex ).split( ' ' );
                receiveBuffer_.remove( 0, splitFlagIndex + 2 );

                if ( requestLineDatas.size() != 3 )
                {
//                    qDebug() << "Session::inspectionBuffer: error2";
                    this->deleteLater();
                    return;
                }

                requestMethod_ = requestLineDatas.at( 0 );
                requestUrl_ = requestLineDatas.at( 1 );
                requestCrlf_ = requestLineDatas.at( 2 );

                if ( ( requestMethod_ != "GET" ) &&
                     ( requestMethod_ != "OPTIONS" ) &&
                     ( requestMethod_ != "DELETE" ) &&
                     ( requestMethod_ != "POST" ) &&
                     ( requestMethod_ != "PUT" ) &&
                     ( requestMethod_ != "HEAD" ) )
                {
//                    qDebug() << "Session::inspectionBuffer: error3:" << requestMethod_;
                    this->deleteLater();
                    return;
                }
            }
            else if ( splitFlagIndex == 0 )
            {
                receiveBuffer_.remove( 0, 2 );

                headerAcceptedFinished_ = true;

                if ( ( requestMethod_.toUpper() == "GET" ) ||
                     ( requestMethod_.toUpper() == "HEAD" )||
                     ( requestMethod_.toUpper() == "OPTIONS" ) ||
                     ( requestMethod_.toUpper() == "DELETE" ) ||
                     ( ( requestMethod_.toUpper() == "POST" ) && ( ( contentLength_ > 0 ) ? ( !receiveBuffer_.isEmpty() ) : ( true ) ) ) ||
                     ( ( requestMethod_.toUpper() == "PUT" ) && ( ( contentLength_ > 0 ) ? ( !receiveBuffer_.isEmpty() ) : ( true ) ) ) )
                {
                    this->inspectionBufferSetup2();
                }
            }
            else
            {
                auto index = receiveBuffer_.indexOf( ':' );

                if ( index <= 0 )
                {
//                    qDebug() << "Session::inspectionBuffer: error4";
                    this->deleteLater();
                    return;
                }

                auto headerData = receiveBuffer_.mid( 0, splitFlagIndex );
                receiveBuffer_.remove( 0, splitFlagIndex + 2 );

                const auto &&key = headerData.mid( 0, index );
                auto value = headerData.mid( index + 1 );

                if ( value.startsWith( ' ' ) )
                {
                    value.remove( 0, 1 );
                }

                requestHeader_[ key ] = value;

                if ( key.toLower() == "content-length" )
                {
                    contentLength_ = value.toLongLong();
                }
            }
        }
    }
    else
    {
        this->inspectionBufferSetup2();
    }
}

void Session::inspectionBufferSetup2()
{
    requestBody_ += receiveBuffer_;
    receiveBuffer_.clear();

    if ( !handleAcceptedCallback_ )
    {
        qDebug() << "Session::inspectionBuffer: error4";
        this->deleteLater();
        return;
    }

    if ( ( contentLength_ != -1 ) && ( requestBody_.size() != contentLength_ ) )
    {
        return;
    }

    if ( contentAcceptedFinished_ )
    {
        return;
    }

    contentAcceptedFinished_ = true;
    handleAcceptedCallback_( this );
}

void Session::onBytesWritten(const qint64 &written)
{
    if ( this->waitWrittenByteCount_ < 0 ) { return; }

    autoCloseTimer_->stop();

    this->waitWrittenByteCount_ -= written;
    if ( this->waitWrittenByteCount_ <= 0 )
    {
        this->waitWrittenByteCount_ = 0;
        QTimer::singleShot( 500, this, &QObject::deleteLater );
        return;
    }

    if ( !replyIoDevice_.isNull() )
    {
        if ( replyIoDevice_->atEnd() )
        {
            replyIoDevice_->deleteLater();
            replyIoDevice_.clear();
        }
        else
        {
            ioDevice_->write( replyIoDevice_->read( 512 * 1024 ) );
        }
    }

    autoCloseTimer_->start();
}
