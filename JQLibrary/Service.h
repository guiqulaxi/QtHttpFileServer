#ifndef SERVICE_H
#define SERVICE_H

#include <QObject>
#include <QPointer>
#include <QDateTime>
#include <QMap>
#include <functional>
class Session;
class TcpServerManage;
#ifndef QT_NO_SSL
class SslServerManage;
#endif


enum ServiceConfigEnum
{
    ServiceUnknownConfig,
    ServiceHttpListenPort,
    ServiceHttpsListenPort,
    ServiceProcessor, // QPointer< QObject > or QList< QPointer< QObject > >
    ServiceUuid,
    ServiceSslCrtFilePath,
    ServiceSslKeyFilePath,
    ServiceSslCAFilePath,
    ServiceSslPeerVerifyMode,
};
class Service: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY( Service )

private:
    enum ReceiveDataType
    {
        UnknownReceiveDataType,
        NoReceiveDataType,
        VariantListReceiveDataType,
        VariantMapReceiveDataType,
        ListVariantMapReceiveDataType,
    };

    struct ApiConfig
    {
        QPointer< QObject > process;
        QString             apiMethod;
        QString             apiName;
        QString             slotName;
        ReceiveDataType     receiveDataType = UnknownReceiveDataType;
    };

    class Recoder
    {
    public:
        Recoder( const QPointer< Session > &session );

        ~Recoder();

        QPointer< Session > session_;
        QDateTime                         acceptedTime_;
        QString                           serviceUuid_;
        QString                           apiName;
    };

protected:
    Service() = default;

public:
    ~Service() = default;


    static QSharedPointer< Service > createService( const QMap< ServiceConfigEnum, QVariant > &config );


    void registerProcessor( const QPointer< QObject > &processor );


    virtual QJsonDocument extractPostJsonData( const QPointer< Session > &session );

    static void reply(
        const QPointer< Session > &session,
        const QJsonObject &data,
        const bool &isSucceed = true,
        const QString &message = { },
        const int &httpStatusCode = 200 );

    static void reply(
        const QPointer< Session > &session,
        const bool &isSucceed = true,
        const QString &message = { },
        const int &httpStatusCode = 200 );


    virtual void httpGetPing( const QPointer< Session > &session );

    virtual void httpGetFaviconIco( const QPointer< Session > &session );

    virtual void httpOptions( const QPointer< Session > &session );

protected:
    bool initialize( const QMap< ServiceConfigEnum, QVariant > &config );

private:
    void onSessionAccepted( const QPointer< Session > &session );


    static QString snakeCaseToCamelCase(const QString &source, const bool &firstCharUpper = false);

    static QList< QVariantMap > variantListToListVariantMap(const QVariantList &source);

private:
    QSharedPointer< TcpServerManage > httpServerManage_;
#ifndef QT_NO_SSL
    QSharedPointer< SslServerManage > httpsServerManage_;
#endif

    QString                                     serviceUuid_;
    QMap< QString, QMap< QString, ApiConfig > > schedules_;    // apiMethod -> apiName -> API
    QMap< QString, std::function< void( const QPointer< Session > &session ) > > schedules2_; // apiPathPrefix -> callback
    QPointer< QObject > certificateVerifier_;
};

#endif // SERVICE_H
