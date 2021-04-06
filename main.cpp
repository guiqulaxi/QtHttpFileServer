// Qt lib import
#include <QtCore>
#include <QImage>
#include <QFile>
#include <QMap>
#include<functional>
#include<QRegularExpression>
#include "Setting.h"
#include<QPair>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QUrl>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <JQLibrary/Session.h>
#include <JQLibrary/SslServerManage.h>
#include <JQLibrary/TcpServerManage.h>
#include <QReadWriteLock>
#include <QWriteLocker>
#include <QReadLocker>
using  requestFunc=std::function< void(const QPointer< Session > & ,const QString & path) >;

static QReadWriteLock gReadWriteLock;
void fileDownload(const QPointer< Session > &session,const QString & path)
{

    QReadLocker locker(&gReadWriteLock);
    qInfo()<<"fileDownload:begin";
    QDir dir(qSetting->rootDirectory());
    QFileInfo fi(dir.absoluteFilePath(path));
    QString abs=fi.absoluteFilePath();
    if(fi.isFile())//准确判断文件是否存在
    {
        session->replyFile(fi.absoluteFilePath());
    }
    else
    {
        session->replyText("File not found",404);
    }
    qInfo()<<"fileDownload:end";
}
void fileInform(const QPointer< Session > &session,const QString & path)
{
    QReadLocker locker(&gReadWriteLock);
    qInfo()<<"fileInform:begin";
    QDir dir(qSetting->rootDirectory());
    QFileInfo fi(dir.absoluteFilePath(path));
    QString abs=fi.absoluteFilePath();
    if(fi.isFile())//准确判断文件是否存在
    {

        QJsonObject jsonObj;
        jsonObj.insert("Size",fi.size());
        jsonObj.insert("LastModified", (qint64)fi.lastModified().toTime_t());
        jsonObj.insert("LastRead", (qint64)fi.lastRead().toTime_t());
        jsonObj.insert("IsReadable",fi.isReadable());
        jsonObj.insert("IsWritable", fi.isWritable());
        session->replyJsonObject(jsonObj);
    }
    else
    {
        session->replyText("File not found",404);
    }
    qInfo()<<"fileInform:end";
}
void dirList(const QPointer< Session > &session,const QString & path)
{

    QReadLocker locker(&gReadWriteLock);
    qInfo()<<"dirList:begin";
    QDir dir(qSetting->rootDirectory());
    QFileInfo di(dir.absoluteFilePath(path));
    QString absPath=di.absoluteFilePath();
    QJsonObject jsonObj;
    if(di.isDir())
    {
        foreach(auto fname,QDir(absPath).entryList())
        {
            QFileInfo fi(QDir(absPath).absoluteFilePath(fname));
            QJsonObject fobj;
            fobj.insert("IsDir", fi.isDir());
            fobj.insert("Size",fi.size());
            fobj.insert("LastModified", (qint64)fi.lastModified().toTime_t());
            fobj.insert("LastRead", (qint64)fi.lastRead().toTime_t());
            fobj.insert("IsReadable",fi.isReadable());
            fobj.insert("IsWritable", fi.isWritable());
            jsonObj.insert(fname,fobj);
        }

        session->replyJsonObject( jsonObj);

    }
    else
    {
        session->replyText("Directory not found",404);
    }
    qInfo()<<"dirList:end";
}
void fileUpload(const QPointer< Session > &session,const QString & path)
{
    QWriteLocker locker(&gReadWriteLock);
    qInfo()<<"fileUpload:begin";
    QDir dir(qSetting->rootDirectory());
    QFileInfo di(dir.absoluteFilePath(path));
    QString absPath=di.absoluteFilePath();
    if(dir.mkpath(di.dir().absolutePath()))
    {
        QFile file(absPath);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(session->requestBody());
            file.close();
            session->replyText("Upload successed");
        }else
        {
            session->replyText("Upload failed",400);
        }
    }
    else
    {
        session->replyText("Upload failed",400);
    }
    qInfo()<<"fileUpload:end";

}
void fileDelete(const QPointer< Session > &session,const QString & path)
{
    QWriteLocker locker(&gReadWriteLock);
    qInfo()<<"fileDelete:begin";
    QDir dir(qSetting->rootDirectory());
    QFileInfo fi(dir.absoluteFilePath(path));
    QString abs=fi.absoluteFilePath();
    if(fi.isFile())//准确判断文件是否存在
    {
        if(QFile::remove(abs))
        {
             session->replyText("Delete successed");
        }
        else
        {
             session->replyText("Delete failed",400);
        }

    }
    else
    {
        session->replyText("File not found",404);
    }
    qInfo()<<"fileDelete:end";
}
void dirDelete(const QPointer< Session > &session,const QString & path)
{
    QWriteLocker locker(&gReadWriteLock);
    qInfo()<<"dirDelete:begin";
    QDir dir(qSetting->rootDirectory());
    QFileInfo fi(dir.absoluteFilePath(path));
    QString abs=fi.absoluteFilePath();
    if(fi.isDir())//准确判断文件是否存在
    {
        QDir qDir(abs);
        if(qDir.removeRecursively())
        {
            session->replyText("Delete successed");
        }else
        {
            session->replyText("Delete failed",400);
        }

    }
    else
    {
        session->replyText("File not found",404);
    }
     qInfo()<<"dirDelete:end";
}

static QHash<QString , requestFunc> gRequestMapping;
static QString gHashFormat="%1,%2,%3";
static auto init=[]()-> int
{
        //GET localhost:2345/files/f1/data
        gRequestMapping.insert(gHashFormat.arg("GET").arg("files").arg("data"),
                                   fileDownload);
        //GET localhost:2345/files/f1/atrr
        gRequestMapping.insert(gHashFormat.arg("GET").arg("files").arg("attr"),
                                   fileInform);
        //GET localhost:2345/files/f1/list
        gRequestMapping.insert(gHashFormat.arg("GET").arg("dirs").arg("list"),
                                   dirList);
        //POST localhost:2345/files/f1/data
        gRequestMapping.insert(gHashFormat.arg("POST").arg("files").arg("data"),
                                   fileUpload);
        //DELETE localhost:2345/files/f1/data
        gRequestMapping.insert(gHashFormat.arg("DELETE").arg("files").arg("data"),
                                   fileDelete);
        //DELETE localhost:2345/files/f1/data
        gRequestMapping.insert(gHashFormat.arg("DELETE").arg("dirs").arg("data"),
                                   dirDelete);
        return 0;
        };

static int _init=init();

void onHttpAccepted(const QPointer< Session > &session)
{

    QString requestMethod= session->requestMethod();
    QString requestSourceIp= session->requestSourceIp();
    QString url= session->requestUrl();

    auto qUrl=QUrl(url);
    QString path=qUrl.path();
    path.lastIndexOf("/");
    auto content=path.right(path.size() - (path.lastIndexOf("/")+1));
    int start =path.indexOf("/");
    int end =path.indexOf("/",start+1);
    auto type=path.mid(start+1,  end-start-1);

    auto filepath=path.mid(end+1,  path.lastIndexOf("/")-end-1);

    qInfo()<<requestMethod<<":"<<qUrl.toString();
    QString router=gHashFormat.arg(requestMethod).arg(type).arg(content);
    if(gRequestMapping.contains(router))
    {
        gRequestMapping[router](session,filepath);
    }
    else
    {
        session->replyText("Operation not supported",406);
    }

    // 注1：因为一个session对应一个单一的HTTP请求，所以session只能reply一次
    // 注2：在reply后，session的生命周期不可控，所以reply后不要再调用session的接口了
}

void initializeHttpServer()
{
    static TcpServerManage tcpServerManage( 2 ); // 设置最大处理线程数，默认2个
    tcpServerManage.setHttpAcceptedCallback( std::bind( onHttpAccepted, std::placeholders::_1 ) );
    auto listenSucceed =false;
    QHostAddress hostAddress(qSetting->ip());
    if(hostAddress.isNull())
    {
        listenSucceed = tcpServerManage.listen( hostAddress, qSetting->port() );
        qInfo()<< "HTTPS server listen:" << qSetting->ip()<<":"<<qSetting->port()<<" "<<listenSucceed;

    }else
    {
        listenSucceed = tcpServerManage.listen( QHostAddress::Any, qSetting->port() );
        qInfo()<< "HTTPS server listen:" << "Any:"<<qSetting->port()<<" "<<listenSucceed;

    }

}

void initializeHttpsServer()
{
#ifndef QT_NO_SSL
    static SslServerManage sslServerManage( 2 ); // 设置最大处理线程数，默认2个
    sslServerManage.setHttpAcceptedCallback( std::bind( onHttpAccepted, std::placeholders::_1 ) );

    auto listenSucceed =false;
    QHostAddress hostAddress(qSetting->ip());
    if(hostAddress.isNull())
    {
        listenSucceed =sslServerManage.listen( hostAddress, qSetting->port(), ":/server.crt", ":/server.key" );
        qDebug() << "HTTP server listen:" << qSetting->ip()<<":"<<qSetting->port()<<" "<<listenSucceed;
    }else
    {
        listenSucceed = sslServerManage.listen( QHostAddress::Any, qSetting->port(), ":/server.crt", ":/server.key" );
        qDebug() << "HTTPS server listen:" << "Any:"<<qSetting->port()<<" "<<listenSucceed;
    }



    // 这是我在一个实际项目中用的配置（用认证的证书，访问时带绿色小锁），其中涉及到隐私的细节已经被替换，但是任然能够看出整体用法
    /*
    qDebug() << "listen:" << sslServerManage.listen(
                    QHostAddress::Any,
                    24684,
                    "xxx/__xxx_com.crt",
                    "xxx/__xxx_com.key",
                    {
                        { "xxx/__xxx_com.ca-bundle", QSsl::Pem },
                        { "xxx/COMODO RSA Certification Authority.crt", QSsl::Pem },
                        { "xxx/COMODO RSA Domain Validation Secure Server CA.cer", QSsl::Der }
                    }
                );
    */
#else
    qDebug() << "SSL not support"
            #endif
}

                int main(int argc, char *argv[])
    {
                qputenv( "QT_SSL_USE_TEMPORARY_KEYCHAIN", "1" );
                qSetMessagePattern( "%{time hh:mm:ss.zzz}: %{message}" );
                qSetting->setRootDirectory("/home/sun");



                QCoreApplication app( argc, argv );
            #ifdef NO_SSL
                initializeHttpServer();
            #else
                initializeHttpsServer();
            #endif
                return app.exec();
}
