import QtQuick
import QtQuick.Controls.Basic

// ── StaticImports ────────────────────────────────────────────────────────────
// Build-only helper for static Qt builds (LINK_MODE=static, see
// docs/docker-build.md): never loaded by the UI and deliberately NOT listed in
// CMakeLists.txt's QML_FILES. main.cpp picks the "Basic" Quick Controls style
// at run time (QQuickStyle::setStyle), which a static build can't see — Qt
// only links the QML plugins some .qml file imports explicitly ("Explicit
// imports are also necessary if your application is built statically",
// https://doc.qt.io/qt-6/qtquickcontrols-styles.html). qt_import_qml_plugins()
// runs qmlimportscanner over the whole project source directory, so the
// `import QtQuick.Controls.Basic` above is enough to get that style plugin
// linked in. Harmless for shared builds.
Item {}
