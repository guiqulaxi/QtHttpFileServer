#ifndef QHTTPFILECLIENT_H
#define QHTTPFILECLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QIODevice>
class QHttpFileClient : public QObject
{
    Q_OBJECT
public:
    explicit QHttpFileClient(QObject *parent = nullptr);
    int downloadFile(const QString& filePath,const QString& localPath);
    int uploadFile(const QString& filePath,const QString& localPath);
    int deleteFile(const QString& filePath);
    int  informFile(const QString& filePath,QString& content);
    int listDir(const QString& dirPath,QString& content);
private:
    QString m_strRootURL;
    QNetworkAccessManager m_networkAccessManager;
    QPointer< QIODevice >                                ioDevice_;
signals:

};

#endif // QHTTPFILECLIENT_H
