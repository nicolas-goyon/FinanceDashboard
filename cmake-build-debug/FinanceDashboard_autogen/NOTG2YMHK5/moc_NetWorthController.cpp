/****************************************************************************
** Meta object code from reading C++ file 'NetWorthController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/overview/NetWorthController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'NetWorthController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18NetWorthControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto NetWorthController::qt_create_metaobjectdata<qt_meta_tag_ZN18NetWorthControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "NetWorthController",
        "QML.Element",
        "auto",
        "timestamps",
        "QVariantList",
        "",
        "checkingBound",
        "savingsBound",
        "tradingBound",
        "kpis",
        "allocation",
        "monthLabels",
        "netGains",
        "netLosses",
        "netChangeMax",
        "netChangeMin",
        "maxTotal",
        "minTimeMs",
        "maxTimeMs"
    };

    QtMocHelpers::UintData qt_methods {
        // Method 'timestamps'
        QtMocHelpers::MethodData<QVariantList() const>(3, 5, QMC::AccessPublic, 0x80000000 | 4),
        // Method 'checkingBound'
        QtMocHelpers::MethodData<QVariantList() const>(6, 5, QMC::AccessPublic, 0x80000000 | 4),
        // Method 'savingsBound'
        QtMocHelpers::MethodData<QVariantList() const>(7, 5, QMC::AccessPublic, 0x80000000 | 4),
        // Method 'tradingBound'
        QtMocHelpers::MethodData<QVariantList() const>(8, 5, QMC::AccessPublic, 0x80000000 | 4),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'kpis'
        QtMocHelpers::PropertyData<QVariantList>(9, 0x80000000 | 4, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'allocation'
        QtMocHelpers::PropertyData<QVariantList>(10, 0x80000000 | 4, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'monthLabels'
        QtMocHelpers::PropertyData<QStringList>(11, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'netGains'
        QtMocHelpers::PropertyData<QVariantList>(12, 0x80000000 | 4, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'netLosses'
        QtMocHelpers::PropertyData<QVariantList>(13, 0x80000000 | 4, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'netChangeMax'
        QtMocHelpers::PropertyData<double>(14, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'netChangeMin'
        QtMocHelpers::PropertyData<double>(15, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'maxTotal'
        QtMocHelpers::PropertyData<double>(16, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'minTimeMs'
        QtMocHelpers::PropertyData<double>(17, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'maxTimeMs'
        QtMocHelpers::PropertyData<double>(18, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<NetWorthController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject NetWorthController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NetWorthControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NetWorthControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18NetWorthControllerE_t>.metaTypes,
    nullptr
} };

void NetWorthController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<NetWorthController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: { QVariantList _r = _t->timestamps();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 1: { QVariantList _r = _t->checkingBound();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 2: { QVariantList _r = _t->savingsBound();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 3: { QVariantList _r = _t->tradingBound();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->kpis(); break;
        case 1: *reinterpret_cast<QVariantList*>(_v) = _t->allocation(); break;
        case 2: *reinterpret_cast<QStringList*>(_v) = _t->monthLabels(); break;
        case 3: *reinterpret_cast<QVariantList*>(_v) = _t->netGains(); break;
        case 4: *reinterpret_cast<QVariantList*>(_v) = _t->netLosses(); break;
        case 5: *reinterpret_cast<double*>(_v) = _t->netChangeMax(); break;
        case 6: *reinterpret_cast<double*>(_v) = _t->netChangeMin(); break;
        case 7: *reinterpret_cast<double*>(_v) = _t->maxTotal(); break;
        case 8: *reinterpret_cast<double*>(_v) = _t->minTimeMs(); break;
        case 9: *reinterpret_cast<double*>(_v) = _t->maxTimeMs(); break;
        default: break;
        }
    }
}

const QMetaObject *NetWorthController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *NetWorthController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18NetWorthControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int NetWorthController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}
QT_WARNING_POP
