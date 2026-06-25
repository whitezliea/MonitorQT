# 阶段 1：Qt 工程和组合根基础

执行日期：2026-06-25  
源执行文档：`WPF项目转换为Qt6_Cpp项目执行文档_2026-06-25_12-54-36.md`  
目标项目：当前 `MonitorQT`，Qt6 / C++ / CMake

## 1. 阶段 1 完成范围

- 已将 CMake 从单一可执行目标拆为五个静态库目标：
  - `MonitorDomain`
  - `MonitorApplication`
  - `MonitorInfrastructure`
  - `MonitorSimulator`
  - `MonitorPresentation`
- 已补充 Qt 模块依赖：`Core`、`Widgets`、`Sql`、`LinguistTools`。
- 已建立 Qt 版组合根骨架：
  - `bootstrap/RuntimeComposition.*`
  - `bootstrap/RuntimeCompositionDependencies.*`
  - `bootstrap/EventRegistration.*`
- 已建立 Application 层事件总线骨架：
  - `application/events/EventBus.*`
- 已保留现有 `QMainWindow + QStackedWidget + shell/nav/pages` UI 壳，七个页面仍通过 `PagePlaceholderWidget` 注册和导航。
- `main.cpp` 已接入 `RuntimeComposition::initialize()`，启动时同时验证阶段 0 行为冻结合同和阶段 1 组合根。

## 2. CMake 目标关系

```text
MonitorDomain
  <- MonitorApplication
      <- MonitorInfrastructure
      <- MonitorSimulator
      <- MonitorPresentation
          <- MonitorQT executable
```

| 目标 | 当前源码 | 依赖 |
| --- | --- | --- |
| `MonitorDomain` | `domain/DomainLayer.*`、`phase0/SourceBehaviorFreeze.*` | `Qt::Core` |
| `MonitorApplication` | `application/ApplicationLayer.*`、`application/events/EventBus.*` | `MonitorDomain`、`Qt::Core` |
| `MonitorInfrastructure` | `infrastructure/InfrastructureLayer.*` | `MonitorApplication`、`Qt::Sql` |
| `MonitorSimulator` | `simulator/SimulatorLayer.*` | `MonitorApplication` |
| `MonitorPresentation` | `mainwindow.*`、`navigation/`、`pages/`、`shell/`、`presentation/PresentationLayer.*` | `MonitorApplication`、`Qt::Widgets` |
| `MonitorQT` | `main.cpp`、`bootstrap/` | 五个分层目标、`Qt::Core`、`Qt::Widgets`、`Qt::Sql` |

## 3. 组合根边界

`RuntimeComposition` 当前只负责：

- 校验阶段 0 源行为冻结合同。
- 校验默认依赖边界：运行参数、默认设备 ID、SQLite driver 名称。
- 收集五个分层目标名称，确认工程结构完整。
- 创建 Application 层 `EventBus`。
- 调用 `EventRegistration` 注册源项目等价的事件订阅描述。

当前不负责：

- 不初始化 SQLite。
- 不启动采集线程。
- 不生成模拟帧。
- 不执行 DataCleanPipeline、AlarmService 或任何业务算法。

## 4. 事件注册冻结

| 顺序 | Event | Consumer | FailurePolicy |
| ---: | --- | --- | --- |
| 10 | `RawFrameReceivedEvent` | `MeasurementMapFrameConsumer` | `Isolated` |
| 20 | `TagRuntimeStatesProducedEvent` | `TagCacheConsumer` | `Critical` |
| 30 | `DataSourceTimedOutEvent` | `DataSourceHealthOperationLogConsumer` | `Isolated` |
| 40 | `DataSourceRecoveredEvent` | `DataSourceHealthOperationLogConsumer` | `Isolated` |
| 50 | `TagRuntimeStatesProducedEvent` | `HistoryRuntimeStateConsumer` | `Isolated` |
| 60 | `AlarmRaisedEvent` | `AlarmEventConsumer` | `Isolated` |
| 70 | `AlarmUpdatedEvent` | `AlarmEventConsumer` | `Isolated` |
| 80 | `AlarmRecoveredEvent` | `AlarmEventConsumer` | `Isolated` |
| 90 | `AlarmAcknowledgedEvent` | `AlarmEventConsumer` | `Isolated` |
| 100 | `AlarmRaisedEvent` | `AlarmOperationLogConsumer` | `Isolated` |
| 110 | `AlarmUpdatedEvent` | `AlarmOperationLogConsumer` | `Isolated` |
| 120 | `AlarmRecoveredEvent` | `AlarmOperationLogConsumer` | `Isolated` |
| 130 | `AlarmAcknowledgedEvent` | `AlarmOperationLogConsumer` | `Isolated` |

## 5. 验证结果

```powershell
cmake --build build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug --config Debug
```

结果：通过，生成 `MonitorDomain`、`MonitorApplication`、`MonitorInfrastructure`、`MonitorSimulator`、`MonitorPresentation` 和 `MonitorQT.exe`。

启动验证：

- 补充 Qt 运行库路径后启动 `MonitorQT.exe`。
- 程序进入事件循环。
- `RuntimeComposition::initialize()` 未返回错误。

## 6. 下一阶段承接点

- 阶段 2 可直接在 `domain/` 下迁移枚举、值对象和规则。
- 阶段 3 可将阶段 0 的 Tag/Runtime 冻结合同拆入正式 `application/configuration` 和 `application/services`。
- 阶段 6 可将当前 `EventBus` 的订阅描述升级为实际强类型事件发布和 handler 调度。
