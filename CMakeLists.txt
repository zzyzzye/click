cmake_minimum_required(VERSION 3.24)

project(QtClicker VERSION 0.1.0 LANGUAGES CXX)

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
)

set(APP_HEADERS
  src/app/MainWindow.h
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
  list(APPEND PLATFORM_SOURCES
    src/platform/windows/WindowsInputApi.cpp
    src/platform/windows/WindowsClickBackend.cpp
  )
  list(APPEND APP_HEADERS
    src/platform/windows/WindowsInputApi.h
    src/platform/windows/WindowsClickBackend.h
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

target_include_directories(QtClicker PRIVATE src)
target_link_libraries(QtClicker PRIVATE QtClickerCore Qt6::Widgets)

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
endif()
