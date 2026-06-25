cmake_minimum_required(VERSION 3.16)

project(MonitorQT VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql LinguistTools)

enable_testing()

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
    domain/alarms/AlarmModels.cpp
    domain/alarms/AlarmModels.h
    domain/common/DomainCommon.cpp
    domain/common/DomainCommon.h
    domain/devices/DeviceModels.cpp
    domain/devices/DeviceModels.h
    domain/DomainLayer.cpp
    domain/DomainLayer.h
    domain/logs/LogModels.cpp
    domain/logs/LogModels.h
    domain/measurements/MeasurementModels.cpp
    domain/measurements/MeasurementModels.h
    domain/rules/DomainRules.cpp
    domain/rules/DomainRules.h
    domain/rules/MatrixStatisticsCalculator.cpp
    domain/rules/MatrixStatisticsCalculator.h
    domain/tags/TagModels.cpp
    domain/tags/TagModels.h
    domain/tasks/TaskModels.cpp
    domain/tasks/TaskModels.h
    phase0/SourceBehaviorFreeze.cpp
    phase0/SourceBehaviorFreeze.h
)

set(APPLICATION_SOURCES
    application/ApplicationLayer.cpp
    application/ApplicationLayer.h
    application/abstractions/IRawFrameSource.h
    application/caches/MatrixFrameCache.cpp
    application/caches/MatrixFrameCache.h
    application/caches/TagCache.cpp
    application/caches/TagCache.h
    application/configuration/ConfigurationValidation.cpp
    application/configuration/ConfigurationValidation.h
    application/configuration/MonitorRuntimeOptions.cpp
    application/configuration/MonitorRuntimeOptions.h
    application/configuration/RuntimeOptionsStore.cpp
    application/configuration/RuntimeOptionsStore.h
    application/configuration/TagRuntimeConfiguration.cpp
    application/configuration/TagRuntimeConfiguration.h
    application/configuration/TrendDiagnosisOptions.cpp
    application/configuration/TrendDiagnosisOptions.h
    application/dto/ApplicationDtos.h
    application/events/ApplicationEvents.cpp
    application/events/ApplicationEvents.h
    application/events/EventBus.cpp
    application/events/EventBus.h
    application/pipelines/DataCleanPipeline.cpp
    application/pipelines/DataCleanPipeline.h
    application/queues/ApplicationQueues.h
    application/queues/BlockingQueue.h
    application/runtime/AcquisitionRuntimeController.cpp
    application/runtime/AcquisitionRuntimeController.h
    application/runtime/DataSourceHealthMonitor.cpp
    application/runtime/DataSourceHealthMonitor.h
    application/runtime/MonitoringRuntimeService.cpp
    application/runtime/MonitoringRuntimeService.h
    application/runtime/PersistenceRuntimeCoordinator.cpp
    application/runtime/PersistenceRuntimeCoordinator.h
    application/runtime/RuntimeLifecycleCoordinator.cpp
    application/runtime/RuntimeLifecycleCoordinator.h
    application/services/AlarmService.cpp
    application/services/AlarmService.h
    application/services/ChartDataService.cpp
    application/services/ChartDataService.h
    application/services/DashboardService.cpp
    application/services/DashboardService.h
    application/services/MeasurementMapAnalysis.cpp
    application/services/MeasurementMapAnalysis.h
    application/services/MeasurementMapService.cpp
    application/services/MeasurementMapService.h
    application/services/OperationLogService.cpp
    application/services/OperationLogService.h
    application/services/RuntimeEventConsumers.cpp
    application/services/RuntimeEventConsumers.h
    application/services/TagDefinitionCatalog.cpp
    application/services/TagDefinitionCatalog.h
    application/services/TagService.cpp
    application/services/TagService.h
    application/services/UiSnapshotProvider.cpp
    application/services/UiSnapshotProvider.h
    application/workers/BatchPersistWorker.h
)

set(INFRASTRUCTURE_SOURCES
    infrastructure/InfrastructureLayer.cpp
    infrastructure/InfrastructureLayer.h
    infrastructure/persistence/HistoryRetentionService.cpp
    infrastructure/persistence/HistoryRetentionService.h
    infrastructure/persistence/PersistenceModels.cpp
    infrastructure/persistence/PersistenceModels.h
    infrastructure/persistence/SQLiteAlarmRepository.cpp
    infrastructure/persistence/SQLiteAlarmRepository.h
    infrastructure/persistence/SQLiteConfigurationRepository.cpp
    infrastructure/persistence/SQLiteConfigurationRepository.h
    infrastructure/persistence/SQLiteHistoryRepository.cpp
    infrastructure/persistence/SQLiteHistoryRepository.h
    infrastructure/persistence/SQLiteOperationLogRepository.cpp
    infrastructure/persistence/SQLiteOperationLogRepository.h
    infrastructure/persistence/SqliteConnectionFactory.cpp
    infrastructure/persistence/SqliteConnectionFactory.h
    infrastructure/persistence/SqliteSchemaMigrations.cpp
    infrastructure/persistence/SqliteSchemaMigrations.h
)

set(SIMULATOR_SOURCES
    simulator/adapters/SimulatorDataSource.cpp
    simulator/adapters/SimulatorDataSource.h
    simulator/generators/ChannelValueGenerator.cpp
    simulator/generators/ChannelValueGenerator.h
    simulator/generators/DeviceStatusGenerator.cpp
    simulator/generators/DeviceStatusGenerator.h
    simulator/generators/FakeDataGenerator.cpp
    simulator/generators/FakeDataGenerator.h
    simulator/generators/MatrixValueGenerator.cpp
    simulator/generators/MatrixValueGenerator.h
    simulator/models/SimulationModels.cpp
    simulator/models/SimulationModels.h
    simulator/noise/NoiseModels.cpp
    simulator/noise/NoiseModels.h
    simulator/profiles/DefaultInstrumentProfile.cpp
    simulator/profiles/DefaultInstrumentProfile.h
    simulator/profiles/SimulationProfile.cpp
    simulator/profiles/SimulationProfile.h
    simulator/scenarios/SimulationScenario.cpp
    simulator/scenarios/SimulationScenario.h
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
    presentation/export/CsvExportWriter.cpp
    presentation/export/CsvExportWriter.h
    presentation/pages/MonitoringPages.cpp
    presentation/pages/MonitoringPages.h
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

add_executable(MonitorCoreTests
    tests/MonitorCoreTests.cpp
)
monitor_apply_target_defaults(MonitorCoreTests)
target_link_libraries(MonitorCoreTests PRIVATE
    MonitorDomain
    MonitorApplication
    MonitorInfrastructure
    MonitorSimulator
    MonitorPresentation
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)
add_test(NAME MonitorCoreTests COMMAND MonitorCoreTests)
