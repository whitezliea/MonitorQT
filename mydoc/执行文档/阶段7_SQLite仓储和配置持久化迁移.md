# 阶段 7：SQLite、仓储和配置持久化迁移

执行日期：2026-06-25

## 完成范围

- 新增 `infrastructure/persistence/SqliteConnectionFactory.*`，使用 QtSql `QSQLITE` 驱动创建每次调用独立的连接名，并启用 WAL、`synchronous=NORMAL`、`busy_timeout=5000`、只读连接 `query_only=ON`。
- 新增 `SqliteSchemaMigrations.*`，按 C# 当前版本迁移到 schema version 5，创建 `history_samples`、`alarm_events`、`operation_logs`、`tag_runtime_settings`、`runtime_settings` 和 `schema_migrations`。
- 新增 `PersistenceModels.*`，迁移历史分页查询、警报分页查询和历史保留结果模型，统一 UTC 时间范围、分页范围和最大页大小校验。
- 新增 `SQLiteHistoryRepository.*`，支持历史样本批量事务写入、`INSERT OR IGNORE` 去重、按 Tag/时间范围分页查询和按批次删除保留期外数据。
- 新增 `SQLiteAlarmRepository.*`，支持警报生命周期 upsert、最新警报、当前未关闭警报和按时间/Tag/等级/状态分页查询。
- 新增 `SQLiteOperationLogRepository.*`，支持操作日志批量事务写入、最新日志查询、时间范围和等级/分类过滤。
- 新增 `SQLiteConfigurationRepository.*`，支持 Tag 运行配置和 runtime settings 的加载与 upsert 保存。
- 新增 `HistoryRetentionService.*`，按保留天数分批删除历史样本，并写入 `History.RetentionCleanup` 操作日志。
- 扩展 `RuntimeComposition`，创建默认 SQLite 连接工厂、历史/警报/日志/配置仓储和历史保留服务。
- 扩展 `main.cpp` 启动前置校验，首次运行会创建默认 `data/multichannel-monitor.db`，并校验 schema version 为当前版本。

## 行为对齐

- 默认数据库路径保持为可执行程序目录下的 `data/multichannel-monitor.db`。
- schema version 保持为 5，迁移历史记录写入 `schema_migrations`。
- 历史样本继续使用兼容 C# 的 UTC ticks 存储，历史查询按 `timestamp_utc_ticks` 和 `id` 稳定排序。
- 警报事件按 `alarm_id` upsert，保留 acknowledge/recover/lastUpdated/closeReason 生命周期字段。
- 操作日志按时间倒序查询，分类过滤保持大小写不敏感。
- 配置保存采用 upsert，保存后可重新加载。
- 历史保留按批次删除，直到本批删除数小于批大小，并记录业务操作日志。

## 验收结果

- `cmake --build build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug --target all` 通过。
- 带 Qt 运行环境的短启动烟测通过：`MonitorQT.exe` 保持运行 5 秒后由脚本关闭。
- 首次启动已创建 `build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug\data\multichannel-monitor.db`。
- Infrastructure 启动自检覆盖临时库迁移、WAL、表结构、历史/警报/日志/配置仓储读写、历史保留清理和操作日志写入。
- 依赖扫描通过：QtSql 和 SQLite 具体实现仅位于 Infrastructure/Bootstrap/可执行入口，Domain/Application 未反向依赖仓储实现。

## 后续衔接

- 阶段 8 可将 UI 的 Logs & Settings 页面接入 `SQLiteConfigurationRepository` 和 `SQLiteOperationLogRepository`。
- 后续可把 `PersistenceRuntimeCoordinator` 的 worker 回调绑定到本阶段仓储，实现运行时队列到 SQLite 的真实落库。
- 如果需要命令行自动化 smoke test，可后续单独增加稳定的控制台测试目标，而不是复用 Windows GUI 子系统输出。
