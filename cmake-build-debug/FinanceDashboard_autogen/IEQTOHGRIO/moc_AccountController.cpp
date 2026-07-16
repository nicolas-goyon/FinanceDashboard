/****************************************************************************
** Meta object code from reading C++ file 'AccountController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/dataentry/AccountController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AccountController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17AccountControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto AccountController::qt_create_metaobjectdata<qt_meta_tag_ZN17AccountControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AccountController",
        "QML.Element",
        "auto",
        "QML.Singleton",
        "true",
        "accountsChanged",
        "",
        "accountCategoriesChanged",
        "accountTypesChanged",
        "statusChanged",
        "addAccount",
        "name",
        "type",
        "category",
        "currency",
        "notes",
        "updateAccount",
        "index",
        "removeAccount",
        "addAccountCategory",
        "color",
        "renameAccountCategory",
        "newName",
        "setAccountCategoryColor",
        "removeAccountCategory",
        "accountCategoryColor",
        "categoryName",
        "addAccountType",
        "renameAccountType",
        "removeAccountType",
        "addHash",
        "accountIndex",
        "hash",
        "linkAccountHash",
        "removeHash",
        "hashIndex",
        "accountHasHash",
        "transactionsForAccount",
        "QVariantList",
        "transactionCountForAccount",
        "reload",
        "accounts",
        "accountCategories",
        "accountTypes",
        "statusText"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'accountsChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'accountCategoriesChanged'
        QtMocHelpers::SignalData<void()>(7, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'accountTypesChanged'
        QtMocHelpers::SignalData<void()>(8, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void()>(9, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'addAccount'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &, const QString &, const QString &)>(10, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 12 }, { QMetaType::QString, 13 }, { QMetaType::QString, 14 },
            { QMetaType::QString, 15 },
        }}),
        // Method 'updateAccount'
        QtMocHelpers::MethodData<void(int, const QString &, const QString &, const QString &, const QString &, const QString &)>(16, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::QString, 11 }, { QMetaType::QString, 12 }, { QMetaType::QString, 13 },
            { QMetaType::QString, 14 }, { QMetaType::QString, 15 },
        }}),
        // Method 'removeAccount'
        QtMocHelpers::MethodData<void(int)>(18, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 },
        }}),
        // Method 'addAccountCategory'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(19, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 20 },
        }}),
        // Method 'renameAccountCategory'
        QtMocHelpers::MethodData<void(int, const QString &)>(21, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::QString, 22 },
        }}),
        // Method 'setAccountCategoryColor'
        QtMocHelpers::MethodData<void(int, const QString &)>(23, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::QString, 20 },
        }}),
        // Method 'removeAccountCategory'
        QtMocHelpers::MethodData<void(int)>(24, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 },
        }}),
        // Method 'accountCategoryColor'
        QtMocHelpers::MethodData<QString(const QString &) const>(25, 6, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 26 },
        }}),
        // Method 'addAccountType'
        QtMocHelpers::MethodData<void(const QString &)>(27, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Method 'renameAccountType'
        QtMocHelpers::MethodData<void(int, const QString &)>(28, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::QString, 22 },
        }}),
        // Method 'removeAccountType'
        QtMocHelpers::MethodData<void(int)>(29, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 },
        }}),
        // Method 'addHash'
        QtMocHelpers::MethodData<bool(int, const QString &)>(30, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 31 }, { QMetaType::QString, 32 },
        }}),
        // Method 'linkAccountHash'
        QtMocHelpers::MethodData<bool(int, const QString &)>(33, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 31 }, { QMetaType::QString, 32 },
        }}),
        // Method 'removeHash'
        QtMocHelpers::MethodData<void(int, int)>(34, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 31 }, { QMetaType::Int, 35 },
        }}),
        // Method 'accountHasHash'
        QtMocHelpers::MethodData<bool(int, const QString &) const>(36, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 31 }, { QMetaType::QString, 32 },
        }}),
        // Method 'transactionsForAccount'
        QtMocHelpers::MethodData<QVariantList(int) const>(37, 6, QMC::AccessPublic, 0x80000000 | 38, {{
            { QMetaType::Int, 31 },
        }}),
        // Method 'transactionCountForAccount'
        QtMocHelpers::MethodData<int(int) const>(39, 6, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 31 },
        }}),
        // Method 'reload'
        QtMocHelpers::MethodData<void()>(40, 6, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'accounts'
        QtMocHelpers::PropertyData<QVariantList>(41, 0x80000000 | 38, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'accountCategories'
        QtMocHelpers::PropertyData<QVariantList>(42, 0x80000000 | 38, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
        // property 'accountTypes'
        QtMocHelpers::PropertyData<QVariantList>(43, 0x80000000 | 38, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'statusText'
        QtMocHelpers::PropertyData<QString>(44, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
            {    3,    4 },
    });
    return QtMocHelpers::metaObjectData<AccountController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject AccountController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17AccountControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17AccountControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17AccountControllerE_t>.metaTypes,
    nullptr
} };

void AccountController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AccountController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->accountsChanged(); break;
        case 1: _t->accountCategoriesChanged(); break;
        case 2: _t->accountTypesChanged(); break;
        case 3: _t->statusChanged(); break;
        case 4: _t->addAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5]))); break;
        case 5: _t->updateAccount((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[6]))); break;
        case 6: _t->removeAccount((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->addAccountCategory((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->renameAccountCategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 9: _t->setAccountCategoryColor((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 10: _t->removeAccountCategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: { QString _r = _t->accountCategoryColor((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 12: _t->addAccountType((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->renameAccountType((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 14: _t->removeAccountType((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 15: { bool _r = _t->addHash((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 16: { bool _r = _t->linkAccountHash((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 17: _t->removeHash((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 18: { bool _r = _t->accountHasHash((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 19: { QVariantList _r = _t->transactionsForAccount((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 20: { int _r = _t->transactionCountForAccount((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 21: _t->reload(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AccountController::*)()>(_a, &AccountController::accountsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AccountController::*)()>(_a, &AccountController::accountCategoriesChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AccountController::*)()>(_a, &AccountController::accountTypesChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AccountController::*)()>(_a, &AccountController::statusChanged, 3))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->accounts(); break;
        case 1: *reinterpret_cast<QVariantList*>(_v) = _t->accountCategories(); break;
        case 2: *reinterpret_cast<QVariantList*>(_v) = _t->accountTypes(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->statusText(); break;
        default: break;
        }
    }
}

const QMetaObject *AccountController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AccountController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17AccountControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AccountController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 22)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 22;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 22)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 22;
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
void AccountController::accountsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AccountController::accountCategoriesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void AccountController::accountTypesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void AccountController::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
