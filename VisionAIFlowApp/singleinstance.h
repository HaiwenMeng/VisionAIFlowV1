#ifndef CSINGLEINSTANCE_H
#define CSINGLEINSTANCE_H

#include <QObject>
#include <QSharedMemory>
#include "ytframelesswidgetlib.h"

class CSingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit CSingleInstance(QObject *parent = 0) : QObject(parent) {}
    ~CSingleInstance() {}

    static bool isSingleInstance(QSharedMemory &sharedMemory, const QString &strSignleKey)
    {
        // 使用共享内存的方式来保证只运行一个实例
        sharedMemory.setKey(strSignleKey);
        if (sharedMemory.attach())
        {
            YtMessageBox::YtShowError(u8"警告",
                                     strSignleKey+u8"程序已运行");
            return false;
        }

        if (!sharedMemory.create(1))
        {
            YtMessageBox::YtShowError( u8"警告",
                                     strSignleKey+u8"程序已运行");
            return false;
        }

        return true;
    }
};

#endif // CSINGLEINSTANCE_H
