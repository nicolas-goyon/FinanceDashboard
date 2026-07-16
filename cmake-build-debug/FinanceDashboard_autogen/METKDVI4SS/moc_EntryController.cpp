/****************************************************************************
** Meta object code from reading C++ file 'EntryController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/EntryController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'EntryController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13CategoryModelE_t {};
} // unnamed namespace

template <> constexpr inline auto CategoryModel::qt_create_metaobjectdata<qt_meta_tag_ZN13CategoryModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CategoryModel",
        "QML.Element",
        "anonymous",
        "totalChanged",
        "",
        "addRow",
        "removeRow",
        "index",
        "setRowName",
        "name",
        "setRowValue",
        "valueText",
        "totalText"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'totalChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'addRow'
        QtMocHelpers::MethodData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'removeRow'
        QtMocHelpers::MethodData<void(int)>(6, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'setRowName'
        QtMocHelpers::MethodData<void(int, const QString &)>(8, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::QString, 9 },
        }}),
        // Method 'setRowValue'
        QtMocHelpers::MethodData<void(int, const QString &)>(10, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::QString, 11 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'totalText'
        QtMocHelpers::PropertyData<QString>(12, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<CategoryModel, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject CategoryModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractListModel::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CategoryModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CategoryModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13CategoryModelE_t>.metaTypes,
    nullptr
} };

void CategoryModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CategoryModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->totalChanged(); break;
        case 1: _t->addRow(); break;
        case 2: _t->removeRow((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->setRowName((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->setRowValue((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CategoryModel::*)()>(_a, &CategoryModel::totalChanged, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QString*>(_v) = _t->totalText(); break;
        default: break;
        }
    }
}

const QMetaObject *CategoryModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CategoryModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CategoryModelE_t>.strings))
        return static_cast<void*>(this);
    return QAbstractListModel::qt_metacast(_clname);
}

int CategoryModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractListModel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void CategoryModel::totalChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
namespace {
struct qt_meta_tag_ZN15EntryControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto EntryController::qt_create_metaobjectdata<qt_meta_tag_ZN15EntryControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "EntryController",
        "QML.Element",
        "auto",
        "dateChanged",
        "",
        "grandTotalChanged",
        "statusChanged",
        "snapshotSaved",
        "setDateYmd",
        "year",
        "month0",
        "day",
        "save",
        "clearAll",
        "stocks",
        "CategoryModel*",
        "coins",
        "metals",
        "etfs",
        "dateText",
        "calYear",
        "calMonth",
        "calDay",
        "grandTotalText",
        "statusText",
        "statusIsError"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'dateChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'grandTotalChanged'
        QtMocHelpers::SignalData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void()>(6, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'snapshotSaved'
        QtMocHelpers::SignalData<void()>(7, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'setDateYmd'
        QtMocHelpers::MethodData<void(int, int, int)>(8, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 9 }, { QMetaType::Int, 10 }, { QMetaType::Int, 11 },
        }}),
        // Method 'save'
        QtMocHelpers::MethodData<void()>(12, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'clearAll'
        QtMocHelpers::MethodData<void()>(13, 4, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'stocks'
        QtMocHelpers::PropertyData<CategoryModel*>(14, 0x80000000 | 15, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'coins'
        QtMocHelpers::PropertyData<CategoryModel*>(16, 0x80000000 | 15, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'metals'
        QtMocHelpers::PropertyData<CategoryModel*>(17, 0x80000000 | 15, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'etfs'
        QtMocHelpers::PropertyData<CategoryModel*>(18, 0x80000000 | 15, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'dateText'
        QtMocHelpers::PropertyData<QString>(19, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'calYear'
        QtMocHelpers::PropertyData<int>(20, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'calMonth'
        QtMocHelpers::PropertyData<int>(21, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'calDay'
        QtMocHelpers::PropertyData<int>(22, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'grandTotalText'
        QtMocHelpers::PropertyData<QString>(23, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'statusText'
        QtMocHelpers::PropertyData<QString>(24, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
        // property 'statusIsError'
        QtMocHelpers::PropertyData<bool>(25, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<EntryController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject EntryController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15EntryControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15EntryControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15EntryControllerE_t>.metaTypes,
    nullptr
} };

void EntryController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<EntryController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->dateChanged(); break;
        case 1: _t->grandTotalChanged(); break;
        case 2: _t->statusChanged(); break;
        case 3: _t->snapshotSaved(); break;
        case 4: _t->setDateYmd((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 5: _t->save(); break;
        case 6: _t->clearAll(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (EntryController::*)()>(_a, &EntryController::dateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (EntryController::*)()>(_a, &EntryController::grandTotalChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (EntryController::*)()>(_a, &EntryController::statusChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (EntryController::*)()>(_a, &EntryController::snapshotSaved, 3))
            return;
    }
    if (_c == QMetaObject::RegisterPropertyMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 3:
        case 2:
        case 1:
        case 0:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< CategoryModel* >(); break;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<CategoryModel**>(_v) = _t->stocks(); break;
        case 1: *reinterpret_cast<CategoryModel**>(_v) = _t->coins(); break;
        case 2: *reinterpret_cast<CategoryModel**>(_v) = _t->metals(); break;
        case 3: *reinterpret_cast<CategoryModel**>(_v) = _t->etfs(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->dateText(); break;
        case 5: *reinterpret_cast<int*>(_v) = _t->calYear(); break;
        case 6: *reinterpret_cast<int*>(_v) = _t->calMonth(); break;
        case 7: *reinterpret_cast<int*>(_v) = _t->calDay(); break;
        case 8: *reinterpret_cast<QString*>(_v) = _t->grandTotalText(); break;
        case 9: *reinterpret_cast<QString*>(_v) = _t->statusText(); break;
        case 10: *reinterpret_cast<bool*>(_v) = _t->statusIsError(); break;
        default: break;
        }
    }
}

const QMetaObject *EntryController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *EntryController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15EntryControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int EntryController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void EntryController::dateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void EntryController::grandTotalChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void EntryController::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void EntryController::snapshotSaved()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
