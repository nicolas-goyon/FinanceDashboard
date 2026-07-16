/****************************************************************************
** Meta object code from reading C++ file 'ImportController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/ImportController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ImportController.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
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
struct qt_meta_tag_ZN16ImportControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto ImportController::qt_create_metaobjectdata<qt_meta_tag_ZN16ImportControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ImportController",
        "QML.Element",
        "auto",
        "resultChanged",
        "",
        "importFile",
        "path",
        "localPath",
        "QUrl",
        "url",
        "statusText",
        "statusState",
        "hasResult",
        "message",
        "summaryRows",
        "QVariantList",
        "points",
        "totalPoints",
        "capNote"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'resultChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'importFile'
        QtMocHelpers::MethodData<void(const QString &)>(5, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Method 'localPath'
        QtMocHelpers::MethodData<QString(const QUrl &) const>(7, 4, QMC::AccessPublic, QMetaType::QString, {{
            { 0x80000000 | 8, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'statusText'
        QtMocHelpers::PropertyData<QString>(10, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'statusState'
        QtMocHelpers::PropertyData<QString>(11, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'hasResult'
        QtMocHelpers::PropertyData<bool>(12, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'message'
        QtMocHelpers::PropertyData<QString>(13, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'summaryRows'
        QtMocHelpers::PropertyData<QVariantList>(14, 0x80000000 | 15, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'points'
        QtMocHelpers::PropertyData<QVariantList>(16, 0x80000000 | 15, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'totalPoints'
        QtMocHelpers::PropertyData<int>(17, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'capNote'
        QtMocHelpers::PropertyData<QString>(18, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<ImportController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject ImportController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16ImportControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16ImportControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16ImportControllerE_t>.metaTypes,
    nullptr
} };

void ImportController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ImportController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->resultChanged(); break;
        case 1: _t->importFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: { QString _r = _t->localPath((*reinterpret_cast<std::add_pointer_t<QUrl>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ImportController::*)()>(_a, &ImportController::resultChanged, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QString*>(_v) = _t->statusText(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->statusState(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->hasResult(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->message(); break;
        case 4: *reinterpret_cast<QVariantList*>(_v) = _t->summaryRows(); break;
        case 5: *reinterpret_cast<QVariantList*>(_v) = _t->points(); break;
        case 6: *reinterpret_cast<int*>(_v) = _t->totalPoints(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->capNote(); break;
        default: break;
        }
    }
}

const QMetaObject *ImportController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ImportController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16ImportControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ImportController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void ImportController::resultChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
