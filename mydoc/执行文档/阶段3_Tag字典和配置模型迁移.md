# 阶段 3：Tag 字典和配置模型迁移

执行日期：2026-06-25  
源执行文档：`WPF项目转换为Qt6_Cpp项目执行文档_2026-06-25_12-54-36.md`  
目标项目：当前 `MonitorQT`，Qt6 / C++ / CMake

## 1. 完成范围

已在 `MonitorApplication` 目标中迁移源项目 `MyProject/Application/Configuration` 与 `TagDefinitionCatalog` 的核心模型：

- `application/configuration/MonitorRuntimeOptions.*`
- `application/configuration/RuntimeOptionsStore.*`
- `application/configuration/TagRuntimeConfiguration.*`
- `application/configuration/ConfigurationValidation.*`
- `application/configuration/TrendDiagnosisOptions.*`
- `application/services/TagDefinitionCatalog.*`

`MainWindow` 的默认 UI 刷新周期和设备 ID 已切换为正式 Application 配置模型和 `TagDefinitionCatalog`，阶段 0 合同只保留在组合根中作为回归校验。

## 2. 默认运行参数

| 参数 | Qt/C++ 默认值 |
| --- | ---: |
| `DataGenerateIntervalMs` | 500 |
| `DataSourceTimeoutPeriods` | 3 |
| `DataSourceTimeoutMs` | 1500 |
| `UiRefreshIntervalMs` | 1000 |
| `HistoryBatchIntervalMs` | 5000 |
| `HistoryMaxBatchSize` | 100 |
| `HistoryRetentionDays` | 30 |
| `HistoryRetentionDeleteBatchSize` | 1000 |
| `AlarmBatchIntervalMs` | 5000 |
| `AlarmMaxBatchSize` | 100 |
| `OperationLogBatchIntervalMs` | 5000 |
| `OperationLogMaxBatchSize` | 100 |
| `TrendWindows` | 1 / 5 / 30 分钟 |
| `TrendBufferCapacity` | 3600 点 |

## 3. Tag 字典

`TagDefinitionCatalog::createDefaults()` 已迁移 22 个默认 Tag：

- 7 个设备/运行时 Tag：`DEVICE.*`
- 6 个原始通道 Tag：`MEAS.TEMP/PRESSURE/LIGHT/VOLTAGE/CURRENT/VIBRATION.CH01`
- 2 个派生 Tag：`MEAS.POWER.CH01`、`MEAS.LOAD_RATIO.CH01`
- 7 个矩阵统计 Tag：`MATRIX.LIGHT.*`

`TagDefinitionCatalog::createSourceMappings()` 已迁移 22 个来源映射：

- `FrameField`：设备状态、在线、错误码、质量、序号
- `Runtime`：帧间隔、连续丢帧数
- `Channel`：6 个原始通道编码到业务 TagId
- `Derived`：功率、负载率
- `Matrix`：平均、最大、最小、均匀性、异常点数量、热点坐标

## 4. 配置 Store 和校验

`RuntimeOptionsStore`：

- 使用 mutex 保护值快照。
- `snapshot()` 返回值副本。
- `replace()` 原子替换当前运行参数。

`TagRuntimeConfigurationStore`：

- 使用 mutex 保护 Tag 配置快照。
- 默认快照 revision 为 0。
- 每次 `replace()` 统一递增 revision，后续历史采样可用 revision 重置采样基线。

`ConfigurationValidation`：

- 校验 TagId 和 definition 匹配。
- 校验阈值必须为有限数。
- 非数值 Tag 不允许配置数值阈值。
- 校验阈值必须在工程范围内。
- 校验 `AlarmLow <= WarningLow < WarningHigh <= AlarmHigh`。
- 数值报警启用时至少需要一个阈值。
- `HistoryIntervalMs` 必须大于 0。
- 运行参数间隔、趋势窗口、历史保留和超时倍数按源项目规则校验。

## 5. 启动自检

`ApplicationLayer::validateApplicationLayer()` 已接入 `RuntimeComposition::initialize()`，启动时验证：

- 默认运行参数与源项目一致。
- 1/5/30 分钟趋势点数为 120/600/3600。
- 默认 Tag 数量为 22，且关键 TagId 均存在。
- 默认 Mapping 数量为 22，且 6 个原始通道映射正确。
- `RuntimeOptionsStore` 可替换快照。
- `TagRuntimeConfigurationStore` 默认 revision 和 replace revision 行为正确。
- `ConfigurationValidation` 接受 `MEAS.TEMP.CH01` 默认配置，并拒绝错误阈值顺序。
- `TrendDiagnosisOptions` 默认值通过校验。

## 6. 验证结果

```powershell
cmake --build build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug --config Debug
```

结果：通过。

启动验证：

- 补充 Qt 运行库路径后启动 `MonitorQT.exe`。
- 程序进入事件循环。
- 阶段 0/1/2/3 自检均通过。

依赖扫描：

```powershell
rg "QWidget|QtWidgets|QSql|QtSql|Simulator" application
```

结果：仅命中 `ApplicationLayer` 中声明禁止依赖的说明字符串，没有实际 include 或类型依赖。

## 7. 下一阶段承接点

- 阶段 4 的模拟器可直接读取 `MonitorRuntimeOptions::dataGenerateIntervalMs` 和 `TagDefinitionCatalog::defaultDeviceId()`。
- 阶段 5 的 `DataCleanPipeline` 可直接使用正式 `TagDefinition`、`TagSourceMapping` 和 `TagRuntimeConfigurationStore`。
- 阶段 6 的历史采样消费者可使用 `TagRuntimeConfiguration::revision` 实现配置变更后的采样基线重置。
