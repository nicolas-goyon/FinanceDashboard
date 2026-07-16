/****************************************************************************
** Meta object code from reading C++ file 'DataEntryController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/dataentry/DataEntryController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DataEntryController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19DataEntryControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto DataEntryController::qt_create_metaobjectdata<qt_meta_tag_ZN19DataEntryControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DataEntryController",
        "QML.Element",
        "auto",
        "accountsChanged",
        "",
        "statusChanged",
        "addAccount",
        "name",
        "type",
        "removeAccount",
        "index",
        "saveSnapshot",
        "addTransaction",
        "accounts",
        "QVariantList",
        "accountTypes",
        "historyRows",
        "annualRows",
        "statusText"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'accountsChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'addAccount'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(6, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 8 },
        }}),
        // Method 'removeAccount'
        QtMocHelpers::MethodData<void(int)>(9, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Method 'saveSnapshot'
        QtMocHelpers::MethodData<void()>(11, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'addTransaction'
        QtMocHelpers::MethodData<void()>(12, 4, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'accounts'
        QtMocHelpers::PropertyData<QVariantList>(13, 0x80000000 | 14, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'accountTypes'
        QtMocHelpers::PropertyData<QStringList>(15, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'historyRows'
        QtMocHelpers::PropertyData<QVariantList>(16, 0x80000000 | 14, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'annualRows'
        QtMocHelpers::PropertyData<QVariantList>(17, 0x80000000 | 14, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'statusText'
        QtMocHelpers::PropertyData<QString>(18, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<DataEntryController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject DataEntryController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19DataEntryControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19DataEntryControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19DataEntryControllerE_t>.metaTypes,
    nullptr
} };

void DataEntryController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DataEntryController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->accountsChanged(); break;
        case 1: _t->statusChanged(); break;
        case 2: _t->addAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->removeAccount((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->saveSnapshot(); break;
        case 5: _t->addTransaction(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DataEntryController::*)()>(_a, &DataEntryController::accountsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DataEntryController::*)()>(_a, &DataEntryController::statusChanged, 1))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->accounts(); break;
        case 1: *reinterpret_cast<QStringList*>(_v) = _t->accountTypes(); break;
        case 2: *reinterpret_cast<QVariantList*>(_v) = _t->historyRows(); break;
        case 3: *reinterpret_cast<QVariantList*>(_v) = _t->annualRows(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->statusText(); break;
        default: break;
        }
    }
}

const QMetaObject *DataEntryController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DataEntryController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19DataEntryControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DataEntryController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void DataEntryController::accountsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DataEntryController::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
