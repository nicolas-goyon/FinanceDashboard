/****************************************************************************
** Meta object code from reading C++ file 'SnapshotImportController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/dataentry/SnapshotImportController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SnapshotImportController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN24SnapshotImportControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto SnapshotImportController::qt_create_metaobjectdata<qt_meta_tag_ZN24SnapshotImportControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SnapshotImportController",
        "QML.Element",
        "auto",
        "statementChanged",
        "",
        "logChanged",
        "statusChanged",
        "parseFile",
        "QUrl",
        "fileUrl",
        "templateId",
        "setRowCategory",
        "row",
        "category",
        "setRowSubcategory",
        "subcategory",
        "setRowIncluded",
        "included",
        "matchAccountIndex",
        "QVariantList",
        "accounts",
        "reset",
        "save",
        "accountId",
        "accountLabel",
        "templates",
        "hasParsed",
        "lastParseFailed",
        "sourceFileName",
        "statementInfo",
        "QVariantMap",
        "transactionRows",
        "logEntries",
        "errorCount",
        "warningCount",
        "statusText"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'statementChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'logChanged'
        QtMocHelpers::SignalData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void()>(6, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'parseFile'
        QtMocHelpers::MethodData<void(const QUrl &, const QString &)>(7, 4, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 9 }, { QMetaType::QString, 10 },
        }}),
        // Method 'setRowCategory'
        QtMocHelpers::MethodData<void(int, const QString &)>(11, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 12 }, { QMetaType::QString, 13 },
        }}),
        // Method 'setRowSubcategory'
        QtMocHelpers::MethodData<void(int, const QString &)>(14, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 12 }, { QMetaType::QString, 15 },
        }}),
        // Method 'setRowIncluded'
        QtMocHelpers::MethodData<void(int, bool)>(16, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 12 }, { QMetaType::Bool, 17 },
        }}),
        // Method 'matchAccountIndex'
        QtMocHelpers::MethodData<int(const QVariantList &) const>(18, 4, QMC::AccessPublic, QMetaType::Int, {{
            { 0x80000000 | 19, 20 },
        }}),
        // Method 'reset'
        QtMocHelpers::MethodData<void()>(21, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'save'
        QtMocHelpers::MethodData<void(int, const QString &)>(22, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 23 }, { QMetaType::QString, 24 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'templates'
        QtMocHelpers::PropertyData<QVariantList>(25, 0x80000000 | 19, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'hasParsed'
        QtMocHelpers::PropertyData<bool>(26, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'lastParseFailed'
        QtMocHelpers::PropertyData<bool>(27, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'sourceFileName'
        QtMocHelpers::PropertyData<QString>(28, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'statementInfo'
        QtMocHelpers::PropertyData<QVariantMap>(29, 0x80000000 | 30, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'transactionRows'
        QtMocHelpers::PropertyData<QVariantList>(31, 0x80000000 | 19, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'logEntries'
        QtMocHelpers::PropertyData<QVariantList>(32, 0x80000000 | 19, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
        // property 'errorCount'
        QtMocHelpers::PropertyData<int>(33, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'warningCount'
        QtMocHelpers::PropertyData<int>(34, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'statusText'
        QtMocHelpers::PropertyData<QString>(35, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<SnapshotImportController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject SnapshotImportController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24SnapshotImportControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24SnapshotImportControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN24SnapshotImportControllerE_t>.metaTypes,
    nullptr
} };

void SnapshotImportController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SnapshotImportController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->statementChanged(); break;
        case 1: _t->logChanged(); break;
        case 2: _t->statusChanged(); break;
        case 3: _t->parseFile((*reinterpret_cast<std::add_pointer_t<QUrl>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->setRowCategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->setRowSubcategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->setRowIncluded((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 7: { int _r = _t->matchAccountIndex((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 8: _t->reset(); break;
        case 9: _t->save((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SnapshotImportController::*)()>(_a, &SnapshotImportController::statementChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SnapshotImportController::*)()>(_a, &SnapshotImportController::logChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SnapshotImportController::*)()>(_a, &SnapshotImportController::statusChanged, 2))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->templates(); break;
        case 1: *reinterpret_cast<bool*>(_v) = _t->hasParsed(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->lastParseFailed(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->sourceFileName(); break;
        case 4: *reinterpret_cast<QVariantMap*>(_v) = _t->statementInfo(); break;
        case 5: *reinterpret_cast<QVariantList*>(_v) = _t->transactionRows(); break;
        case 6: *reinterpret_cast<QVariantList*>(_v) = _t->logEntries(); break;
        case 7: *reinterpret_cast<int*>(_v) = _t->errorCount(); break;
        case 8: *reinterpret_cast<int*>(_v) = _t->warningCount(); break;
        case 9: *reinterpret_cast<QString*>(_v) = _t->statusText(); break;
        default: break;
        }
    }
}

const QMetaObject *SnapshotImportController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SnapshotImportController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24SnapshotImportControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SnapshotImportController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void SnapshotImportController::statementChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void SnapshotImportController::logChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void SnapshotImportController::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
