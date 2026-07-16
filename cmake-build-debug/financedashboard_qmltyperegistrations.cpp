/****************************************************************************
** Generated QML type registration code
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <QtQml/qqml.h>
#include <QtQml/qqmlmoduleregistration.h>

#if __has_include(<CashFlowController.h>)
#  include <CashFlowController.h>
#endif
#if __has_include(<CategoryController.h>)
#  include <CategoryController.h>
#endif
#if __has_include(<DataEntryController.h>)
#  include <DataEntryController.h>
#endif
#if __has_include(<InvestmentsController.h>)
#  include <InvestmentsController.h>
#endif
#if __has_include(<NetWorthController.h>)
#  include <NetWorthController.h>
#endif
#if __has_include(<ProjectsController.h>)
#  include <ProjectsController.h>
#endif
#if __has_include(<SavingsController.h>)
#  include <SavingsController.h>
#endif
#if __has_include(<SnapshotImportController.h>)
#  include <SnapshotImportController.h>
#endif


#if !defined(QT_STATIC)
#define Q_QMLTYPE_EXPORT Q_DECL_EXPORT
#else
#define Q_QMLTYPE_EXPORT
#endif
Q_QMLTYPE_EXPORT void qml_register_types_FinanceDashboard()
{
    QT_WARNING_PUSH QT_WARNING_DISABLE_DEPRECATED
    qmlRegisterTypesAndRevisions<CashFlowController>("FinanceDashboard", 1);
    qmlRegisterTypesAndRevisions<CategoryController>("FinanceDashboard", 1);
    qmlRegisterTypesAndRevisions<DataEntryController>("FinanceDashboard", 1);
    qmlRegisterTypesAndRevisions<InvestmentsController>("FinanceDashboard", 1);
    qmlRegisterTypesAndRevisions<NetWorthController>("FinanceDashboard", 1);
    qmlRegisterTypesAndRevisions<ProjectsController>("FinanceDashboard", 1);
    qmlRegisterTypesAndRevisions<SavingsController>("FinanceDashboard", 1);
    qmlRegisterTypesAndRevisions<SnapshotImportController>("FinanceDashboard", 1);
    QT_WARNING_POP
    qmlRegisterModule("FinanceDashboard", 1, 0);
}

static const QQmlModuleRegistration financeDashboardRegistration("FinanceDashboard", qml_register_types_FinanceDashboard);
