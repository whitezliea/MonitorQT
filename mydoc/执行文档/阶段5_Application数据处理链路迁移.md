# 阶段 5：Application 数据处理链路迁移

执行日期：2026-06-25

## 完成范围

- 新增 `application/pipelines/DataCleanPipeline.*`，迁移 RawFrame 到 CleanedTagValue/TagValue/TagRuntimeState 的核心链路。
- 新增 `application/services/AlarmService.*`，迁移质量报警、阈值报警、触发、更新、确认和恢复生命周期。
- 新增 `application/caches/TagCache.*`、`MatrixFrameCache.*`，支持实时 Tag 快照、趋势缓冲和最新矩阵帧缓存。
- 新增 `application/services/TagService.*`、`ChartDataService.*`、`DashboardService.*`、`MeasurementMapService.*`。
- 新增 `application/services/MeasurementMapAnalysis.*` 和 `application/dto/ApplicationDtos.h`，提供矩阵异常点、质量状态、热力图 cell、趋势和 Dashboard 快照 DTO。
- 更新 `CMakeLists.txt`，将阶段 5 文件纳入 `MonitorApplication` 静态库。
- 扩展 `ApplicationLayer::validateApplicationLayer()`，加入阶段 5 运行期自检。

## 行为对齐

- 默认 22 个 Tag 的清洗输出保持一致：帧字段、6 个通道、功率、负载率、7 个矩阵统计。
- 帧间隔和丢帧数按同一设备上一帧计算；首帧均为 0。
- 通道缺失、设备离线、帧错误码和越界质量按源端 `DataCleanPipeline` 规则传播。
- 功率 = 电压 x 电流；负载率 = 电流 / 5.0 x 100。
- 矩阵平均、最大、最小、均匀性、异常点数量、热点行列从同一矩阵帧生成。
- 报警状态顺序保持：AlarmHigh/AlarmLow 优先于 WarningHigh/WarningLow；Offline 和非 Good 质量进入质量类报警。
- 当前报警支持 Active、Acknowledged、Recovered；同一报警仅在等级/类型变化、5 秒间隔或显著数值变化时更新。
- Tag 缓存只缓存数值型趋势点，并按容量裁剪。
- Dashboard、Trend、MeasurementMap 均从 Application 快照或缓存读取，不依赖 UI、SQLite 或模拟器内部。

## 验收结果

- `cmake --build build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug --config Debug` 通过。
- 启动烟测通过：`MonitorQT.exe` 能完成组合根初始化并保持运行，随后由脚本关闭。
- Application 依赖扫描通过：新增代码未引用 QtWidgets、QtSql 或 simulator 内部。
- 修复 `TagRuntimeConfigurationStore` 成员初始化顺序问题，默认配置快照 revision 稳定为 0，避免组合根初始化偶发失败。

## 后续衔接

- 阶段 6 可在当前服务上接入 EventBus、线程安全队列、MonitoringRuntimeService 和后台 Worker。
- 阶段 8/9 可直接消费 `DashboardSnapshot`、`TrendSeries`、`MeasurementMapSnapshot` 和 `MatrixPreview`。
