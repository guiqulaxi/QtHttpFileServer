#include "TcpServerManage.h"
#include <QTcpServer>
#include <QTcpSocket>
#include "Session.h"
TcpServerManage::TcpServerManage(const int &handleMaxThreadCount):
    AbstractManage( handleMaxThreadCount )
{ }

TcpServerManage::~TcpServerManage()
{
    if ( this->isRunning() )
    {
        this->deinitialize();
    }
}

bool TcpServerManage::listen(const QHostAddress &address, const quint16 &port)
{
    listenAddress_ = address;
    listenPort_ = port;

    return this->initialize();
}

bool TcpServerManage::isRunning()
{
    return !tcpServer_.isNull();
}

bool TcpServerManage::onStart()
{
    mutex_.lock();

    tcpServer_ = new QTcpServer;

    mutex_.unlock();

    QObject::connect( tcpServer_.data(), &QTcpServer::newConnection, [ this ]()
    {
        auto socket = this->tcpServer_->nextPendingConnection();

        this->newSession( new Session( socket ) );
    } );

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

void TcpServerManage::onFinish()
{
    this->mutex_.lock();

    tcpServer_->close();
    delete tcpServer_.data();
    tcpServer_.clear();

    this->mutex_.unlock();
}
