cmake_minimum_required(VERSION 3.24)
project(niricapture VERSION 0.6.4 LANGUAGES CXX)
include(GNUInstallDirs)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core DBus Gui Quick QuickWidgets Test Widgets)
find_package(LayerShellQt REQUIRED)

set(NIRICAPTURE_EDITOR_SOURCES
  src/config.cpp
  src/config.hpp
  src/editor/tool.hpp
  src/editor/annotation-document.cpp
  src/editor/annotation-document.hpp
  src/editor/annotation-geometry.cpp
  src/editor/annotation-geometry.hpp
  src/editor/snap.cpp
  src/editor/snap.hpp
  src/editor/tool-registry.cpp
  src/editor/tool-registry.hpp
  src/editor/editor-canvas.cpp
  src/editor/editor-canvas.hpp
  src/editor/font-picker.cpp
  src/editor/font-picker.hpp
  src/editor/tool-bar.cpp
  src/editor/tool-bar.hpp
  src/editor/tools/builtin-tools.cpp
  src/editor/tools/builtin-tools.hpp
  src/editor/tools/two-point-tool.cpp
  src/editor/tools/two-point-tool.hpp
  src/editor/tools/freehand-tool.cpp
  src/editor/tools/freehand-tool.hpp
  src/editor/tools/select-tool.cpp
  src/editor/tools/select-tool.hpp
  src/editor/tools/pen-tool.cpp
  src/editor/tools/pen-tool.hpp
  src/editor/tools/highlighter-tool.cpp
  src/editor/tools/highlighter-tool.hpp
  src/editor/tools/arrow-tool.cpp
  src/editor/tools/arrow-tool.hpp
  src/editor/tools/line-tool.cpp
  src/editor/tools/line-tool.hpp
  src/editor/tools/rectangle-tool.cpp
  src/editor/tools/rectangle-tool.hpp
  src/editor/tools/ellipse-tool.cpp
  src/editor/tools/ellipse-tool.hpp
  src/editor/tools/eyedropper-tool.cpp
  src/editor/tools/eyedropper-tool.hpp
  src/editor/tools/text-tool.cpp
  src/editor/tools/text-tool.hpp
  src/editor/tools/marker-tool.cpp
  src/editor/tools/marker-tool.hpp
  src/editor/tools/mosaic-tool.cpp
  src/editor/tools/mosaic-tool.hpp
  src/editor/tools/blur-tool.cpp
  src/editor/tools/blur-tool.hpp
)

set(NIRICAPTURE_SETTINGS_QML
  src/settings/qml/Main.qml
  src/settings/qml/CheckListPage.qml
  src/settings/qml/PalettePage.qml
  src/settings/qml/CanvasPage.qml
  src/settings/qml/ThemePage.qml
  src/settings/qml/DataPage.qml
  src/settings/qml/ColorPopup.qml
  src/settings/qml/Card.qml
  src/settings/qml/ColorField.qml
  src/settings/qml/Icon.qml
  src/settings/qml/IconButton.qml
  src/settings/qml/MdButton.qml
  src/settings/qml/MdCheck.qml
  src/settings/qml/Snackbar.qml
  src/settings/qml/Md3.js
  src/settings/qml/Icons.js
)

qt_add_executable(niricapture
  src/main.cpp
  src/cli-path.cpp
  src/cli-path.hpp
  src/icons.cpp
  src/icons.hpp
  src/instance-lock.cpp
  src/instance-lock.hpp
  src/pin.cpp
  src/pin.hpp
  src/pin-file.cpp
  src/pin-file.hpp
  src/pin-layout.cpp
  src/pin-layout.hpp
  src/capture.cpp
  src/capture.hpp
  src/surface-capture.cpp
  src/desktop-capture.cpp
  src/desktop-capture.hpp
  src/region-selector.cpp
  src/region-selector.hpp
  ${NIRICAPTURE_EDITOR_SOURCES}
  src/i18n.cpp
  src/i18n.hpp
  src/editor-window.cpp
  src/editor-window.hpp
  src/settings-window.cpp
  src/settings-window.hpp
  src/md3.cpp
  src/md3.hpp
  src/settings/settings-bridge.cpp
  src/settings/settings-bridge.hpp
)
qt_add_resources(niricapture "capture-fonts"
  PREFIX "/fonts"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neucha.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/Roboto-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/Roboto-Medium.ttf"
)
qt_add_resources(niricapture "settings-qml"
  PREFIX "/settings"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/settings"
  FILES ${NIRICAPTURE_SETTINGS_QML}
)

target_include_directories(niricapture PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(niricapture PRIVATE cxx_std_23)
target_compile_options(niricapture PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(niricapture PRIVATE
  NIRICAPTURE_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(niricapture PRIVATE
  Qt6::Concurrent
  Qt6::Core
  Qt6::DBus
  Qt6::Gui
  Qt6::Quick
  Qt6::QuickWidgets
  Qt6::Widgets
  LayerShellQt::Interface
)


qt_add_executable(niricapture-smoke
  tests/editor-smoke.cpp
  tests/clipboard-smoke.cpp
  tests/clipboard-smoke.hpp
  src/icons.cpp
  src/icons.hpp
  src/instance-lock.cpp
  src/instance-lock.hpp
  tests/instance-lock-smoke.cpp
  tests/instance-lock-smoke.hpp
  tests/transform-smoke.cpp
  tests/transform-smoke.hpp
  tests/tool-smoke.cpp
  tests/tool-smoke.hpp
  tests/editor-window-smoke.cpp
  tests/editor-window-smoke.hpp
  src/settings-window.cpp
  src/settings-window.hpp
  src/md3.cpp
  src/md3.hpp
  src/settings/settings-bridge.cpp
  src/settings/settings-bridge.hpp
  tests/config-smoke.cpp
  tests/config-smoke.hpp
  src/cli-path.cpp
  src/cli-path.hpp
  src/capture.cpp
  src/capture.hpp
  src/surface-capture.cpp
  src/desktop-capture.cpp
  src/desktop-capture.hpp
  src/region-selector.cpp
  src/region-selector.hpp
  ${NIRICAPTURE_EDITOR_SOURCES}
  src/i18n.cpp
  src/i18n.hpp
  src/editor-window.cpp
  src/editor-window.hpp
  src/pin-file.cpp
  src/pin-file.hpp
  src/pin-layout.cpp
  src/pin-layout.hpp
  tests/pin-lifecycle-smoke.cpp
  tests/pin-lifecycle-smoke.hpp
  tests/pin-layout-smoke.cpp
  tests/pin-layout-smoke.hpp
)
qt_add_resources(niricapture-smoke "capture-fonts-smoke"
  PREFIX "/fonts"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neucha.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/Roboto-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/Roboto-Medium.ttf"
)
qt_add_resources(niricapture-smoke "settings-qml-smoke"
  PREFIX "/settings"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/settings"
  FILES ${NIRICAPTURE_SETTINGS_QML}
)
target_include_directories(niricapture-smoke PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
target_compile_features(niricapture-smoke PRIVATE cxx_std_23)
target_compile_options(niricapture-smoke PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(niricapture-smoke PRIVATE
  Qt6::Concurrent
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  Qt6::QuickWidgets
  Qt6::Test
  Qt6::Widgets
  LayerShellQt::Interface
)

qt_add_executable(niricapture-desktop-smoke
  tests/desktop-capture-smoke.cpp
  src/capture.cpp
  src/capture.hpp
  src/desktop-capture.cpp
  src/desktop-capture.hpp
)
target_include_directories(niricapture-desktop-smoke PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
target_compile_features(niricapture-desktop-smoke PRIVATE cxx_std_23)
target_compile_options(niricapture-desktop-smoke PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(niricapture-desktop-smoke PRIVATE
  Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Widgets
)

enable_testing()
add_test(NAME desktop-capture COMMAND niricapture-desktop-smoke)

install(TARGETS niricapture
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(FILES LICENSE
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/niricapture"
)
install(FILES assets/OFL.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/niricapture"
  RENAME Neucha-OFL.txt
)
install(FILES NOTICE LICENSES/MIT-nirisnap.txt LICENSES/Apache-2.0.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/niricapture"
)
install(FILES niricapture.desktop niricapture-editor.desktop
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)
install(FILES packaging/niricapture-keybinds.kdl
  packaging/niricapture-settings-window.kdl
  DESTINATION "${CMAKE_INSTALL_DATADIR}/niricapture"
)
install(FILES README.md README.en.md
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/niricapture"
)
