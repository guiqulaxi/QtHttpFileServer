#include "Service.h"
#include <QtDebug>
#include <QMetaClassInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QPainter>
#include <QUuid>
#include <QMetaObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include "Session.h"
#include "TcpServerManage.h"
#ifndef QT_NO_SSL
#include "SslServerManage.h"
#endif
QSharedPointer< Service > Service::createService(const QMap< ServiceConfigEnum, QVariant > &config)
{
    QSharedPointer< Service > result( new Service );

    if ( !result->initialize( config ) ) { return { }; }

    return result;
}

void Service::registerProcessor( const QPointer< QObject > &processor )
{
    static QSet< QString > exceptionSlots( { "deleteLater", "_q_reregisterTimers" } );
    static QSet< QString > allowMethod( { "GET", "POST", "DELETE", "PUT"} );

    QString apiPathPrefix;
    for ( auto index = 0; index < processor->metaObject()->classInfoCount(); ++index )
    {
        if ( QString( processor->metaObject()->classInfo( 0 ).name() ) == "apiPathPrefix" )
        {
            apiPathPrefix = processor->metaObject()->classInfo( 0 ).value();
        }
    }

    for ( auto index = 0; index < processor->metaObject()->methodCount(); ++index )
    {
        const auto &&metaMethod = processor->metaObject()->method( index );
        if ( metaMethod.methodType() != QMetaMethod::Slot ) { continue; }

        ApiConfig api;

        api.process = processor;

        if ( metaMethod.name() == "sessionAccepted" )
        {
            schedules2_[ apiPathPrefix ] =
                [ = ]( const QPointer< Session > &session ) {
                    QMetaObject::invokeMethod(
                        processor,
                        "sessionAccepted",
                        Qt::DirectConnection,
                        Q_ARG( QPointer< Session >, session ) );
                };

            continue;
        }
        else if ( metaMethod.parameterTypes() == QList< QByteArray >( { "QPointer<Session>" } ) )
        {
            api.receiveDataType = NoReceiveDataType;
        }
        else if ( metaMethod.parameterTypes() == QList< QByteArray >( { "QVariantList", "QPointer<Session>" } ) )
        {
            api.receiveDataType = VariantListReceiveDataType;
        }
        else if ( metaMethod.parameterTypes() == QList< QByteArray >( { "QVariantMap", "QPointer<Session>" } ) )
        {
            api.receiveDataType = VariantMapReceiveDataType;
        }
        else if ( metaMethod.parameterTypes() == QList< QByteArray >( { "QList<QVariantMap>", "QPointer<Session>" } ) )
        {
            api.receiveDataType = ListVariantMapReceiveDataType;
        }
        else if ( metaMethod.name() == "certificateVerifier" )
        {
            certificateVerifier_ = processor;
        }
        else
        {
            continue;
        }

        api.slotName = QString( metaMethod.name() );
        if ( exceptionSlots.contains( api.slotName ) ) { continue; }

        for ( const auto &methdo: allowMethod )
        {
            if ( api.slotName.startsWith( methdo.toLower() ) )
            {
                api.apiMethod = methdo;
                break;
            }
        }
        if ( api.apiMethod.isEmpty() ) { continue; }

        api.apiName = api.slotName.mid( api.apiMethod.size() );
        if ( api.apiName.isEmpty() ) { continue; }

        api.apiName[ 0 ] = api.apiName[ 0 ].toLower();
        api.apiName.push_front( "/" );

        if ( !apiPathPrefix.isEmpty() )
        {
            api.apiName = apiPathPrefix + api.apiName;
        }

        schedules_[ api.apiMethod.toUpper() ][ api.apiName ] = api;
    }
}

QJsonDocument Service::extractPostJsonData(const QPointer< Session > &session)
{
    return QJsonDocument::fromJson( session->requestBody() );
}

void Service::reply(
    const QPointer< Session > &session,
    const QJsonObject &data,
    const bool &isSucceed,
    const QString &message,
    const int &httpStatusCode )
{
    QJsonObject result;
    result[ "isSucceed" ] = isSucceed;
    result[ "message" ] = message;

    if ( !data.isEmpty() )
    {
        result[ "data" ] = data;
    }

    session->replyJsonObject( result, httpStatusCode );
}

void Service::reply(
    const QPointer< Session > &session,
    const bool &isSucceed,
    const QString &message,
    const int &httpStatusCode )
{
    reply( session, QJsonObject(), isSucceed, message, httpStatusCode );
}

void Service::httpGetPing(const QPointer< Session > &session)
{
    QJsonObject data;
    data[ "serverTime" ] = QDateTime::currentMSecsSinceEpoch();

    reply( session, data );
}

void Service::httpGetFaviconIco(const QPointer< Session > &session)
{
    QImage image( 256, 256, QImage::Format_ARGB32 );
    image.fill( 0x0 );

    QPainter painter( &image );
    painter.setPen( Qt::NoPen );
    painter.setBrush( QColor( "#ff00ff" ) );
    painter.drawEllipse( 16, 16, 224, 224 );
    painter.end();

    session->replyImage( image );
}

void Service::httpOptions(const QPointer< Session > &session)
{
    session->replyOptions();
}

bool Service::initialize( const QMap< ServiceConfigEnum, QVariant > &config )
{
    if ( config.contains( ServiceProcessor ) &&
         config[ ServiceProcessor ].canConvert< QPointer< QObject > >() &&
         !config[ ServiceProcessor ].value< QPointer< QObject > >().isNull() )
    {
        this->registerProcessor( config[ ServiceProcessor ].value< QPointer< QObject > >() );
    }

    if ( config.contains( ServiceProcessor ) &&
         config[ ServiceProcessor ].canConvert< QList< QPointer< QObject > > >() )
    {
        for ( const auto &process: config[ ServiceProcessor ].value< QList< QPointer< QObject > > >() )
        {
            if ( !process ) { continue; }

            this->registerProcessor( process );
        }
    }

    const auto httpPort = static_cast< quint16 >( config[ ServiceHttpListenPort ].toInt() );
    if ( httpPort > 0 )
    {
        this->httpServerManage_.reset( new TcpServerManage );
        this->httpServerManage_->setHttpAcceptedCallback( std::bind( &Service::onSessionAccepted, this, std::placeholders::_1 ) );

        if ( !this->httpServerManage_->listen(
                 QHostAddress::Any,
                 httpPort
             ) )
        {
            qWarning() << "Service: listen port error:" << httpPort;
            return false;
        }
    }
#ifndef QT_NO_SSL

    const auto httpsPort = static_cast< quint16 >( config[ ServiceHttpsListenPort ].toInt() );
    if ( httpsPort > 0 )
    {
        this->httpsServerManage_.reset( new SslServerManage );
        this->httpsServerManage_->setHttpAcceptedCallback( std::bind( &Service::onSessionAccepted, this, std::placeholders::_1 ) );

        QSslSocket::PeerVerifyMode peerVerifyMode = QSslSocket::VerifyNone;
        if ( config.contains( ServiceSslPeerVerifyMode ) )
        {
            peerVerifyMode = static_cast< QSslSocket::PeerVerifyMode >( config[ ServiceSslPeerVerifyMode ].toInt() );
        }

        QString crtFilePath = config[ ServiceSslCrtFilePath ].toString();
        QString keyFilePath = config[ ServiceSslKeyFilePath ].toString();
        if ( crtFilePath.isEmpty() || keyFilePath.isEmpty() )
        {
            qWarning() << "Service: crt or key file path error";
            return false;
        }

        QList< QPair< QString, QSsl::EncodingFormat > > caFileList;
        for ( const auto &caFilePath: config[ ServiceSslCAFilePath ].toStringList() )
        {
            QPair< QString, QSsl::EncodingFormat > pair;
            pair.first = caFilePath;
            pair.second = QSsl::Pem;
            caFileList.push_back( pair );
        }

        if ( !this->httpsServerManage_->listen(
                 QHostAddress::Any,
                 httpsPort,
                 crtFilePath,
                 keyFilePath,
                 caFileList,
                 peerVerifyMode
             ) )
        {
            qWarning() << "Service: listen port error:" << httpsPort;
            return false;
        }
    }

    const auto serviceUuid = config[ ServiceUuid ].toString();
    if ( !QUuid( serviceUuid ).isNull() )
    {
        this->serviceUuid_ = serviceUuid;
    }

#endif
    return true;
}

void Service::onSessionAccepted(const QPointer< Session > &session)
{
    if ( certificateVerifier_ && qobject_cast< QSslSocket * >( session->ioDevice() ) )
    {
        QMetaObject::invokeMethod(
                    certificateVerifier_,
                    "certificateVerifier",
                    Qt::DirectConnection,
                    Q_ARG( QSslCertificate, session->peerCertificate() ),
                    Q_ARG( QPointer< Session >, session )
                );

        if ( session->replyHttpCode() >= 0 ) { return; }
    }

    const auto schedulesIt = schedules_.find( session->requestMethod() );
    if ( schedulesIt != schedules_.end() )
    {
        auto apiName = session->requestUrlPath();

        auto it = schedulesIt.value().find( session->requestUrlPath() );
        if ( ( it == schedulesIt.value().end() ) && ( session->requestUrlPath().contains( "_" ) ) )
        {
            apiName = Service::snakeCaseToCamelCase( session->requestUrlPath() );
            it = schedulesIt.value().find( apiName );
        }

        if ( it != schedulesIt.value().end() )
        {
            Recoder recoder( session );
            recoder.serviceUuid_ = serviceUuid_;
            recoder.apiName = apiName;

            switch ( it->receiveDataType )
            {
                case NoReceiveDataType:
                {
                    QMetaObject::invokeMethod(
                                it->process,
                                it->slotName.toLatin1().data(),
                                Qt::DirectConnection,
                                Q_ARG( QPointer< Session >, session )
                            );
                    return;
                }
                case VariantListReceiveDataType:
                {
                    const auto &&json = this->extractPostJsonData( session );
                    if ( !json.isNull() )
                    {
                        QMetaObject::invokeMethod(
                                    it->process,
                                    it->slotName.toLatin1().data(),
                                    Qt::DirectConnection,
                                    Q_ARG( QVariantList, json.array().toVariantList() ),
                                    Q_ARG( QPointer< Session >, session )
                                );
                        return;
                    }
                    break;
                }
                case VariantMapReceiveDataType:
                {
                    const auto &&json = this->extractPostJsonData( session );
                    if ( !json.isNull() )
                    {
                        QMetaObject::invokeMethod(
                                    it->process,
                                    it->slotName.toLatin1().data(),
                                    Qt::DirectConnection,
                                    Q_ARG( QVariantMap, json.object().toVariantMap() ),
                                    Q_ARG( QPointer< Session >, session )
                                );
                        return;
                    }
                    break;
                }
                case ListVariantMapReceiveDataType:
                {
                    const auto &&json = this->extractPostJsonData( session );
                    if ( !json.isNull() )
                    {
                        QMetaObject::invokeMethod(
                                    it->process,
                                    it->slotName.toLatin1().data(),
                                    Qt::DirectConnection,
                                    Q_ARG( QList< QVariantMap >, Service::variantListToListVariantMap( json.array().toVariantList() ) ),
                                    Q_ARG( QPointer< Session >, session )
                                );
                        return;
                    }
                    break;
                }
                default:
                {
                    qDebug() << "onSessionAccepted: data type not match:" << it->receiveDataType;
                    reply( session, false, "data type not match", 404 );
                    return;
                }
            }

            qDebug() << "onSessionAccepted: data error:" << it->receiveDataType;
            reply( session, false, "data error", 404 );
            return;
        }
    }

    for ( auto it = schedules2_.begin(); it != schedules2_.end(); ++it )
    {
        if ( session->requestUrlPath().startsWith( it.key() ) )
        {
            it.value()( session );
            return;
        }
    }

    if ( ( session->requestMethod() == "GET" ) && ( session->requestUrlPath() == "/ping" ) )
    {
        this->httpGetPing( session );
        return;
    }
    else if ( ( session->requestMethod() == "GET" ) && ( session->requestUrlPath() == "/favicon.ico" ) )
    {
        this->httpGetFaviconIco( session );
        return;
    }
    else if ( session->requestMethod() == "OPTIONS" )
    {
        this->httpOptions( session );
        return;
    }

    qDebug().noquote() << "API not found:" << session->requestMethod() << session->requestUrlPath();
    reply( session, false, "API not found", 404 );
}

QString Service::snakeCaseToCamelCase(const QString &source, const bool &firstCharUpper)
{
    const auto &&splitList = source.split( '_', QString::SkipEmptyParts );
    QString result;

    for ( const auto &splitTag: splitList )
    {
        if ( splitTag.size() == 1 )
        {
            if ( result.isEmpty() )
            {
                if ( firstCharUpper )
                {
                    result += splitTag[ 0 ].toUpper();
                }
                else
                {
                    result += splitTag;
                }
            }
            else
            {
                result += splitTag[ 0 ].toUpper();
            }
        }
        else
        {
            if ( result.isEmpty() )
            {
                if ( firstCharUpper )
                {
                    result += splitTag[ 0 ].toUpper();
                    result += splitTag.midRef( 1 );
                }
                else
                {
                    result += splitTag;
                }
            }
            else
            {
                result += splitTag[ 0 ].toUpper();
                result += splitTag.midRef( 1 );
            }
        }
    }

    return result;
}

QList< QVariantMap > Service::variantListToListVariantMap(const QVariantList &source)
{
    QList< QVariantMap > result;

    for ( const auto &item: source )
    {
        result.push_back( item.toMap() );
    }

    return result;
}

Service::Recoder::Recoder(const QPointer< Session > &session)
{
    qDebug() << "HTTP accepted:" << session->requestMethod().toLatin1().data() << session->requestUrlPath().toLatin1().data();

    session_ = session;
    acceptedTime_ = QDateTime::currentDateTime();
}

Service::Recoder::~Recoder()
{
    if ( !session_ )
    {
        return;
    }

    const auto &&replyTime = QDateTime::currentDateTime();
    const auto &&elapsed = replyTime.toMSecsSinceEpoch() - acceptedTime_.toMSecsSinceEpoch();

    qDebug().noquote() << "HTTP finished:" << QString::number( elapsed ).rightJustified( 3, ' ' )
                       << "ms, code:" << session_->replyHttpCode()
                       << ", accepted:" << QString::number( session_->requestBody().size() ).rightJustified( 3, ' ' )
                       << ", reply:" << QString::number( session_->replyBodySize() ).rightJustified( 3, ' ' );
}
