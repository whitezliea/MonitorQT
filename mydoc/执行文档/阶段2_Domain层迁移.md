# 阶段 2：Domain 层迁移

执行日期：2026-06-25  
源执行文档：`WPF项目转换为Qt6_Cpp项目执行文档_2026-06-25_12-54-36.md`  
目标项目：当前 `MonitorQT`，Qt6 / C++ / CMake

## 1. 完成范围

已在 `MonitorDomain` 目标中迁移源项目 `MyProject/Domain` 的稳定领域模型和纯规则：

- `domain/common/DomainCommon.*`
- `domain/devices/DeviceModels.*`
- `domain/tags/TagModels.*`
- `domain/measurements/MeasurementModels.*`
- `domain/alarms/AlarmModels.*`
- `domain/logs/LogModels.*`
- `domain/tasks/TaskModels.*`
- `domain/rules/MatrixStatisticsCalculator.*`
- `domain/rules/DomainRules.*`

Domain 层只使用 QtCore 值类型和 STL，不依赖 `QObject`、`QWidget`、SQLite、模拟器或 UI。

## 2. 已迁移对象

| C# Domain | Qt/C++ Domain |
| --- | --- |
| `DeviceStatus`、`DeviceConnectionState`、`DeviceErrorCode` | `enum class` |
| `TagCategory`、`TagQuality`、`TagAlarmState`、`TagDataType`、`TagValueKind`、`SourceType` | `enum class` |
| `RawMeasurementFrame`、`ChannelValue`、`MatrixFrame`、`MatrixStatistics` | 值语义 `struct` |
| `TagDefinition`、`TagSourceMapping`、`CleanedTagValue`、`TagRuntimeState`、`TagValue`、`TagSnapshot` | 值语义 `struct` |
| `AlarmDefinition`、`AlarmEvent`、`ActiveAlarm`、`AlarmLevel`、`AlarmState` | 值语义 `struct` / `enum class` |
| `OperationLog`、`OperationLogQuery`、日志等级和分类 | 值语义 `struct` / `enum class` |
| `MeasurementTask`、`MeasurementTaskSummary` | 值语义 `struct` |
| `UtcDateTime`、`TimeRange`、`Result`、`DomainException` | 领域公共工具 |

## 3. 已迁移规则

| 规则 | 对齐行为 |
| --- | --- |
| `MatrixStatisticsCalculator` | 忽略 NaN/Infinity，使用 Welford 算法计算均值和标准差，保持 `Uniformity == Min/Max` |
| `MatrixStatisticsRule` | 委托 `MatrixFrame::calculateStatistics()` |
| `QualityRule` | `Offline -> TagQuality::Offline`，非零错误码 -> `DeviceError`，否则 `Good` |
| `TagValidationRule` | 超出 `MinValue/MaxValue` 返回 `OutOfRange` |
| `AlarmRule` | 保持 Offline、Invalid、AlarmHigh/Low、WarningHigh/Low、Normal 顺序 |
| `MeasurementTimeContract` | RawFrame 和 MatrixFrame 必须使用 UTC，矩阵时间必须等于源 RawFrame 时间 |

## 4. 启动自检

`DomainLayer::validateDomainLayer()` 已接入 `RuntimeComposition::initialize()`，启动时验证：

- 2 x 2 矩阵统计结果与 C# 测试一致。
- 非有限矩阵值被忽略，全无效时返回 NaN 统计。
- `QualityRule`、`TagValidationRule`、`AlarmRule` 关键分支一致。
- `MeasurementTimeContract` 接受 UTC RawFrame，并拒绝矩阵时间与 RawFrame 时间不一致。

## 5. 验证结果

```powershell
cmake --build build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug --config Debug
```

结果：通过。

启动验证：

- 补充 Qt 运行库路径后启动 `MonitorQT.exe`。
- 程序进入事件循环。
- 阶段 0、阶段 1、阶段 2 自检均通过。

依赖扫描：

```powershell
rg "QObject|QWidget|QSql|QtWidgets|QtSql" domain
```

结果：仅命中 `DomainLayer` 中声明禁止依赖的字符串，没有实际 include 或类型依赖。

## 6. 下一阶段承接点

- 阶段 3 可将 `phase0/SourceBehaviorFreeze.*` 中冻结的默认 Tag 字典和运行参数迁入正式 `application/configuration` 与 `application/services`。
- 阶段 5 的 `DataCleanPipeline` 可直接消费本阶段的 `RawMeasurementFrame`、`TagDefinition`、`CleanedTagValue` 和矩阵统计规则。
