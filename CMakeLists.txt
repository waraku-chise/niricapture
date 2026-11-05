cmake_minimum_required(VERSION 3.24)
project(niricapture VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core Gui Test Widgets)
find_package(LayerShellQt REQUIRED)

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
  src/eyedropper.cpp
  src/eyedropper.hpp
  src/surface-capture.cpp
  src/desktop-capture.cpp
  src/desktop-capture.hpp
  src/region-selector.cpp
  src/region-selector.hpp
  src/editor.cpp
  src/editor.hpp
)
qt_add_resources(niricapture "capture-fonts"
  PREFIX "/fonts"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neucha.ttf"
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
  Qt6::Gui
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
  src/cli-path.cpp
  src/cli-path.hpp
  src/capture.cpp
  src/capture.hpp
  src/surface-capture.cpp
  src/desktop-capture.cpp
  src/desktop-capture.hpp
  src/region-selector.cpp
  src/region-selector.hpp
  src/editor.cpp
  src/editor.hpp
  src/eyedropper.cpp
  src/eyedropper.hpp
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
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neucha.ttf"
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
install(FILES NOTICE LICENSES/MIT-nirisnap.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/niricapture"
)
install(FILES niricapture.desktop
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)
install(FILES packaging/niricapture-keybinds.kdl
  DESTINATION "${CMAKE_INSTALL_DATADIR}/niricapture"
)
install(FILES README.md README.zh-CN.md
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/niricapture"
)
