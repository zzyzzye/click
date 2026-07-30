cmake_minimum_required(VERSION 3.24)

project(ClickFlow VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(CTest)

if(APPLE)
  foreach(qt_prefix IN ITEMS
      /opt/homebrew/opt/qt
      /usr/local/opt/qt
      /opt/homebrew/opt/qtbase
      /usr/local/opt/qtbase)
    if(EXISTS "${qt_prefix}/lib/cmake/Qt6" OR EXISTS "${qt_prefix}/lib/cmake")
      list(PREPEND CMAKE_PREFIX_PATH "${qt_prefix}")
      list(PREPEND CMAKE_PREFIX_PATH "${qt_prefix}/lib/cmake")
      break()
    endif()
  endforeach()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

set(CLICKFLOW_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
  src/app/ClickFlowVersion.h.in
  "${CLICKFLOW_GENERATED_DIR}/ClickFlowVersion.h"
  @ONLY
)

set(CORE_SOURCES
  src/core/ClickController.cpp
  src/core/ClickTypes.cpp
  src/core/SettingsRepository.cpp
)

set(CORE_HEADERS
  src/core/ClickBackend.h
  src/core/ClickController.h
  src/core/ClickTypes.h
  src/core/HotkeyService.h
  src/core/SettingsRepository.h
)

add_library(QtClickerCore STATIC
  ${CORE_SOURCES}
  ${CORE_HEADERS}
)

target_include_directories(QtClickerCore PUBLIC src)
target_link_libraries(QtClickerCore PUBLIC Qt6::Core)

set(APP_SOURCES
  src/app/main.cpp
  src/app/MainWindow.cpp
  src/app/AppIdentity.cpp
)

set(APP_HEADERS
  src/app/MainWindow.h
  src/app/AppIdentity.h
  src/platform/PlatformServices.h
)

if(APPLE)
  enable_language(OBJCXX)
  list(APPEND PLATFORM_SOURCES
    src/platform/macos/MacOSClickBackend.mm
    src/platform/macos/MacOSHotkeyService.mm
    src/platform/macos/PlatformServicesMac.mm
  )
  list(APPEND APP_HEADERS
    src/platform/macos/MacOSClickBackend.h
    src/platform/macos/MacOSHotkeyService.h
  )
elseif(WIN32)
  configure_file(
    src/app/ClickFlow.rc.in
    "${CLICKFLOW_GENERATED_DIR}/ClickFlow.rc"
    @ONLY
  )
  list(APPEND APP_SOURCES "${CLICKFLOW_GENERATED_DIR}/ClickFlow.rc")
  list(APPEND PLATFORM_SOURCES
    src/platform/windows/WindowsInputApi.cpp
    src/platform/windows/WindowsClickBackend.cpp
    src/platform/windows/WindowsHotkeyMapping.cpp
    src/platform/windows/WindowsHotkeyApi.cpp
    src/platform/windows/WindowsHotkeyService.cpp
    src/platform/windows/PlatformServicesWindows.cpp
  )
  list(APPEND APP_HEADERS
    src/platform/windows/WindowsInputApi.h
    src/platform/windows/WindowsClickBackend.h
    src/platform/windows/WindowsHotkeyMapping.h
    src/platform/windows/WindowsHotkeyApi.h
    src/platform/windows/WindowsHotkeyService.h
  )
else()
  message(FATAL_ERROR "QtClicker supports only macOS and Windows.")
endif()

add_executable(QtClicker MACOSX_BUNDLE
  ${APP_SOURCES}
  ${CORE_SOURCES}
  ${PLATFORM_SOURCES}
  ${APP_HEADERS}
  ${CORE_HEADERS}
)

target_include_directories(QtClicker PRIVATE src "${CLICKFLOW_GENERATED_DIR}")
target_link_libraries(QtClicker PRIVATE QtClickerCore Qt6::Widgets)
set_target_properties(QtClicker PROPERTIES OUTPUT_NAME ClickFlow)

if(APPLE)
  target_link_libraries(QtClicker PRIVATE
    "-framework ApplicationServices"
    "-framework Carbon"
  )
elseif(WIN32)
  target_link_libraries(QtClicker PRIVATE user32)
endif()

add_executable(QtClickerTests
  tests/ClickerTests.cpp
)

target_include_directories(QtClickerTests PRIVATE src)
target_link_libraries(QtClickerTests PRIVATE QtClickerCore Qt6::Test Qt6::Widgets)

add_test(NAME QtClickerTests COMMAND QtClickerTests)
if(WIN32)
  set_tests_properties(QtClickerTests PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()

add_executable(ClickFlowMacroTypesTests
  tests/MacroTypesTests.cpp
  src/core/MacroTypes.cpp
  src/core/MacroTypes.h
  src/core/MacroRepository.cpp
  src/core/MacroRepository.h
)
target_include_directories(ClickFlowMacroTypesTests PRIVATE src)
target_link_libraries(ClickFlowMacroTypesTests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MacroTypes COMMAND ClickFlowMacroTypesTests)
if(WIN32)
  set_tests_properties(MacroTypes PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()

add_executable(ClickFlowIdentityTests
  tests/AppIdentityTests.cpp
  src/app/AppIdentity.cpp
  src/app/AppIdentity.h
)
target_include_directories(ClickFlowIdentityTests PRIVATE
  src
  "${CLICKFLOW_GENERATED_DIR}"
)
target_link_libraries(ClickFlowIdentityTests PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME AppIdentity COMMAND ClickFlowIdentityTests)
if(WIN32)
  set_tests_properties(AppIdentity PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()

set(SHELL_WIDGET_SOURCES
  src/app/resources/ClickFlowResources.qrc
  src/app/widgets/NavigationSidebar.cpp
  src/app/widgets/NavigationSidebar.h
  src/app/widgets/StatusStrip.cpp
  src/app/widgets/StatusStrip.h
  src/app/widgets/ActionBar.cpp
  src/app/widgets/ActionBar.h
)

add_executable(ClickFlowProductShellTests
  tests/ProductShellTests.cpp
  ${SHELL_WIDGET_SOURCES}
)
target_include_directories(ClickFlowProductShellTests PRIVATE
  src
  "${CLICKFLOW_GENERATED_DIR}"
)
target_link_libraries(ClickFlowProductShellTests PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME ProductShell COMMAND ClickFlowProductShellTests)
if(WIN32)
  set_tests_properties(ProductShell PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()

set(SHELL_PAGE_SOURCES
  src/app/pages/ClickSettingsPage.cpp
  src/app/pages/ClickSettingsPage.h
  src/app/pages/HotkeySettingsPage.cpp
  src/app/pages/HotkeySettingsPage.h
  src/app/pages/PresetsAboutPage.cpp
  src/app/pages/PresetsAboutPage.h
)

target_sources(QtClicker PRIVATE ${SHELL_WIDGET_SOURCES} ${SHELL_PAGE_SOURCES})

add_executable(ClickFlowPageTests
  tests/ClickFlowPageTests.cpp
  ${SHELL_PAGE_SOURCES}
)
target_include_directories(ClickFlowPageTests PRIVATE src)
target_link_libraries(ClickFlowPageTests PRIVATE QtClickerCore Qt6::Test Qt6::Widgets)
add_test(NAME ClickFlowPages COMMAND ClickFlowPageTests)
if(WIN32)
  set_tests_properties(ClickFlowPages PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()

if(WIN32)
  add_executable(QtClickerWindowsTests
    tests/WindowsClickBackendTests.cpp
    src/platform/windows/WindowsInputApi.cpp
    src/platform/windows/WindowsClickBackend.cpp
    src/platform/windows/WindowsInputApi.h
    src/platform/windows/WindowsClickBackend.h
  )
  target_include_directories(QtClickerWindowsTests PRIVATE src)
  target_link_libraries(QtClickerWindowsTests PRIVATE
    QtClickerCore
    Qt6::Test
    Qt6::Widgets
    user32
  )
  add_test(NAME WindowsClickBackend COMMAND QtClickerWindowsTests)
  set_tests_properties(WindowsClickBackend PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )

  add_executable(QtClickerWindowsHotkeyTests
    tests/WindowsHotkeyTests.cpp
    src/platform/windows/WindowsHotkeyMapping.cpp
    src/platform/windows/WindowsHotkeyMapping.h
    src/platform/windows/WindowsHotkeyApi.cpp
    src/platform/windows/WindowsHotkeyApi.h
    src/platform/windows/WindowsHotkeyService.cpp
    src/platform/windows/WindowsHotkeyService.h
  )
  target_include_directories(QtClickerWindowsHotkeyTests PRIVATE src)
  target_link_libraries(QtClickerWindowsHotkeyTests PRIVATE
    QtClickerCore
    Qt6::Test
    Qt6::Widgets
    user32
  )
  add_test(NAME WindowsHotkeyMapping COMMAND QtClickerWindowsHotkeyTests)
  set_tests_properties(WindowsHotkeyMapping PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )

  add_executable(QtClickerMainWindowTests
    tests/MainWindowTests.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    ${SHELL_WIDGET_SOURCES}
    ${SHELL_PAGE_SOURCES}
    src/platform/windows/WindowsInputApi.cpp
    src/platform/windows/WindowsClickBackend.cpp
    src/platform/windows/WindowsHotkeyMapping.cpp
    src/platform/windows/WindowsHotkeyApi.cpp
    src/platform/windows/WindowsHotkeyService.cpp
    src/platform/windows/WindowsHotkeyService.h
    src/platform/windows/PlatformServicesWindows.cpp
  )
  target_include_directories(QtClickerMainWindowTests PRIVATE src)
  target_link_libraries(QtClickerMainWindowTests PRIVATE
    QtClickerCore
    Qt6::Test
    Qt6::Widgets
    user32
  )
  add_test(NAME MainWindow COMMAND QtClickerMainWindowTests)
  set_tests_properties(MainWindow PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()
