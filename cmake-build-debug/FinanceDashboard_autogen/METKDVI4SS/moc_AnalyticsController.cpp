/****************************************************************************
** Meta object code from reading C++ file 'AnalyticsController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/AnalyticsController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AnalyticsController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19AnalyticsControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto AnalyticsController::qt_create_metaobjectdata<qt_meta_tag_ZN19AnalyticsControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AnalyticsController",
        "QML.Element",
        "auto",
        "dataChanged",
        "",
        "reload",
        "timestamps",
        "QVariantList",
        "etfsBound",
        "metalsBound",
        "coinsBound",
        "stocksBound",
        "hasData",
        "maxTotal",
        "minTimeMs",
        "maxTimeMs"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'dataChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'reload'
        QtMocHelpers::MethodData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'timestamps'
        QtMocHelpers::MethodData<QVariantList() const>(6, 4, QMC::AccessPublic, 0x80000000 | 7),
        // Method 'etfsBound'
        QtMocHelpers::MethodData<QVariantList() const>(8, 4, QMC::AccessPublic, 0x80000000 | 7),
        // Method 'metalsBound'
        QtMocHelpers::MethodData<QVariantList() const>(9, 4, QMC::AccessPublic, 0x80000000 | 7),
        // Method 'coinsBound'
        QtMocHelpers::MethodData<QVariantList() const>(10, 4, QMC::AccessPublic, 0x80000000 | 7),
        // Method 'stocksBound'
        QtMocHelpers::MethodData<QVariantList() const>(11, 4, QMC::AccessPublic, 0x80000000 | 7),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'hasData'
        QtMocHelpers::PropertyData<bool>(12, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'maxTotal'
        QtMocHelpers::PropertyData<double>(13, QMetaType::Double, QMC::DefaultPropertyFlags, 0),
        // property 'minTimeMs'
        QtMocHelpers::PropertyData<double>(14, QMetaType::Double, QMC::DefaultPropertyFlags, 0),
        // property 'maxTimeMs'
        QtMocHelpers::PropertyData<double>(15, QMetaType::Double, QMC::DefaultPropertyFlags, 0),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<AnalyticsController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject AnalyticsController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19AnalyticsControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19AnalyticsControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19AnalyticsControllerE_t>.metaTypes,
    nullptr
} };

void AnalyticsController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AnalyticsController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->dataChanged(); break;
        case 1: _t->reload(); break;
        case 2: { QVariantList _r = _t->timestamps();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 3: { QVariantList _r = _t->etfsBound();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 4: { QVariantList _r = _t->metalsBound();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 5: { QVariantList _r = _t->coinsBound();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 6: { QVariantList _r = _t->stocksBound();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AnalyticsController::*)()>(_a, &AnalyticsController::dataChanged, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<bool*>(_v) = _t->hasData(); break;
        case 1: *reinterpret_cast<double*>(_v) = _t->maxTotal(); break;
        case 2: *reinterpret_cast<double*>(_v) = _t->minTimeMs(); break;
        case 3: *reinterpret_cast<double*>(_v) = _t->maxTimeMs(); break;
        default: break;
        }
    }
}

const QMetaObject *AnalyticsController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AnalyticsController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19AnalyticsControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AnalyticsController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void AnalyticsController::dataChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
