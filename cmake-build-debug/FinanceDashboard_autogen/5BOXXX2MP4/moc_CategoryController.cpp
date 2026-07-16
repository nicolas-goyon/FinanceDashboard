/****************************************************************************
** Meta object code from reading C++ file 'CategoryController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/category/CategoryController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CategoryController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18CategoryControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto CategoryController::qt_create_metaobjectdata<qt_meta_tag_ZN18CategoryControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CategoryController",
        "QML.Element",
        "auto",
        "QML.Singleton",
        "true",
        "categoriesChanged",
        "",
        "categoryColor",
        "categoryName",
        "subcategoryColor",
        "subcategoryName",
        "addCategory",
        "name",
        "color",
        "renameCategory",
        "index",
        "newName",
        "setCategoryColor",
        "removeCategory",
        "addSubcategory",
        "categoryIndex",
        "renameSubcategory",
        "subIndex",
        "setSubcategoryColor",
        "removeSubcategory",
        "categories",
        "QVariantList"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'categoriesChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'categoryColor'
        QtMocHelpers::MethodData<QString(const QString &) const>(7, 6, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 8 },
        }}),
        // Method 'subcategoryColor'
        QtMocHelpers::MethodData<QString(const QString &, const QString &) const>(9, 6, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 8 }, { QMetaType::QString, 10 },
        }}),
        // Method 'addCategory'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(11, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 12 }, { QMetaType::QString, 13 },
        }}),
        // Method 'renameCategory'
        QtMocHelpers::MethodData<void(int, const QString &)>(14, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 }, { QMetaType::QString, 16 },
        }}),
        // Method 'setCategoryColor'
        QtMocHelpers::MethodData<void(int, const QString &)>(17, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 }, { QMetaType::QString, 13 },
        }}),
        // Method 'removeCategory'
        QtMocHelpers::MethodData<void(int)>(18, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Method 'addSubcategory'
        QtMocHelpers::MethodData<void(int, const QString &, const QString &)>(19, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 20 }, { QMetaType::QString, 12 }, { QMetaType::QString, 13 },
        }}),
        // Method 'renameSubcategory'
        QtMocHelpers::MethodData<void(int, int, const QString &)>(21, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 20 }, { QMetaType::Int, 22 }, { QMetaType::QString, 16 },
        }}),
        // Method 'setSubcategoryColor'
        QtMocHelpers::MethodData<void(int, int, const QString &)>(23, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 20 }, { QMetaType::Int, 22 }, { QMetaType::QString, 13 },
        }}),
        // Method 'removeSubcategory'
        QtMocHelpers::MethodData<void(int, int)>(24, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 20 }, { QMetaType::Int, 22 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'categories'
        QtMocHelpers::PropertyData<QVariantList>(25, 0x80000000 | 26, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
            {    3,    4 },
    });
    return QtMocHelpers::metaObjectData<CategoryController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject CategoryController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CategoryControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CategoryControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18CategoryControllerE_t>.metaTypes,
    nullptr
} };

void CategoryController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CategoryController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->categoriesChanged(); break;
        case 1: { QString _r = _t->categoryColor((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 2: { QString _r = _t->subcategoryColor((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 3: _t->addCategory((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->renameCategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->setCategoryColor((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->removeCategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->addSubcategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 8: _t->renameSubcategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 9: _t->setSubcategoryColor((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 10: _t->removeSubcategory((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CategoryController::*)()>(_a, &CategoryController::categoriesChanged, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->categories(); break;
        default: break;
        }
    }
}

const QMetaObject *CategoryController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CategoryController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CategoryControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CategoryController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
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
void CategoryController::categoriesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
