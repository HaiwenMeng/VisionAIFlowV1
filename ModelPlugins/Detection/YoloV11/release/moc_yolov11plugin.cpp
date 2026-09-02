/****************************************************************************
** Meta object code from reading C++ file 'yolov11plugin.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.7.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../yolov11plugin.h"
#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'yolov11plugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.7.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSvisionaiflowSCOPEyolov11SCOPEYolo11PluginENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSvisionaiflowSCOPEyolov11SCOPEYolo11PluginENDCLASS = QtMocHelpers::stringData(
    "visionaiflow::yolov11::Yolo11Plugin"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSvisionaiflowSCOPEyolov11SCOPEYolo11PluginENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

Q_CONSTINIT const QMetaObject visionaiflow::yolov11::Yolo11Plugin::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSvisionaiflowSCOPEyolov11SCOPEYolo11PluginENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSvisionaiflowSCOPEyolov11SCOPEYolo11PluginENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSvisionaiflowSCOPEyolov11SCOPEYolo11PluginENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Yolo11Plugin, std::true_type>
    >,
    nullptr
} };

void visionaiflow::yolov11::Yolo11Plugin::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    (void)_o;
    (void)_id;
    (void)_c;
    (void)_a;
}

const QMetaObject *visionaiflow::yolov11::Yolo11Plugin::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *visionaiflow::yolov11::Yolo11Plugin::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSvisionaiflowSCOPEyolov11SCOPEYolo11PluginENDCLASS.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "plugin_api::IDetectionPlugin"))
        return static_cast< plugin_api::IDetectionPlugin*>(this);
    if (!strcmp(_clname, "visionaiflow.plugin_api.IDetectionPlugin/2.0"))
        return static_cast< visionaiflow::plugin_api::IDetectionPlugin*>(this);
    return QObject::qt_metacast(_clname);
}

int visionaiflow::yolov11::Yolo11Plugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    return _id;
}
using namespace visionaiflow;
using namespace visionaiflow::yolov11;

#ifdef QT_MOC_EXPORT_PLUGIN_V2
static constexpr unsigned char qt_pluginMetaDataV2_Yolo11Plugin[] = {
    0xbf, 
    // "IID"
    0x02,  0x78,  0x2c,  'v',  'i',  's',  'i',  'o', 
    'n',  'a',  'i',  'f',  'l',  'o',  'w',  '.', 
    'p',  'l',  'u',  'g',  'i',  'n',  '_',  'a', 
    'p',  'i',  '.',  'I',  'D',  'e',  't',  'e', 
    'c',  't',  'i',  'o',  'n',  'P',  'l',  'u', 
    'g',  'i',  'n',  '/',  '2',  '.',  '0', 
    // "className"
    0x03,  0x6c,  'Y',  'o',  'l',  'o',  '1',  '1', 
    'P',  'l',  'u',  'g',  'i',  'n', 
    // "MetaData"
    0x04,  0xaa,  0x6c,  'd',  'i',  's',  'p',  'l', 
    'a',  'y',  '_',  'n',  'a',  'm',  'e',  0x66, 
    'Y',  'O',  'L',  'O',  '1',  '1',  0x69,  'p', 
    'l',  'u',  'g',  'i',  'n',  '_',  'i',  'd', 
    0x78,  0x1e,  'v',  'i',  's',  'i',  'o',  'n', 
    'a',  'i',  'f',  'l',  'o',  'w',  '.',  'd', 
    'e',  't',  'e',  'c',  't',  'i',  'o',  'n', 
    '.',  'y',  'o',  'l',  'o',  'v',  '1',  '1', 
    0x78,  0x18,  's',  'u',  'p',  'p',  'o',  'r', 
    't',  's',  '_',  'b',  'a',  'c',  'k',  'b', 
    'o',  'n',  'e',  '_',  'e',  'x',  'p',  'o', 
    'r',  't',  0xf4,  0x6f,  's',  'u',  'p',  'p', 
    'o',  'r',  't',  's',  '_',  'e',  'x',  'p', 
    'o',  'r',  't',  0xf4,  0x6d,  's',  'u',  'p', 
    'p',  'o',  'r',  't',  's',  '_',  'f',  'p', 
    '1',  '6',  0xf4,  0x72,  's',  'u',  'p',  'p', 
    'o',  'r',  't',  's',  '_',  'm',  'u',  'l', 
    't',  'i',  '_',  'g',  'p',  'u',  0xf4,  0x73, 
    's',  'u',  'p',  'p',  'o',  'r',  't',  's', 
    '_',  'p',  'r',  'e',  't',  'r',  'a',  'i', 
    'n',  'e',  'd',  0xf5,  0x6f,  's',  'u',  'p', 
    'p',  'o',  'r',  't',  's',  '_',  'r',  'e', 
    's',  'u',  'm',  'e',  0xf4,  0x69,  't',  'a', 
    's',  'k',  '_',  't',  'y',  'p',  'e',  0x69, 
    'd',  'e',  't',  'e',  'c',  't',  'i',  'o', 
    'n',  0x67,  'v',  'e',  'r',  's',  'i',  'o', 
    'n',  0x65,  '1',  '.',  '0',  '.',  '0', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN_V2(visionaiflow::yolov11::Yolo11Plugin, Yolo11Plugin, qt_pluginMetaDataV2_Yolo11Plugin)
#else
QT_PLUGIN_METADATA_SECTION
Q_CONSTINIT static constexpr unsigned char qt_pluginMetaData_Yolo11Plugin[] = {
    'Q', 'T', 'M', 'E', 'T', 'A', 'D', 'A', 'T', 'A', ' ', '!',
    // metadata version, Qt version, architectural requirements
    0, QT_VERSION_MAJOR, QT_VERSION_MINOR, qPluginArchRequirements(),
    0xbf, 
    // "IID"
    0x02,  0x78,  0x2c,  'v',  'i',  's',  'i',  'o', 
    'n',  'a',  'i',  'f',  'l',  'o',  'w',  '.', 
    'p',  'l',  'u',  'g',  'i',  'n',  '_',  'a', 
    'p',  'i',  '.',  'I',  'D',  'e',  't',  'e', 
    'c',  't',  'i',  'o',  'n',  'P',  'l',  'u', 
    'g',  'i',  'n',  '/',  '2',  '.',  '0', 
    // "className"
    0x03,  0x6c,  'Y',  'o',  'l',  'o',  '1',  '1', 
    'P',  'l',  'u',  'g',  'i',  'n', 
    // "MetaData"
    0x04,  0xaa,  0x6c,  'd',  'i',  's',  'p',  'l', 
    'a',  'y',  '_',  'n',  'a',  'm',  'e',  0x66, 
    'Y',  'O',  'L',  'O',  '1',  '1',  0x69,  'p', 
    'l',  'u',  'g',  'i',  'n',  '_',  'i',  'd', 
    0x78,  0x1e,  'v',  'i',  's',  'i',  'o',  'n', 
    'a',  'i',  'f',  'l',  'o',  'w',  '.',  'd', 
    'e',  't',  'e',  'c',  't',  'i',  'o',  'n', 
    '.',  'y',  'o',  'l',  'o',  'v',  '1',  '1', 
    0x78,  0x18,  's',  'u',  'p',  'p',  'o',  'r', 
    't',  's',  '_',  'b',  'a',  'c',  'k',  'b', 
    'o',  'n',  'e',  '_',  'e',  'x',  'p',  'o', 
    'r',  't',  0xf4,  0x6f,  's',  'u',  'p',  'p', 
    'o',  'r',  't',  's',  '_',  'e',  'x',  'p', 
    'o',  'r',  't',  0xf4,  0x6d,  's',  'u',  'p', 
    'p',  'o',  'r',  't',  's',  '_',  'f',  'p', 
    '1',  '6',  0xf4,  0x72,  's',  'u',  'p',  'p', 
    'o',  'r',  't',  's',  '_',  'm',  'u',  'l', 
    't',  'i',  '_',  'g',  'p',  'u',  0xf4,  0x73, 
    's',  'u',  'p',  'p',  'o',  'r',  't',  's', 
    '_',  'p',  'r',  'e',  't',  'r',  'a',  'i', 
    'n',  'e',  'd',  0xf5,  0x6f,  's',  'u',  'p', 
    'p',  'o',  'r',  't',  's',  '_',  'r',  'e', 
    's',  'u',  'm',  'e',  0xf4,  0x69,  't',  'a', 
    's',  'k',  '_',  't',  'y',  'p',  'e',  0x69, 
    'd',  'e',  't',  'e',  'c',  't',  'i',  'o', 
    'n',  0x67,  'v',  'e',  'r',  's',  'i',  'o', 
    'n',  0x65,  '1',  '.',  '0',  '.',  '0', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN(visionaiflow::yolov11::Yolo11Plugin, Yolo11Plugin)
#endif  // QT_MOC_EXPORT_PLUGIN_V2

QT_WARNING_POP
