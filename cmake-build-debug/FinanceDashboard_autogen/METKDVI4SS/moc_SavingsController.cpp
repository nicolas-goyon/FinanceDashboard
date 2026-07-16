/****************************************************************************
** Meta object code from reading C++ file 'SavingsController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/SavingsController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SavingsController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17SavingsControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto SavingsController::qt_create_metaobjectdata<qt_meta_tag_ZN17SavingsControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SavingsController",
        "QML.Element",
        "auto",
        "timestamps",
        "QVariantList",
        "",
        "accountValues",
        "index",
        "interestValues",
        "kpis",
        "goals",
        "yearLabels",
        "interestMax",
        "balanceMax",
        "minTimeMs",
        "maxTimeMs"
    };

    QtMocHelpers::UintData qt_methods {
        // Method 'timestamps'
        QtMocHelpers::MethodData<QVariantList() const>(3, 5, QMC::AccessPublic, 0x80000000 | 4),
        // Method 'accountValues'
        QtMocHelpers::MethodData<QVariantList(int) const>(6, 5, QMC::AccessPublic, 0x80000000 | 4, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'interestValues'
        QtMocHelpers::MethodData<QVariantList(int) const>(8, 5, QMC::AccessPublic, 0x80000000 | 4, {{
            { QMetaType::Int, 7 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'kpis'
        QtMocHelpers::PropertyData<QVariantList>(9, 0x80000000 | 4, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'goals'
        QtMocHelpers::PropertyData<QVariantList>(10, 0x80000000 | 4, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'yearLabels'
        QtMocHelpers::PropertyData<QStringList>(11, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'interestMax'
        QtMocHelpers::PropertyData<double>(12, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'balanceMax'
        QtMocHelpers::PropertyData<double>(13, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'minTimeMs'
        QtMocHelpers::PropertyData<double>(14, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'maxTimeMs'
        QtMocHelpers::PropertyData<double>(15, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<SavingsController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject SavingsController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17SavingsControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17SavingsControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17SavingsControllerE_t>.metaTypes,
    nullptr
} };

void SavingsController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SavingsController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: { QVariantList _r = _t->timestamps();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 1: { QVariantList _r = _t->accountValues((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 2: { QVariantList _r = _t->interestValues((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->kpis(); break;
        case 1: *reinterpret_cast<QVariantList*>(_v) = _t->goals(); break;
        case 2: *reinterpret_cast<QStringList*>(_v) = _t->yearLabels(); break;
        case 3: *reinterpret_cast<double*>(_v) = _t->interestMax(); break;
        case 4: *reinterpret_cast<double*>(_v) = _t->balanceMax(); break;
        case 5: *reinterpret_cast<double*>(_v) = _t->minTimeMs(); break;
        case 6: *reinterpret_cast<double*>(_v) = _t->maxTimeMs(); break;
        default: break;
        }
    }
}

const QMetaObject *SavingsController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SavingsController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17SavingsControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SavingsController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
