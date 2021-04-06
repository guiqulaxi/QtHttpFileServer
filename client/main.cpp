#include <QCoreApplication>
#include <JQNet>
#include <QDebug>
#include <QFile>
#include "QHttpFileClient.h"
int main(int argc, char *argv[])
{
    qputenv( "QT_SSL_USE_TEMPORARY_KEYCHAIN", "1" );
    qSetMessagePattern( "%{time hh:mm:ss.zzz}: %{message}" );

    QCoreApplication app( argc, argv );
    QFile file("/home/sun/1.deb");
    QByteArray data;

    QHttpFileClient client;

    while(true)
    {

        //client.downloadFile("张/1.deb","/dev/null");
        QString cont;
        client.informFile("张/1.deb",cont);
        client.listDir("张",cont);
        //client.deleteFile("张/1.deb");
        //client.uploadFile("张/1.deb","/home/sun/1.deb");
    }

    return 0;
}
