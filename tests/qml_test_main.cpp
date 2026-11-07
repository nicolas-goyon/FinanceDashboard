// The QML tests of tests/qml (Qt Quick Test). Each tst_*.qml there is one
// test case. It imports the components it tests from qml/components: by path,
// or with `import FinanceDashboard` (a copy of the components with a qmldir,
// made by tests/CMakeLists.txt and given with -import).
#include <QtQuickTest/quicktest.h>

QUICK_TEST_MAIN(qml_test)
