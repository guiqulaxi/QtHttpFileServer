#ifndef TCPSERVERMANAGE_H
#define TCPSERVERMANAGE_H
class QTcpServer;
#include "AbstractManage.h"
#include  <QHostAddress>
#include <QPointer>

class  TcpServerManage: public AbstractManage
{
    Q_OBJECT
    Q_DISABLE_COPY( TcpServerManage )

public:
    TcpServerManage(const int &handleMaxThreadCount = 2);

    ~TcpServerManage();

    bool listen( const QHostAddress &address, const quint16 &port );

private:
    bool isRunning();

    bool onStart();

    void onFinish();

private:
    QPointer<QTcpServer > tcpServer_;

    QHostAddress listenAddress_ = QHostAddress::Any;
    quint16 listenPort_ = 0;
};




#endif // TCPSERVERMANAGE_H
