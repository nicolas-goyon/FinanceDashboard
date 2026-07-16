/****************************************************************************
** Meta object code from reading C++ file 'DatabaseController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/DatabaseController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DatabaseController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18DatabaseControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto DatabaseController::qt_create_metaobjectdata<qt_meta_tag_ZN18DatabaseControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DatabaseController",
        "QML.Element",
        "auto",
        "currentTableChanged",
        "",
        "rowCountChanged",
        "statusChanged",
        "changesSaved",
        "columnWidth",
        "column",
        "addRow",
        "deleteRows",
        "QVariantList",
        "rows",
        "save",
        "revert",
        "refresh",
        "model",
        "QAbstractItemModel*",
        "currentTable",
        "rowCount",
        "statusText",
        "statusIsError"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'currentTableChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'rowCountChanged'
        QtMocHelpers::SignalData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void()>(6, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'changesSaved'
        QtMocHelpers::SignalData<void()>(7, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'columnWidth'
        QtMocHelpers::MethodData<int(int) const>(8, 4, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 9 },
        }}),
        // Method 'addRow'
        QtMocHelpers::MethodData<int()>(10, 4, QMC::AccessPublic, QMetaType::Int),
        // Method 'deleteRows'
        QtMocHelpers::MethodData<void(const QVariantList &)>(11, 4, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Method 'save'
        QtMocHelpers::MethodData<void()>(14, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'revert'
        QtMocHelpers::MethodData<void()>(15, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'refresh'
        QtMocHelpers::MethodData<void()>(16, 4, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'model'
        QtMocHelpers::PropertyData<QAbstractItemModel*>(17, 0x80000000 | 18, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'currentTable'
        QtMocHelpers::PropertyData<int>(19, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
        // property 'rowCount'
        QtMocHelpers::PropertyData<int>(20, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'statusText'
        QtMocHelpers::PropertyData<QString>(21, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
        // property 'statusIsError'
        QtMocHelpers::PropertyData<bool>(22, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<DatabaseController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject DatabaseController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DatabaseControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DatabaseControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18DatabaseControllerE_t>.metaTypes,
    nullptr
} };

void DatabaseController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DatabaseController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentTableChanged(); break;
        case 1: _t->rowCountChanged(); break;
        case 2: _t->statusChanged(); break;
        case 3: _t->changesSaved(); break;
        case 4: { int _r = _t->columnWidth((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 5: { int _r = _t->addRow();
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 6: _t->deleteRows((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 7: _t->save(); break;
        case 8: _t->revert(); break;
        case 9: _t->refresh(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DatabaseController::*)()>(_a, &DatabaseController::currentTableChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DatabaseController::*)()>(_a, &DatabaseController::rowCountChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DatabaseController::*)()>(_a, &DatabaseController::statusChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DatabaseController::*)()>(_a, &DatabaseController::changesSaved, 3))
            return;
    }
    if (_c == QMetaObject::RegisterPropertyMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAbstractItemModel* >(); break;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QAbstractItemModel**>(_v) = _t->model(); break;
        case 1: *reinterpret_cast<int*>(_v) = _t->currentTable(); break;
        case 2: *reinterpret_cast<int*>(_v) = _t->rowCount(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->statusText(); break;
        case 4: *reinterpret_cast<bool*>(_v) = _t->statusIsError(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 1: _t->setCurrentTable(*reinterpret_cast<int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *DatabaseController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DatabaseController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DatabaseControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DatabaseController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
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
void DatabaseController::currentTableChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DatabaseController::rowCountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void DatabaseController::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void DatabaseController::changesSaved()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
