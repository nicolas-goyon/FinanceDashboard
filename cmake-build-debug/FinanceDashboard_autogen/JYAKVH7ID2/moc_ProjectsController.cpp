/****************************************************************************
** Meta object code from reading C++ file 'ProjectsController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/controllers/projects/ProjectsController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ProjectsController.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18ProjectsControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto ProjectsController::qt_create_metaobjectdata<qt_meta_tag_ZN18ProjectsControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ProjectsController",
        "QML.Element",
        "auto",
        "projectsChanged",
        "",
        "projectChanged",
        "loanChanged",
        "addProject",
        "name",
        "target",
        "monthsToTarget",
        "earmarked",
        "monthly",
        "projectionTimestamps",
        "QVariantList",
        "projectionValues",
        "projectNames",
        "projectCards",
        "currentProject",
        "kpis",
        "targetAmount",
        "chartMax",
        "minTimeMs",
        "maxTimeMs",
        "loanRatePct",
        "loanScenarios",
        "saveVsBorrowText"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'projectsChanged'
        QtMocHelpers::SignalData<void()>(3, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'projectChanged'
        QtMocHelpers::SignalData<void()>(5, 4, QMC::AccessPublic, QMetaType::Void),
        // Signal 'loanChanged'
        QtMocHelpers::SignalData<void()>(6, 4, QMC::AccessPublic, QMetaType::Void),
        // Method 'addProject'
        QtMocHelpers::MethodData<void(const QString &, double, int, double, double)>(7, 4, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 }, { QMetaType::Double, 9 }, { QMetaType::Int, 10 }, { QMetaType::Double, 11 },
            { QMetaType::Double, 12 },
        }}),
        // Method 'projectionTimestamps'
        QtMocHelpers::MethodData<QVariantList() const>(13, 4, QMC::AccessPublic, 0x80000000 | 14),
        // Method 'projectionValues'
        QtMocHelpers::MethodData<QVariantList() const>(15, 4, QMC::AccessPublic, 0x80000000 | 14),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'projectNames'
        QtMocHelpers::PropertyData<QStringList>(16, QMetaType::QStringList, QMC::DefaultPropertyFlags, 0),
        // property 'projectCards'
        QtMocHelpers::PropertyData<QVariantList>(17, 0x80000000 | 14, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'currentProject'
        QtMocHelpers::PropertyData<int>(18, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'kpis'
        QtMocHelpers::PropertyData<QVariantList>(19, 0x80000000 | 14, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
        // property 'targetAmount'
        QtMocHelpers::PropertyData<double>(20, QMetaType::Double, QMC::DefaultPropertyFlags, 1),
        // property 'chartMax'
        QtMocHelpers::PropertyData<double>(21, QMetaType::Double, QMC::DefaultPropertyFlags, 1),
        // property 'minTimeMs'
        QtMocHelpers::PropertyData<double>(22, QMetaType::Double, QMC::DefaultPropertyFlags, 1),
        // property 'maxTimeMs'
        QtMocHelpers::PropertyData<double>(23, QMetaType::Double, QMC::DefaultPropertyFlags, 1),
        // property 'loanRatePct'
        QtMocHelpers::PropertyData<double>(24, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 2),
        // property 'loanScenarios'
        QtMocHelpers::PropertyData<QVariantList>(25, 0x80000000 | 14, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'saveVsBorrowText'
        QtMocHelpers::PropertyData<QString>(26, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<ProjectsController, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject ProjectsController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18ProjectsControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18ProjectsControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18ProjectsControllerE_t>.metaTypes,
    nullptr
} };

void ProjectsController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ProjectsController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->projectsChanged(); break;
        case 1: _t->projectChanged(); break;
        case 2: _t->loanChanged(); break;
        case 3: _t->addProject((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[5]))); break;
        case 4: { QVariantList _r = _t->projectionTimestamps();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 5: { QVariantList _r = _t->projectionValues();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ProjectsController::*)()>(_a, &ProjectsController::projectsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProjectsController::*)()>(_a, &ProjectsController::projectChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProjectsController::*)()>(_a, &ProjectsController::loanChanged, 2))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QStringList*>(_v) = _t->projectNames(); break;
        case 1: *reinterpret_cast<QVariantList*>(_v) = _t->projectCards(); break;
        case 2: *reinterpret_cast<int*>(_v) = _t->currentProject(); break;
        case 3: *reinterpret_cast<QVariantList*>(_v) = _t->kpis(); break;
        case 4: *reinterpret_cast<double*>(_v) = _t->targetAmount(); break;
        case 5: *reinterpret_cast<double*>(_v) = _t->chartMax(); break;
        case 6: *reinterpret_cast<double*>(_v) = _t->minTimeMs(); break;
        case 7: *reinterpret_cast<double*>(_v) = _t->maxTimeMs(); break;
        case 8: *reinterpret_cast<double*>(_v) = _t->loanRatePct(); break;
        case 9: *reinterpret_cast<QVariantList*>(_v) = _t->loanScenarios(); break;
        case 10: *reinterpret_cast<QString*>(_v) = _t->saveVsBorrowText(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 2: _t->setCurrentProject(*reinterpret_cast<int*>(_v)); break;
        case 8: _t->setLoanRatePct(*reinterpret_cast<double*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *ProjectsController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ProjectsController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18ProjectsControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ProjectsController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void ProjectsController::projectsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ProjectsController::projectChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ProjectsController::loanChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
