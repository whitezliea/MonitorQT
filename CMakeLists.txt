cmake_minimum_required(VERSION 3.16)

project(MonitorQT VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql LinguistTools)

function(monitor_apply_target_defaults target_name)
    target_include_directories(${target_name} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_options(${target_name} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    )
endfunction()

set(TS_FILES MonitorQT_zh_CN.ts)

set(DOMAIN_SOURCES
    domain/DomainLayer.cpp
    domain/DomainLayer.h
    phase0/SourceBehaviorFreeze.cpp
    phase0/SourceBehaviorFreeze.h
)

set(APPLICATION_SOURCES
    application/ApplicationLayer.cpp
    application/ApplicationLayer.h
    application/events/EventBus.cpp
    application/events/EventBus.h
)

set(INFRASTRUCTURE_SOURCES
    infrastructure/InfrastructureLayer.cpp
    infrastructure/InfrastructureLayer.h
)

set(SIMULATOR_SOURCES
    simulator/SimulatorLayer.cpp
    simulator/SimulatorLayer.h
)

set(PRESENTATION_SOURCES
    mainwindow.cpp
    mainwindow.h
    navigation/NavigationPage.h
    navigation/NavigationService.cpp
    navigation/NavigationService.h
    pages/PagePlaceholderWidget.cpp
    pages/PagePlaceholderWidget.h
    presentation/PresentationLayer.cpp
    presentation/PresentationLayer.h
    shell/BottomStatusBarWidget.cpp
    shell/BottomStatusBarWidget.h
    shell/SideNavigationWidget.cpp
    shell/SideNavigationWidget.h
    shell/TopStatusBarWidget.cpp
    shell/TopStatusBarWidget.h
)

set(BOOTSTRAP_SOURCES
    bootstrap/EventRegistration.cpp
    bootstrap/EventRegistration.h
    bootstrap/RuntimeComposition.cpp
    bootstrap/RuntimeComposition.h
    bootstrap/RuntimeCompositionDependencies.cpp
    bootstrap/RuntimeCompositionDependencies.h
)

add_library(MonitorDomain STATIC
    ${DOMAIN_SOURCES}
)
monitor_apply_target_defaults(MonitorDomain)
target_link_libraries(MonitorDomain PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)

add_library(MonitorApplication STATIC
    ${APPLICATION_SOURCES}
)
monitor_apply_target_defaults(MonitorApplication)
target_link_libraries(MonitorApplication PUBLIC
    MonitorDomain
    Qt${QT_VERSION_MAJOR}::Core
)

add_library(MonitorInfrastructure STATIC
    ${INFRASTRUCTURE_SOURCES}
)
monitor_apply_target_defaults(MonitorInfrastructure)
target_link_libraries(MonitorInfrastructure PUBLIC
    MonitorApplication
    Qt${QT_VERSION_MAJOR}::Sql
)

add_library(MonitorSimulator STATIC
    ${SIMULATOR_SOURCES}
)
monitor_apply_target_defaults(MonitorSimulator)
target_link_libraries(MonitorSimulator PUBLIC
    MonitorApplication
)

add_library(MonitorPresentation STATIC
    ${PRESENTATION_SOURCES}
)
monitor_apply_target_defaults(MonitorPresentation)
target_link_libraries(MonitorPresentation PUBLIC
    MonitorApplication
    Qt${QT_VERSION_MAJOR}::Widgets
)

set(EXECUTABLE_SOURCES
    main.cpp
    ${BOOTSTRAP_SOURCES}
    ${TS_FILES}
)

set(TRANSLATION_SOURCES
    ${EXECUTABLE_SOURCES}
    ${DOMAIN_SOURCES}
    ${APPLICATION_SOURCES}
    ${INFRASTRUCTURE_SOURCES}
    ${SIMULATOR_SOURCES}
    ${PRESENTATION_SOURCES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MonitorQT
        MANUAL_FINALIZATION
        ${EXECUTABLE_SOURCES}
    )

    qt_create_translation(QM_FILES ${TRANSLATION_SOURCES} ${TS_FILES})
else()
    if(ANDROID)
        add_library(MonitorQT SHARED
            ${EXECUTABLE_SOURCES}
        )
    else()
        add_executable(MonitorQT
            ${EXECUTABLE_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${TRANSLATION_SOURCES} ${TS_FILES})
endif()

monitor_apply_target_defaults(MonitorQT)
target_link_libraries(MonitorQT PRIVATE
    MonitorDomain
    MonitorApplication
    MonitorInfrastructure
    MonitorSimulator
    MonitorPresentation
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)

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
