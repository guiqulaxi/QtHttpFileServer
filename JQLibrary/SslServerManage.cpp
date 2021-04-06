#include "SslServerManage.h"
#include <QTcpServer>
#include <QSslConfiguration>
#include <QSslKey>
#include "Session.h"
class SslServerHelper: public QTcpServer
{
    void incomingConnection(qintptr socketDescriptor) final
    {
        onIncomingConnectionCallback_( socketDescriptor );
    }

public:
    std::function< void(qintptr socketDescriptor) > onIncomingConnectionCallback_;
};
SslServerManage::SslServerManage(const int &handleMaxThreadCount):
    AbstractManage( handleMaxThreadCount )
{ }

SslServerManage::~SslServerManage()
{
    if ( this->isRunning() )
    {
        this->deinitialize();
    }
}

bool SslServerManage::listen(
        const QHostAddress &address,
        const quint16 &port,
        const QString &crtFilePath,
        const QString &keyFilePath,
        const QList< QPair< QString, QSsl::EncodingFormat > > &caFileList,
        const QSslSocket::PeerVerifyMode &peerVerifyMode
    )
{
    listenAddress_ = address;
    listenPort_ = port;

    QFile fileForCrt( crtFilePath );
    if ( !fileForCrt.open( QIODevice::ReadOnly ) )
    {
        qDebug() << "SslServerManage::listen: error: can not open file:" << crtFilePath;
        return false;
    }

    QFile fileForKey( keyFilePath );
    if ( !fileForKey.open( QIODevice::ReadOnly ) )
    {
        qDebug() << "SslServerManage::listen: error: can not open file:" << keyFilePath;
        return false;
    }

    QSslCertificate sslCertificate( fileForCrt.readAll(), QSsl::Pem );
    QSslKey sslKey( fileForKey.readAll(), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey );

    QList< QSslCertificate > caCertificates;
    for ( const auto &caFile: caFileList )
    {
        QFile fileForCa( caFile.first );
        if ( !fileForCa.open( QIODevice::ReadOnly ) )
        {
            qDebug() << "SslServerManage::listen: error: can not open file:" << caFile.first;
            return false;
        }

        caCertificates.push_back( QSslCertificate( fileForCa.readAll(), caFile.second ) );
    }

    sslConfiguration_.reset( new QSslConfiguration );
    sslConfiguration_->setPeerVerifyMode( peerVerifyMode );
    sslConfiguration_->setPeerVerifyDepth( 1 );
    sslConfiguration_->setLocalCertificate( sslCertificate );
    sslConfiguration_->setPrivateKey( sslKey );
    sslConfiguration_->setProtocol( QSsl::TlsV1_1OrLater );
    sslConfiguration_->setCaCertificates( caCertificates );

    return this->initialize();
}

bool SslServerManage::isRunning()
{
    return !tcpServer_.isNull();
}

bool SslServerManage::onStart()
{
    mutex_.lock();

    tcpServer_ = new SslServerHelper;

    mutex_.unlock();

    tcpServer_->onIncomingConnectionCallback_ = [ this ](qintptr socketDescriptor)
    {
//        qDebug() << "incomming";

        auto sslSocket = new QSslSocket;

        sslSocket->setSslConfiguration( *sslConfiguration_ );

        QObject::connect( sslSocket, &QSslSocket::encrypted, [ this, sslSocket ]()
        {
            this->newSession( new Session( sslSocket ) );
        } );

//        QObject::connect( sslSocket, static_cast< void(QSslSocket::*)(const QList<QSslError> &errors) >(&QSslSocket::sslErrors), [](const QList<QSslError> &errors)
//        {
//            qDebug() << "sslErrors:" << errors;
//        } );

        sslSocket->setSocketDescriptor( socketDescriptor );
        sslSocket->startServerEncryption();
    };

    if ( !tcpServer_->listen( listenAddress_, listenPort_ ) )
    {
        mutex_.lock();

        delete tcpServer_.data();
        tcpServer_.clear();

        mutex_.unlock();

        return false;
    }

    return true;
}

void SslServerManage::onFinish()
{
    this->mutex_.lock();

    tcpServer_->close();
    delete tcpServer_.data();
    tcpServer_.clear();

    this->mutex_.unlock();
}
