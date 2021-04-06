#include "QHttpFileClient.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
QHttpFileClient::QHttpFileClient(QObject *parent) : QObject(parent),
    m_strRootURL("http://localhost:2345/")
{

}
int QHttpFileClient::downloadFile(const QString& filePath,const QString& localPath)
{

      QString  strUrl(m_strRootURL+"files/"+ filePath+"/data");
      QUrl url(  strUrl);
      QNetworkRequest networkRequest(url);
      QNetworkReply* reply=  m_networkAccessManager.get(networkRequest);
      QSharedPointer< bool > isCalled( new bool( false ) );

      QFileInfo di(localPath);
      QFile file(localPath);
      if(di.dir().mkpath(di.dir().absolutePath()))
      {
          if (!file.open(QIODevice::WriteOnly))
          {
              //
              file.write(reply->readAll());
              return -1;
          }
      }
      QEventLoop loop;
      QObject::connect( reply, &QNetworkReply::finished, [isCalled,&loop ]()
      {
          if ( *isCalled ) { return; }
          *isCalled = true;
           loop.exit(0);
      } );

#ifndef QT_NO_SSL
    if ( reply->url().toString().toLower().startsWith( "https" ) )
    {
        QObject::connect( reply, static_cast< void( QNetworkReply::* )( const QList< QSslError > & ) >( &QNetworkReply::sslErrors ), [ reply ](const QList< QSslError > & /*errors*/)
        {
            reply->ignoreSslErrors();
        } );
    }
#endif

    QObject::connect( reply, static_cast< void( QNetworkReply::* )( QNetworkReply::NetworkError ) >( &QNetworkReply::error ), [ reply, isCalled,&loop ](const QNetworkReply::NetworkError &code)
    {
        if ( *isCalled ) { return; }
        *isCalled = true;
        loop.exit(-1);

    } );

    QObject::connect( reply, &QNetworkReply::readyRead,[reply,&file]()
    {
        if(file.isOpen())
        {
            file.write(reply->readAll());
        }
    } );

    loop.exec();

    if(reply->error()==QNetworkReply::NoError){

        //状态码

    }else{
        //获取响应信息
        const QByteArray reply_data=reply->readAll();
        qInfo()<<"read all:"<<reply_data;
    }

    qInfo()<<"GET,"<<strUrl<<","<<reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if(file.isOpen())
    {
        file.close();
    }
    return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

}
int QHttpFileClient::uploadFile(const QString& filePath,const QString& localPath)
{

    QString  strUrl(m_strRootURL+"files/"+ filePath+"/data");
    QUrl url(  strUrl);
    QNetworkRequest networkRequest(url);

    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,"application/octet-stream ");
    QFile file(localPath);

        if (!file.open(QIODevice::ReadOnly))
        {
            return -1;
        }
    QNetworkReply* reply=  m_networkAccessManager.post(networkRequest,&file);


    QSharedPointer< bool > isCalled( new bool( false ) );

    QEventLoop loop;
    QObject::connect( reply, &QNetworkReply::finished, [isCalled,&loop ]()
    {
        if ( *isCalled ) { return; }
        *isCalled = true;
         loop.exit(0);
    } );

#ifndef QT_NO_SSL
  if ( reply->url().toString().toLower().startsWith( "https" ) )
  {
      QObject::connect( reply, static_cast< void( QNetworkReply::* )( const QList< QSslError > & ) >( &QNetworkReply::sslErrors ), [ reply ](const QList< QSslError > & /*errors*/)
      {
          reply->ignoreSslErrors();
      } );
  }
#endif

  QObject::connect( reply, static_cast< void( QNetworkReply::* )( QNetworkReply::NetworkError ) >( &QNetworkReply::error ), [ reply, isCalled,&loop ](const QNetworkReply::NetworkError &code)
  {
      if ( *isCalled ) { return; }
      *isCalled = true;
      loop.exit(-1);

  } );

  loop.exec();

  if(reply->error()==QNetworkReply::NoError){

      //状态码

  }else{
      //获取响应信息
      const QByteArray reply_data=reply->readAll();
      qInfo()<<"read all:"<<reply_data;
  }

  qInfo()<<"POST,"<<strUrl<<","<<reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  if(file.isOpen())
  {
      file.close();
  }
  return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}
int QHttpFileClient::deleteFile(const QString& filePath)
{

    QString  strUrl(m_strRootURL+"files/"+ filePath+"/data");
    QUrl url(  strUrl);
    QNetworkRequest networkRequest(url);
    QNetworkReply* reply=  m_networkAccessManager.deleteResource(networkRequest);
    QSharedPointer< bool > isCalled( new bool( false ) );

    QEventLoop loop;
    QObject::connect( reply, &QNetworkReply::finished, [isCalled,&loop ]()
    {
        if ( *isCalled ) { return; }
        *isCalled = true;
         loop.exit(0);
    } );

#ifndef QT_NO_SSL
  if ( reply->url().toString().toLower().startsWith( "https" ) )
  {
      QObject::connect( reply, static_cast< void( QNetworkReply::* )( const QList< QSslError > & ) >( &QNetworkReply::sslErrors ), [ reply ](const QList< QSslError > & /*errors*/)
      {
          reply->ignoreSslErrors();
      } );
  }
#endif

  QObject::connect( reply, static_cast< void( QNetworkReply::* )( QNetworkReply::NetworkError ) >( &QNetworkReply::error ), [ isCalled,&loop ](const QNetworkReply::NetworkError &code)
  {
      if ( *isCalled ) { return; }
      *isCalled = true;
      loop.exit(-1);

  } );

  loop.exec();

  if(reply->error()==QNetworkReply::NoError){

      //状态码

  }else{
      //获取响应信息
      const QByteArray reply_data=reply->readAll();
      qInfo()<<"read all:"<<reply_data;
  }

  qInfo()<<"DELETE,"<<strUrl<<","<<reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

}
int  QHttpFileClient::informFile(const QString& filePath,QString& content)
{

    QString  strUrl(m_strRootURL+"files/"+ filePath+"/attr");
    QUrl url(  strUrl);
    QNetworkRequest networkRequest(url);
    QNetworkReply* reply=  m_networkAccessManager.get(networkRequest);
    QSharedPointer< bool > isCalled( new bool( false ) );

    QEventLoop loop;
    QObject::connect( reply, &QNetworkReply::finished, [isCalled,&loop ]()
    {
        if ( *isCalled ) { return; }
        *isCalled = true;
         loop.exit(0);
    } );

#ifndef QT_NO_SSL
  if ( reply->url().toString().toLower().startsWith( "https" ) )
  {
      QObject::connect( reply, static_cast< void( QNetworkReply::* )( const QList< QSslError > & ) >( &QNetworkReply::sslErrors ), [ reply ](const QList< QSslError > & /*errors*/)
      {
          reply->ignoreSslErrors();
      } );
  }
#endif

  QObject::connect( reply, static_cast< void( QNetworkReply::* )( QNetworkReply::NetworkError ) >( &QNetworkReply::error ), [ isCalled,&loop ](const QNetworkReply::NetworkError &code)
  {
      if ( *isCalled ) { return; }
      *isCalled = true;
      loop.exit(-1);

  } );

  loop.exec();

  if(reply->error()==QNetworkReply::NoError){


    content=reply->readAll();
  }else{
      //获取响应信息
       content=reply->readAll();
  }

  qInfo()<<"GET,"<<strUrl<<","<<reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}
int QHttpFileClient::listDir(const QString& dirPath,QString& content)
{

    QString  strUrl(m_strRootURL+"dirs/"+ dirPath+"/list");
    QUrl url(  strUrl);
    QNetworkRequest networkRequest(url);
    QNetworkReply* reply=  m_networkAccessManager.get(networkRequest);
    QSharedPointer< bool > isCalled( new bool( false ) );

    QEventLoop loop;
    QObject::connect( reply, &QNetworkReply::finished, [isCalled,&loop ]()
    {
        if ( *isCalled ) { return; }
        *isCalled = true;
         loop.exit(0);
    } );

#ifndef QT_NO_SSL
  if ( reply->url().toString().toLower().startsWith( "https" ) )
  {
      QObject::connect( reply, static_cast< void( QNetworkReply::* )( const QList< QSslError > & ) >( &QNetworkReply::sslErrors ), [ reply ](const QList< QSslError > & /*errors*/)
      {
          reply->ignoreSslErrors();
      } );
  }
#endif

  QObject::connect( reply, static_cast< void( QNetworkReply::* )( QNetworkReply::NetworkError ) >( &QNetworkReply::error ), [ isCalled,&loop ](const QNetworkReply::NetworkError &code)
  {
      if ( *isCalled ) { return; }
      *isCalled = true;
      loop.exit(-1);

  } );

  loop.exec();

  if(reply->error()==QNetworkReply::NoError){


    content=reply->readAll();
  }else{
      //获取响应信息
       content=reply->readAll();
  }
    qInfo()<<"GET,"<<strUrl<<","<<reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}
