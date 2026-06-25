# 阶段 8：Qt 表现层和页面迁移

执行日期：2026-06-25

## 完成范围

- 新增 `application/services/UiSnapshotProvider.*`，为 Qt 表现层提供统一 `UiSnapshot`，聚合 Shell、Dashboard、Tag、Trend、Alarm、History、Measurement Map、Logs & Settings 所需数据。
- 扩展 `application/dto/ApplicationDtos.h`，新增 `ShellSnapshot` 和 `UiSnapshot`，让页面只消费 Application DTO，不直接读取设备、模拟器或 SQLite 实时表。
- 新增 `presentation/pages/MonitoringPages.*`，完成七个 QWidget 页面：
  - `DashboardPageWidget`：指标卡、关键 Tag、活跃警报、矩阵预览。
  - `RealtimeTagsPageWidget`：Tag 表格、文本筛选、分类筛选。
  - `TrendPageWidget`：实时曲线、阈值线、质量点、尖峰诊断、样本明细。
  - `AlarmCenterPageWidget`：当前警报、历史筛选、选中警报确认。
  - `HistoryPageWidget`：Tag 查询、条数限制、CSV 导出。
  - `MeasurementMapPageWidget`：16 x 16 热力图、量程、统计、异常点、单元格选择。
  - `LogsSettingsPageWidget`：操作日志、运行参数保存、Tag 阈值和历史策略保存。
- 扩展 `mainwindow.*`，用真实页面替换七个 `PagePlaceholderWidget` 注册项，并通过 `QTimer` 周期调用 `UiSnapshotProvider::refresh()`。
- 扩展 Shell 状态栏，显示数据源状态、DB 状态、最后帧号、矩阵帧号、同步状态、刷新间隔和当前时间。
- 将 Start/Stop、Navigate、Acknowledge、Query、Export、Save 映射为 Qt signal/slot；CSV 导出使用 `QFileDialog`，导出和保存结果使用 `QMessageBox`。
- 更新 `CMakeLists.txt`，将新增 Application 服务和 Presentation 页面纳入构建目标。

## 行为对齐

- MainWindow 保留 Shell 控制职责：启动、停止、导航、周期刷新和页面快照分发。
- 页面刷新统一采用 `refresh(snapshot)`，页面内部只保留筛选、选中项、表格状态等 UI 状态。
- UI 层依赖扫描无命中：`presentation/`、`shell/`、`navigation/`、`mainwindow.*` 未直接引用模拟器、QtSql、SQLite 或 `infrastructure/persistence`。
- `UiSnapshotProvider` 当前作为阶段 8 的表现层聚合应用服务，复用 Domain/Application 数据清洗、Tag 状态、警报评估、Dashboard、趋势缓存和矩阵分析逻辑。
- Logs & Settings 的运行参数保存会立即同步到 `UiSnapshotProvider`、主窗口刷新定时器和底部状态栏；Tag 配置保存会更新警报评估配置。

## 验收结果

- `cmake --build build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug --target all` 通过。
- 带 Qt 运行环境的短启动烟测通过：`MonitorQT.exe` 保持运行 5 秒后由脚本关闭。
- 依赖边界扫描通过：UI 未直接读取设备或 SQLite 实时表，数据来源保持为 Application DTO/`UiSnapshotProvider`。
- 七个页面可注册、可导航、可刷新，核心交互链路已接入。

## 后续衔接

- 阶段 9 可继续强化趋势图、热力图和导出格式，例如独立图表交互、更多 CSV 字段、图表/矩阵导出。
- 后续若要让 Logs & Settings 保存跨进程持久化，应先在 Application 层抽象配置/日志用例，再由 Infrastructure 仓储实现注入，避免表现层直接依赖 SQLite 仓储实现。
