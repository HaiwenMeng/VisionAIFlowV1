#include "ytroishowdisp.h"
#include "ytroishowdispplugin.h"

#include <QtPlugin>

YtRoiShowDispPlugin::YtRoiShowDispPlugin(QObject *parent)
    : QObject(parent)
{
    m_initialized = false;
}

void YtRoiShowDispPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
    if (m_initialized)
        return;

    // Add extension registrations, etc. here

    m_initialized = true;
}

bool YtRoiShowDispPlugin::isInitialized() const
{
    return m_initialized;
}

QWidget *YtRoiShowDispPlugin::createWidget(QWidget *parent)
{
    return new YtRoiShowDisp(parent);
}

QString YtRoiShowDispPlugin::name() const
{
    return QLatin1String("YtRoiShowDisp");
}

QString YtRoiShowDispPlugin::group() const
{
    return QLatin1String("YTPlgin");
}

QIcon YtRoiShowDispPlugin::icon() const
{
    return QIcon(":/YtRoiShowDisp.png");
}

QString YtRoiShowDispPlugin::toolTip() const
{
    return QLatin1String("");
}

QString YtRoiShowDispPlugin::whatsThis() const
{
    return QLatin1String("");
}

bool YtRoiShowDispPlugin::isContainer() const
{
    return false;
}

QString YtRoiShowDispPlugin::domXml() const
{
    return QLatin1String("<widget class=\"YtRoiShowDisp\" name=\"ytRoiShowDisp\">\n</widget>\n");
}

QString YtRoiShowDispPlugin::includeFile() const
{
    return QLatin1String("ytroishowdisp.h");
}
#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(ytroishowdispplugin, YtRoiShowDispPlugin)
#endif // QT_VERSION < 0x050000
