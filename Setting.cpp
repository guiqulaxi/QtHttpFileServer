#include "Setting.h"

Setting::Setting():m_strIp("127.0.0.1"),m_iPort(2345),m_strRootDirectory(".")
{

}
void Setting::setIp(const QString& ip)
{
    m_strIp=ip;
}
QString Setting::ip()const
{
    return m_strIp;
}
void Setting::setPort(qint16 port)
{
    m_iPort=port;
}
qint16 Setting::port()const
{
    return m_iPort;
}
void Setting::setRootDirectory(const QString& rootDirectory)
{
    m_strRootDirectory=rootDirectory;
}
QString Setting::rootDirectory()const
{
    return  m_strRootDirectory;
}
