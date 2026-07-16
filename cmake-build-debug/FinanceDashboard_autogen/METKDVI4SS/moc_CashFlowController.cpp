/****************************************************************************
** Meta object code from reading C++ file 'CashFlowController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/CashFlowController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CashFlowController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18CashFlowControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto CashFlowController::qt_create_metaobjectdata<qt_meta_tag_ZN18CashFlowControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CashFlowController",
        "QML.Element",
        "auto",
        "monthChanged",
        "",
        "rollingTimestamps",
        "QVariantList",
        "rollingValues",
        "categoryValues",
        "index",
        "monthLabels",
        "incomeValues",
        "expenseValues",
        "barsMax",
        "categories",
        "selectedMonth",
        "monthSlices",
        "biggestRows",
        "flowInputs",
        "flowOutputs",
        "minTimeMs",
        "maxTimeMs"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'monthChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'rollingTimestamps'
        QtMocHelpers::MethodData<QVariantList() const>(5, 4, QMC::AccessPublic, 0x80000000 | 6),
        // Method 'rollingValues'
        QtMocHelpers::MethodData<QVariantList() const>(7, 4, QMC::AccessPublic, 0x80000000 | 6),
        // Method 'categoryValues'
        QtMocHelpers::MethodData<QVariantList(int) const>(8, 4, QMC::AccessPublic, 0x80000000 | 6, {{
            { QMetaType::Int, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'monthLabels'
        QtMocHelpers::PropertyData<QStringList>(10, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'incomeValues'
        QtMocHelpers::PropertyData<QVariantList>(11, 0x80000000 | 6, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'expenseValues'
        QtMocHelpers::PropertyData<QVariantList>(12, 0x80000000 | 6, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'barsMax'
        QtMocHelpers::PropertyData<double>(13, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'categories'
        QtMocHelpers::PropertyData<QVariantList>(14, 0x80000000 | 6, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'selectedMonth'
        QtMocHelpers::PropertyData<int>(15, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
        // property 'monthSlices'
        QtMocHelpers::PropertyData<QVariantList>(16, 0x80000000 | 6, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'biggestRows'
        QtMocHelpers::PropertyData<QVariantList>(17, 0x80000000 | 6, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'flowInputs'
        QtMocHelpers::PropertyData<QVariantList>(18, 0x80000000 | 6, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'flowOutputs'
        QtMocHelpers::PropertyData<QVariantList>(19, 0x80000000 | 6, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'minTimeMs'
        QtMocHelpers::PropertyData<double>(20, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'maxTimeMs'
        QtMocHelpers::PropertyData<double>(21, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Constant),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<CashFlowController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject CashFlowController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CashFlowControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CashFlowControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18CashFlowControllerE_t>.metaTypes,
    nullptr
} };

void CashFlowController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CashFlowController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->monthChanged(); break;
        case 1: { QVariantList _r = _t->rollingTimestamps();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 2: { QVariantList _r = _t->rollingValues();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 3: { QVariantList _r = _t->categoryValues((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CashFlowController::*)()>(_a, &CashFlowController::monthChanged, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QStringList*>(_v) = _t->monthLabels(); break;
        case 1: *reinterpret_cast<QVariantList*>(_v) = _t->incomeValues(); break;
        case 2: *reinterpret_cast<QVariantList*>(_v) = _t->expenseValues(); break;
        case 3: *reinterpret_cast<double*>(_v) = _t->barsMax(); break;
        case 4: *reinterpret_cast<QVariantList*>(_v) = _t->categories(); break;
        case 5: *reinterpret_cast<int*>(_v) = _t->selectedMonth(); break;
        case 6: *reinterpret_cast<QVariantList*>(_v) = _t->monthSlices(); break;
        case 7: *reinterpret_cast<QVariantList*>(_v) = _t->biggestRows(); break;
        case 8: *reinterpret_cast<QVariantList*>(_v) = _t->flowInputs(); break;
        case 9: *reinterpret_cast<QVariantList*>(_v) = _t->flowOutputs(); break;
        case 10: *reinterpret_cast<double*>(_v) = _t->minTimeMs(); break;
        case 11: *reinterpret_cast<double*>(_v) = _t->maxTimeMs(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 5: _t->setSelectedMonth(*reinterpret_cast<int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *CashFlowController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CashFlowController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CashFlowControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CashFlowController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void CashFlowController::monthChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
