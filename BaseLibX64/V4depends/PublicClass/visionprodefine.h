#ifndef VISIONPRODEFINE_H
#define VISIONPRODEFINE_H

#pragma execution_character_set("utf-8")

#include <QString>
#include <QFile>
#include <QDir>
#include <QApplication>

/// 软件文件路径
#define FOLDER_PATH_DATA_FILE               QApplication::applicationDirPath() + "/DataFile"                        // 软件数据文件夹地址
#define FOLDER_PATH_FUNCWIDGET_MANAGE       QApplication::applicationDirPath() + "/DataFile/FuncWidgetManage"       // 功能窗口管理文件夹地址
#define FOLDER_PATH_FUNCWIDGET_PLUGIN       QApplication::applicationDirPath() + "/WidgetPlugins"                    // 功能窗口插件文件夹地址
#define FOLDER_PATH_THREAD_MANAGE           QApplication::applicationDirPath() + "/DataFile/ThreadManage"           // 线程管理文件夹地址

#define FILE_PATH_MAINWINDOW_LAYOUT         QApplication::applicationDirPath() + "/DataFile/MainWindowLayout.data"  // 界面布局文件
#define FILE_PATH_DEVICE_INFO               QApplication::applicationDirPath() + "/DataFile/DeviceInfo.data"        // 要加载的设备信息文件
#define FILE_PATH_PLUGIN_INFO               QApplication::applicationDirPath() + "/DataFile/PluginInfo.data"        // 要加载的插件信息文件
#define FILE_PATH_SYSTEM_PARAM              QApplication::applicationDirPath() + "/DataFile/SystemParam.data"       // 系统参数文件
#define FILE_PATH_FUNCWIDGET_LAYOUT         QApplication::applicationDirPath() + "/DataFile/FuncWidgetLayout.data"  // 功能窗口布局文件

/// 软件外设相关
#define DEV_TYPE_CAMERA     QString("工业相机")
#define DEV_TYPE_PLC        QString("PLC")

struct SoftDevDefine
{
    QString DevType;//设备类型
    QString DevName;//设备名称
    QString DevIni;//设备配置文件
    QString DevDll;//设备加载dll
    bool DevInitS;//是否异步启动
};

struct SoftDevDefineList
{
    QList<SoftDevDefine> m_DevDefineDo;
    void toLoad()
    {
        m_DevDefineDo.clear();
        QFile file(FILE_PATH_DEVICE_INFO);
        if(!file.open(QIODevice::ReadOnly)) {
            return;
        }
        QDataStream in(&file);
        SoftDevDefine temA;
        int size = 0;
        in >> size;
        for(int i = 0; i < size; i++) {
            in >> temA.DevDll;
            in >> temA.DevIni;
            in >> temA.DevInitS;
            in >> temA.DevName;
            in >> temA.DevType;
            m_DevDefineDo.append(temA);
        }
        file.close();
    }
    void toSave()
    {
        QFile file(FILE_PATH_DEVICE_INFO);
        if(!file.open(QIODevice::WriteOnly)) {
            return;
        }
        QDataStream out(&file);
        out << m_DevDefineDo.size();
        for(int i = 0; i <  m_DevDefineDo.size(); i++)
        {
            out << m_DevDefineDo.at(i).DevDll;
            out << m_DevDefineDo.at(i).DevIni;
            out << m_DevDefineDo.at(i).DevInitS;
            out << m_DevDefineDo.at(i).DevName;
            out << m_DevDefineDo.at(i).DevType;
        }
        file.close();
    }
    void toLoadDBF(QString FileName)
    {
        m_DevDefineDo.clear();
        QFile file(FileName);
        if(!file.open(QIODevice::ReadOnly)) {
            return;
        }
        QDataStream in(&file);
        SoftDevDefine temA;
        int size = 0;
        in >> size;
        for(int i = 0; i < size; i++) {
            in >> temA.DevDll;
            in >> temA.DevIni;
            in >> temA.DevInitS;
            in >> temA.DevName;
            in >> temA.DevType;
            m_DevDefineDo.append(temA);
        }
        file.close();
    }
    void toSaveDBF(QString FileName)
    {
        QFile file(FileName);
        if(!file.open(QIODevice::WriteOnly)) {
            return;
        }
        QDataStream out(&file);
        out << m_DevDefineDo.size();
        for(int i = 0; i <  m_DevDefineDo.size(); i++)
        {
            out << m_DevDefineDo.at(i).DevDll;
            out << m_DevDefineDo.at(i).DevIni;
            out << m_DevDefineDo.at(i).DevInitS;
            out << m_DevDefineDo.at(i).DevName;
            out << m_DevDefineDo.at(i).DevType;
        }
        file.close();
    }
};

struct SoftPluginDefineList
{
    QMap<QString,QStringList>    m_PluginLoad;
    void toLoad()
    {
        m_PluginLoad.clear();
        QFile file(FILE_PATH_PLUGIN_INFO);
        if(!file.open(QIODevice::ReadOnly))
        {
            return;
        }
        QDataStream in(&file);
        int size = 0;
        in >> size;
        QString DllName;
        QStringList GetData;
        for(int i = 0; i < size; i++)
        {
            in >> DllName;
            in >> GetData;
            m_PluginLoad.insert(DllName,GetData);
        }
        file.close();
    }
    void toSave()
    {
        QFile file(FILE_PATH_PLUGIN_INFO);
        if(!file.open(QIODevice::WriteOnly)) {
            return;
        }
        QDataStream out(&file);
        out << m_PluginLoad.keys().size();
        foreach (QString Key, m_PluginLoad.keys())
        {
            out<< Key;
            out<< m_PluginLoad.value(Key);
        }
        file.close();
    }
};

#endif // VISIONPRODEFINE_H
