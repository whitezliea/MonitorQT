cmake_minimum_required(VERSION 3.16)

project(MonitorQT VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES MonitorQT_zh_CN.ts)

set(APP_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        navigation/NavigationPage.h
        navigation/NavigationService.cpp
        navigation/NavigationService.h
        pages/PagePlaceholderWidget.cpp
        pages/PagePlaceholderWidget.h
        phase0/SourceBehaviorFreeze.cpp
        phase0/SourceBehaviorFreeze.h
        shell/BottomStatusBarWidget.cpp
        shell/BottomStatusBarWidget.h
        shell/SideNavigationWidget.cpp
        shell/SideNavigationWidget.h
        shell/TopStatusBarWidget.cpp
        shell/TopStatusBarWidget.h
)

set(PROJECT_SOURCES
        ${APP_SOURCES}
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MonitorQT
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET MonitorQT APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${APP_SOURCES} ${TS_FILES})
else()
    if(ANDROID)
        add_library(MonitorQT SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(MonitorQT
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${APP_SOURCES} ${TS_FILES})
endif()

target_link_libraries(MonitorQT PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_compile_options(MonitorQT PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MonitorQT)
endif()
set_target_properties(MonitorQT PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MonitorQT
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MonitorQT)
endif()
