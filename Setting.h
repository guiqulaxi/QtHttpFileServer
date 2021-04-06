#ifndef SETTING_H
#define SETTING_H
#include <QString>
#define qSetting Setting::instance()
class Setting
{
public:
    static Setting* instance(){
        static Setting gSetting;
        return &gSetting;
    }
    void setIp(const QString& ip);
    QString ip()const;

    void setPort(qint16 ip);
    qint16 port()const;

    void setRootDirectory(const QString& rootDirectory);
    QString rootDirectory()const;
private:
    Setting();

    QString m_strIp;
    qint16  m_iPort;
    QString m_strRootDirectory;
};

#endif // SETTING_H
