# MultiChannel Monitor Qt6 Widgets UI 软件设计书

版本：Qt6 / C++ / Qt Widgets 桌面版
目标：将原 WPF 上位机监控 UI 迁移为 Qt6 Widgets 桌面应用
核心迁移思想：从 WPF 的 MVVM / Binding / Renderer，迁移到 Qt 的 QObject / Signal-Slot / Model-View / paintEvent 绘制模型。

---

# 1. 设计目标

本设计书面向一个仪器监控型桌面应用，不是传统表单管理系统。

原 WPF 项目已经形成完整页面闭环：

```text
Dashboard
Realtime Tags
Trend
Alarm Center
History
Measurement Map
Logs & Settings
```

Qt6 版本的目标不是简单复刻 XAML，而是保留原软件的业务页面和交互关系，同时按照 Qt Widgets 的方式重新组织 UI 框架、组件、绘图、表格模型和刷新机制。

Qt6 版本核心目标：

```text
1. 保留原 WPF 的上位机业务页面闭环
2. 使用 Qt6 Widgets 构建桌面 Shell 和页面系统
3. 使用 QStackedWidget 替代 WPF ContentControl + DataTemplate 页面切换
4. 使用 QObject + signal/slot 替代 ObservableProperty + Binding
5. 使用 QAbstractTableModel + QTableView 替代 ObservableCollection + DataGrid
6. 使用 QWidget + paintEvent + QPainter 替代 ScottPlot Renderer / ItemsControl 热力图
7. 保留 UI 快照思想，避免 UI 直接消费原始设备帧
8. 后台线程不直接操作 UI，通过信号槽回到主线程
```

一句话总结：

```text
WPF 版本：
Application 快照 -> ViewModel -> Binding -> XAML / Renderer

Qt6 Widgets 版本：
Application 快照 -> Presenter / Controller -> QObject 状态 / TableModel -> QWidget.update() -> paintEvent / QPainter
```

---

# 2. Qt6 UI 心智模型

## 2.1 从 WPF 到 Qt 的核心变化

WPF 的核心是：

```text
属性变化
  -> PropertyChanged
  -> Binding 自动更新控件属性
  -> WPF 渲染视觉树
```

Qt Widgets 的核心是：

```text
状态变化
  -> emit signal
  -> slot 更新对象状态
  -> QWidget::update()
  -> Qt 事件循环安排重绘
  -> paintEvent()
  -> QPainter 根据当前状态完整重画
```

所以 Qt6 版本的图像绘制心智模型是：

```text
保存状态，不保存画面；
请求重绘，不直接乱画；
paintEvent 根据当前状态完整重建画面。
```

错误思路：

```cpp
void onDataArrived()
{
    QPainter painter(this);
    painter.drawLine(...);   // 错误：不要在数据回调里直接画
}
```

正确思路：

```cpp
void TrendChartWidget::setSnapshot(const TrendSnapshot& snapshot)
{
    m_snapshot = snapshot;
    update();                 // 请求重绘
}

void TrendChartWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    drawBackground(painter);
    drawAxes(painter);
    drawCurve(painter);
    drawThresholdLines(painter);
    drawCurrentPoint(painter);
}
```

## 2.2 Qt 绘制闭环

```text
UiCompositeSnapshot 到达
  -> ShellController 接收
  -> 分发给各 PagePresenter
  -> PagePresenter 更新页面状态
  -> TableModel 发 dataChanged / beginResetModel / endResetModel
  -> CustomWidget 保存 snapshot 并 update()
  -> paintEvent 使用 QPainter 绘制当前状态
```

## 2.3 Qt 中的三类 UI 表达方式

```text
普通控件：
  QLabel / QPushButton / QComboBox / QCheckBox / QLineEdit / QDateTimeEdit

表格控件：
  QTableView + QAbstractTableModel + QStyledItemDelegate

复杂图像：
  QWidget + paintEvent + QPainter
  或 QGraphicsView + QGraphicsScene + QGraphicsItem
```

本项目建议第一版采用：

```text
趋势图：QWidget + QPainter
热力图：QWidget + QPainter
实时表格：QTableView + QAbstractTableModel
报警表格：QTableView + QAbstractTableModel
历史表格：QTableView + QAbstractTableModel
日志表格：QTableView + QAbstractTableModel
主页面切换：QStackedWidget
```

---

# 3. 总体 UI 架构

## 3.1 Qt6 版本总体结构

```text
MultiChannelMonitor.Qt
  ├── Application / Runtime
  │     后台数据采集、清洗、报警、矩阵分析、历史写入、UI 快照生成
  │
  ├── Presentation.Qt
  │     Qt6 Widgets 桌面 UI
  │
  └── Shared / Domain
        DTO、枚举、业务模型、Tag 定义、报警状态、矩阵快照
```

Qt UI 层内部结构：

```text
Presentation.Qt
  ├── App
  │   ├── main.cpp
  │   ├── QtAppBootstrapper
  │   └── RuntimeComposition
  │
  ├── Shell
  │   ├── MainWindow
  │   ├── ShellController
  │   ├── SideNavigationWidget
  │   ├── TopStatusBarWidget
  │   └── BottomStatusBarWidget
  │
  ├── Pages
  │   ├── Dashboard
  │   ├── RealtimeTags
  │   ├── Trend
  │   ├── AlarmCenter
  │   ├── History
  │   ├── MeasurementMap
  │   └── LogsSettings
  │
  ├── Models
  │   ├── RealtimeTagsTableModel
  │   ├── AlarmTableModel
  │   ├── HistorySamplesTableModel
  │   ├── OperationLogsTableModel
  │   ├── ThresholdSettingsTableModel
  │   └── AbnormalPointsTableModel
  │
  ├── Widgets
  │   ├── Charts
  │   │   ├── TrendChartWidget
  │   │   ├── MiniTrendChartWidget
  │   │   └── ChartAxisMapper
  │   │
  │   ├── Heatmap
  │   │   ├── HeatmapWidget
  │   │   ├── MiniHeatmapWidget
  │   │   ├── HeatmapColorMapper
  │   │   └── HeatmapHitTester
  │   │
  │   ├── Common
  │   │   ├── MetricCardWidget
  │   │   ├── StatusBadgeWidget
  │   │   ├── AlarmBadgeWidget
  │   │   ├── QualityBadgeWidget
  │   │   ├── SectionPanelWidget
  │   │   └── PageToolbarWidget
  │   │
  │   └── Delegates
  │       ├── AlarmStateDelegate
  │       ├── QualityStateDelegate
  │       ├── NumericValueDelegate
  │       └── ThresholdEditDelegate
  │
  ├── Navigation
  │   ├── NavigationService
  │   ├── NavigationPage
  │   ├── NavigationItem
  │   └── NavigationParameter
  │
  ├── Presenters
  │   ├── DashboardPresenter
  │   ├── RealtimeTagsPresenter
  │   ├── TrendPresenter
  │   ├── AlarmCenterPresenter
  │   ├── HistoryPresenter
  │   ├── MeasurementMapPresenter
  │   └── LogsSettingsPresenter
  │
  ├── Services
  │   ├── UiRefreshTimerService
  │   ├── UiThreadDispatcher
  │   ├── DialogService
  │   ├── FilePickerService
  │   ├── ToastService
  │   └── ExportServiceAdapter
  │
  ├── Styles
  │   ├── app.qss
  │   ├── colors.qss
  │   ├── tables.qss
  │   ├── buttons.qss
  │   └── cards.qss
  │
  └── Resources
      ├── icons.qrc
      └── fonts.qrc
```

---

# 4. WPF 到 Qt 的模块映射

```text
WPF MainWindow
  -> Qt QMainWindow / MainWindow

WPF ContentControl + DataTemplate
  -> Qt QStackedWidget + NavigationService

WPF ShellViewModel
  -> Qt ShellController / MainWindowController

WPF PageViewModel
  -> Qt PagePresenter / PageController

WPF ObservableProperty
  -> Qt QObject 属性 + notify signal

WPF RelayCommand
  -> Qt slot / QAction / QPushButton clicked connection

WPF DataGrid
  -> Qt QTableView + QAbstractTableModel

WPF ItemsControl + UniformGrid 指标卡
  -> Qt QGridLayout + MetricCardWidget

WPF ScottPlot.WPF 趋势图
  -> Qt TrendChartWidget : QWidget + QPainter

WPF ItemsControl + UniformGrid 热力图
  -> Qt HeatmapWidget : QWidget + QPainter

WPF TabControl
  -> Qt QTabWidget

WPF ComboBox / TextBox / CheckBox / DatePicker
  -> Qt QComboBox / QLineEdit / QCheckBox / QDateTimeEdit

WPF Converter
  -> Qt helper function / QStyledItemDelegate / widget style helper

WPF DispatcherTimer
  -> Qt QTimer

WPF Dispatcher.Invoke
  -> Qt queued signal-slot / QMetaObject::invokeMethod
```

---

# 5. Shell 主窗口设计

## 5.1 UX 草图

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ MultiChannel Monitor                         Device: MCMD-001   ● Running   │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ┌────────────────────┐  ┌─────────────────────────────────────────────────┐ │
│ │                    │  │                                                 │ │
│ │  [Logo / Name]     │  │  Page Title                                      │ │
│ │                    │  │  ─────────────────────────────────────────────   │ │
│ │  ● Dashboard       │  │                                                 │ │
│ │  ○ Realtime Tags   │  │                                                 │ │
│ │  ○ Trend           │  │                                                 │ │
│ │  ○ Alarm Center    │  │              Current Page Content                │ │
│ │  ○ History         │  │                                                 │ │
│ │  ○ Matrix Map      │  │                                                 │ │
│ │  ○ Logs/Settings   │  │                                                 │ │
│ │                    │  │                                                 │ │
│ │                    │  │                                                 │ │
│ │  Start  Stop       │  │                                                 │ │
│ │                    │  │                                                 │ │
│ └────────────────────┘  └─────────────────────────────────────────────────┘ │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│ DataSource: Connected | Last Frame: 1024 | UI Refresh: 1s | 2026-06-07 10:30 │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 5.2 Qt 组件选择

```text
MainWindow:
  QMainWindow

主布局:
  QWidget + QVBoxLayout

顶部状态栏:
  QWidget + QHBoxLayout + QLabel + StatusBadgeWidget

中间区域:
  QSplitter 或 QWidget + QHBoxLayout

左侧导航:
  QListWidget / QToolButton 列表 / 自定义 SideNavigationWidget

右侧页面容器:
  QStackedWidget

底部状态栏:
  QStatusBar 或自定义 BottomStatusBarWidget

Start / Stop:
  QPushButton

页面标题:
  QLabel
```

## 5.3 类职责

```text
MainWindow
  只负责 Qt 控件组合和布局

ShellController
  负责页面注册、导航、启动停止、状态刷新、UiCompositeSnapshot 分发

NavigationService
  根据 NavigationPage 切换 QStackedWidget 当前页

UiRefreshTimerService
  内部使用 QTimer，每 1s 从 UiSnapshotProvider 拉取快照

TopStatusBarWidget
  显示设备状态、运行状态、采集状态

BottomStatusBarWidget
  显示 DataSource、LastFrame、UI Refresh、当前时间
```

## 5.4 Shell 数据刷新闭环

```text
QTimer timeout
  -> ShellController::refreshSnapshot()
  -> UiSnapshotProvider::getSnapshot(...)
  -> DashboardPresenter::applySnapshot(...)
  -> RealtimeTagsPresenter::applySnapshot(...)
  -> TrendPresenter::applySnapshot(...)
  -> AlarmCenterPresenter::applySnapshot(...)
  -> MeasurementMapPresenter::applySnapshot(...)
  -> MainWindow 状态栏刷新
```

---

# 6. Dashboard 页面设计

Dashboard 是总览页，负责快速回答：

```text
设备是否正常？
关键指标是多少？
有没有报警？
最近趋势是否稳定？
矩阵测量有没有异常？
```

## 6.1 UX 草图

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Dashboard                                                                    │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐          │
│ │ Device       │ │ Active Alarm │ │ Sample Rate  │ │ Data Quality │          │
│ │ ● Running    │ │ 3 Active     │ │ 500 ms       │ │ 98.5% Good   │          │
│ └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘          │
│                                                                              │
│ ┌───────────────────────────────┐ ┌──────────────────────────────────────┐   │
│ │ Key Measurements              │ │ Recent Trend                         │   │
│ │                               │ │                                      │   │
│ │ Temperature   25.4 ℃    Good  │ │  TEMP_CH01                           │   │
│ │ Pressure      101.2 kPa Good  │ │  ┌────────────────────────────────┐  │   │
│ │ Light         580.6 lux Good  │ │  │       /\      /\               │  │   │
│ │ Voltage       12.1 V    Good  │ │  │  ____/  \____/  \_____         │  │   │
│ │ Current       1.2 A     Good  │ │  └────────────────────────────────┘  │   │
│ │ Vibration     0.03 mm/s Good  │ │  Window: Last 1 min                  │   │
│ └───────────────────────────────┘ └──────────────────────────────────────┘   │
│                                                                              │
│ ┌───────────────────────────────┐ ┌──────────────────────────────────────┐   │
│ │ Active Alarms                 │ │ Measurement Map Preview              │   │
│ │                               │ │                                      │   │
│ │ [High] TEMP_CH01  86.5 ℃      │ │       ░░▒▒▓▓▒▒░░                    │   │
│ │ [Warn] VOLTAGE    9.2 V       │ │       ░▒▒▓██▓▒▒░                    │   │
│ │ [Bad ] LIGHT      DeviceError │ │       ░░▒▒▓▓▒▒░░                    │   │
│ │                               │ │  Max: 890 lux  Avg: 610 lux          │   │
│ │             View All Alarms > │ │             Open Matrix Map >        │   │
│ └───────────────────────────────┘ └──────────────────────────────────────┘   │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 6.2 Qt 组件选择

```text
DashboardPage:
  QWidget

页面主布局:
  QVBoxLayout

顶部指标卡:
  QGridLayout + MetricCardWidget

关键测量:
  QTableView + KeyMeasurementsTableModel
  或 QWidget + QGridLayout + TagValueRowWidget

当前报警:
  QListWidget / QTableView + AlarmSummaryTableModel

趋势预览:
  MiniTrendChartWidget : QWidget + QPainter

矩阵预览:
  MiniHeatmapWidget : QWidget + QPainter

跳转按钮:
  QPushButton / QLabel link style
```

## 6.3 数据输入

```text
DashboardSnapshot
TrendSeriesSnapshot
MatrixPreviewSnapshot
TagDefinitionMap
AlarmSummary
DataQualitySummary
```

## 6.4 Presenter 职责

```text
DashboardPresenter
  ├── applySnapshot(snapshot)
  ├── updateMetricCards()
  ├── updateKeyMeasurementsModel()
  ├── updateActiveAlarmList()
  ├── updateMiniTrendWidget()
  ├── updateMiniHeatmapWidget()
  └── 处理 Dashboard 内部跳转信号
```

## 6.5 页面行为

```text
点击 Active Alarm 卡片
  -> NavigationService.navigateTo(AlarmCenter)

点击 Recent Trend 卡片
  -> NavigationService.navigateTo(Trend, selectedTagId)

点击 Measurement Map Preview
  -> NavigationService.navigateTo(MeasurementMap)

点击关键指标行
  -> NavigationService.navigateTo(RealtimeTags 或 Trend)
```

---

# 7. Realtime Tags 页面设计

Realtime Tags 是实时数据表页面，负责显示所有 Tag 的当前值、质量、报警状态和更新时间。

## 7.1 UX 草图

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Realtime Tags                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ Category: [All v]   Quality: [All v]   Alarm: [All v]   Search: [_______]    │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ TagId              Name          Value      Unit   Quality   Alarm      │   │
│ ├────────────────────────────────────────────────────────────────────────┤   │
│ │ TEMP_CH01          Temperature   25.4       ℃      Good      Normal     │   │
│ │ PRESSURE_CH01      Pressure      101.2      kPa    Good      Normal     │   │
│ │ LIGHT_CH01         Light         580.6      lux    Good      Normal     │   │
│ │ VOLTAGE_CH01       Voltage       12.1       V      Good      Normal     │   │
│ │ CURRENT_CH01       Current       1.2        A      Good      Normal     │   │
│ │ VIBRATION_CH01     Vibration     0.03       mm/s   Good      Normal     │   │
│ │ MATRIX_AVG_LIGHT   Matrix Avg    610.2      lux    Good      Normal     │   │
│ │ MATRIX_MAX_LIGHT   Matrix Max    890.4      lux    Good      Warning    │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ Selected Tag Detail                                                     │   │
│ │                                                                        │   │
│ │ TagId: TEMP_CH01                                                        │   │
│ │ Description: Temperature Channel 01                                      │   │
│ │ Range: -20 ~ 120 ℃                                                      │   │
│ │ WarningHigh: 60 ℃     AlarmHigh: 80 ℃                                   │   │
│ │ Last Update: 2026-06-07 10:30:01.500                                    │   │
│ │                                                                        │   │
│ │ [View Trend]   [View History]                                           │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 7.2 Qt 组件选择

```text
RealtimeTagsPage:
  QWidget

筛选区:
  QComboBox Category
  QComboBox Quality
  QComboBox Alarm
  QLineEdit Search
  QPushButton ClearFilter

表格:
  QTableView
  RealtimeTagsTableModel : QAbstractTableModel
  QualityStateDelegate
  AlarmStateDelegate
  NumericValueDelegate

详情区:
  QGroupBox / SectionPanelWidget
  QLabel
  QPushButton View Trend
  QPushButton View History
```

## 7.3 表格 Model 设计

```cpp
class RealtimeTagsTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        TagId,
        Name,
        Value,
        Unit,
        Quality,
        Alarm,
        LastUpdate,
        ColumnCount
    };

    void setRows(const std::vector<RealtimeTagRow>& rows);
    RealtimeTagRow rowAt(int row) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<RealtimeTagRow> m_rows;
};
```

## 7.4 Presenter 职责

```text
RealtimeTagsPresenter
  ├── 保存 latestTags
  ├── 处理 Category / Quality / Alarm / Search 变化
  ├── applyFilter()
  ├── 更新 RealtimeTagsTableModel
  ├── 监听 QTableView selectionModel 当前行变化
  ├── 更新 Selected Tag Detail
  ├── 处理 View Trend / View History 按钮
```

## 7.5 页面行为

```text
选择分类
  -> applyFilter()
  -> tableModel.setRows(filteredRows)

输入搜索
  -> applyFilter()
  -> tableModel.setRows(filteredRows)

点击表格行
  -> updateSelectedTagDetail()

点击 View Trend
  -> NavigationService.navigateTo(Trend, selectedTagId)

点击 View History
  -> NavigationService.navigateTo(History, selectedTagId)
```

---

# 8. Trend 趋势页面设计

Trend 页面负责显示某个 Tag 最近 1 / 5 / 30 分钟变化，显示阈值线、质量点、尖峰、当前值、统计摘要和诊断信息。

## 8.1 UX 草图

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Trend                                                                        │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ Tag: [TEMP_CH01 - Temperature v]     Window: [1 min] [5 min] [30 min]        │
│ Auto Refresh: [ON]     Current: 25.4 ℃     Quality: Good     Alarm: Normal   │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │                                                                        │   │
│ │  Temperature Trend                                                     │   │
│ │                                                                        │   │
│ │  90 ┤                                                                  │   │
│ │  80 ┤------------------------ AlarmHigh -----------------------------  │   │
│ │  70 ┤                                                                  │   │
│ │  60 ┤------------------------ WarningHigh ---------------------------  │   │
│ │  50 ┤                                                                  │   │
│ │  40 ┤                                                                  │   │
│ │  30 ┤          /\       /\                                             │   │
│ │  20 ┤_________/  \_____/  \____________________________                │   │
│ │     └────────────────────────────────────────────────────── Time       │   │
│ │                                                                        │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│ ┌────────────────────────────┐ ┌─────────────────────────────────────────┐   │
│ │ Statistics                  │ │ Related Tags                            │   │
│ │ Min: 24.8 ℃                 │ │ Voltage      12.1 V                     │   │
│ │ Max: 26.2 ℃                 │ │ Current      1.2 A                      │   │
│ │ Avg: 25.4 ℃                 │ │ Power        14.5 W                     │   │
│ │ Last: 25.4 ℃                │ │ Vibration    0.03 mm/s                  │   │
│ └────────────────────────────┘ └─────────────────────────────────────────┘   │
│                                                                              │
│ [Open History Query]   [Pause/Resume]                                        │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 8.2 Qt 组件选择

```text
TrendPage:
  QWidget

顶部工具条:
  QComboBox Tag
  QComboBox / QButtonGroup Window
  QCheckBox Auto Refresh
  QLabel Current
  QualityBadgeWidget
  AlarmBadgeWidget

趋势图:
  TrendChartWidget : QWidget + QPainter

统计区:
  QGroupBox / SectionPanelWidget + QLabel 网格

相关 Tag:
  QListView / QTableView

最近采样:
  QTableView + TrendSamplesTableModel

操作按钮:
  QPushButton Open History
  QPushButton Pause/Resume
```

## 8.3 TrendChartWidget 设计

```cpp
class TrendChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrendChartWidget(QWidget* parent = nullptr);

    void setSnapshot(const TrendSnapshot& snapshot);
    void setAutoScale(bool enabled);
    void setShowQualityPoints(bool enabled);
    void setShowThresholds(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    TrendSnapshot m_snapshot;
    QPoint m_hoverPoint;
    bool m_hasHover = false;

    QRectF plotArea() const;

    void drawBackground(QPainter& p);
    void drawTitle(QPainter& p);
    void drawAxes(QPainter& p);
    void drawGrid(QPainter& p);
    void drawThresholdLines(QPainter& p);
    void drawCurve(QPainter& p);
    void drawQualityPoints(QPainter& p);
    void drawSpikeMarkers(QPainter& p);
    void drawCurrentPoint(QPainter& p);
    void drawHoverTooltip(QPainter& p);

    QPointF mapToPlot(const TrendPoint& point) const;
};
```

## 8.4 绘图规则

```text
TrendChartWidget 不访问数据库
TrendChartWidget 不启动后台任务
TrendChartWidget 不计算报警
TrendChartWidget 只消费 TrendSnapshot
TrendChartWidget 每次 paintEvent 根据 snapshot 完整重画
```

绘制顺序：

```text
1. 背景
2. 标题和单位
3. 坐标轴
4. 网格线
5. 阈值线
6. 趋势折线
7. 非 Good 质量点
8. 尖峰点
9. 当前点
10. Hover tooltip
```

## 8.5 Presenter 职责

```text
TrendPresenter
  ├── 管理 SelectedTagId
  ├── 管理 SelectedWindow
  ├── 管理 AutoRefresh / Pause
  ├── 接收 TrendSnapshot
  ├── 更新 TrendChartWidget
  ├── 更新统计 QLabel
  ├── 更新最近采样表格
  ├── 处理 Open History
  └── 暂停时缓存 pendingSnapshot
```

暂停逻辑：

```text
AutoRefresh = false
  -> 新 snapshot 到达时不立即更新 TrendChartWidget
  -> 保存 pendingSnapshot
  -> 页面显示 Paused

AutoRefresh = true
  -> 应用 pendingSnapshot
  -> TrendChartWidget.setSnapshot()
  -> update()
```

---

# 9. Alarm Center 页面设计

Alarm Center 负责显示当前报警、历史报警、报警确认、报警详情和跳转历史。

## 9.1 UX 草图

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Alarm Center                                                                 │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐          │
│ │ Active       │ │ Unack        │ │ Recovered    │ │ Today Total  │          │
│ │ 3            │ │ 2            │ │ 8            │ │ 21           │          │
│ └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘          │
│                                                                              │
│ [Current Alarms] [History Alarms]                                             │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ Level   TagId        Message              Value   Time        Ack       │   │
│ ├────────────────────────────────────────────────────────────────────────┤   │
│ │ High    TEMP_CH01    Temperature High     86.5    10:30:01    No        │   │
│ │ Warn    VOLTAGE      Voltage Low          9.2     10:29:40    Yes       │   │
│ │ Bad     LIGHT_CH01   Device Error         --      10:28:12    No        │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ Alarm Detail                                                            │   │
│ │                                                                        │   │
│ │ AlarmId: ALM-20260607-0001                                              │   │
│ │ TagId: TEMP_CH01                                                        │   │
│ │ Level: AlarmHigh                                                        │   │
│ │ Trigger Value: 86.5 ℃                                                    │   │
│ │ Trigger Time: 2026-06-07 10:30:01                                       │   │
│ │ State: Active                                                           │   │
│ │                                                                        │   │
│ │ [Acknowledge]   [View Trend]   [View History]                           │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 9.2 Qt 组件选择

```text
AlarmCenterPage:
  QWidget

顶部摘要:
  QGridLayout + MetricCardWidget

当前 / 历史切换:
  QTabWidget
  或 QButtonGroup + QStackedWidget

报警表格:
  QTableView
  AlarmTableModel : QAbstractTableModel
  AlarmStateDelegate
  AlarmLevelDelegate

报警详情:
  QGroupBox + QLabel

操作:
  QPushButton Acknowledge
  QPushButton View Trend
  QPushButton View History
  QPushButton Query
  QPushButton Cancel
```

## 9.3 TableModel 字段

```text
AlarmTableModel columns:
  Level
  TagId
  Message
  Value
  Unit
  TriggerTime
  State
  Ack
```

## 9.4 Presenter 职责

```text
AlarmCenterPresenter
  ├── 当前模式：Current / Recent / History
  ├── applySnapshot(AlarmCenterSnapshot)
  ├── updateSummaryCards()
  ├── updateCurrentAlarmModel()
  ├── runHistoryQuery()
  ├── cancelQuery()
  ├── acknowledgeSelectedAlarm()
  ├── updateAlarmDetail()
  ├── navigateToTrend()
  └── navigateToHistory()
```

## 9.5 报警确认流程

```text
用户点击 Acknowledge
  -> AlarmCenterPresenter 检查 selectedAlarm 是否可确认
  -> 调用 AcknowledgeAlarmUseCase
  -> 成功后更新 AlarmTableModel 中对应行
  -> emit dataChanged
  -> 刷新详情区和摘要卡片
```

---

# 10. History 页面设计

History 页面负责按时间范围和 Tag 查询历史数据、显示历史趋势预览、分页和导出 CSV。

## 10.1 UX 草图

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ History Query                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ Time Range: [2026-06-07 10:00] ~ [2026-06-07 10:30]                          │
│ Tag: [TEMP_CH01 v]   Quality: [All v]   Alarm State: [All v]                 │
│                                                                              │
│ [Query]   [Cancel]   [Export CSV]   [Clear]                                  │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ History Trend Preview                                                   │   │
│ │                                                                        │   │
│ │  90 ┤                                                                  │   │
│ │  80 ┤------------------ AlarmHigh -----------------------------------  │   │
│ │  70 ┤                                                                  │   │
│ │  60 ┤------------------ WarningHigh ---------------------------------  │   │
│ │  50 ┤                  /\                                              │   │
│ │  40 ┤_________________/  \___________________________                  │   │
│ │     └────────────────────────────────────────────────── Time           │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ Timestamp               TagId        Value     Unit   Quality   Alarm   │   │
│ ├────────────────────────────────────────────────────────────────────────┤   │
│ │ 2026-06-07 10:00:01     TEMP_CH01    25.4      ℃      Good      Normal  │   │
│ │ 2026-06-07 10:00:02     TEMP_CH01    25.5      ℃      Good      Normal  │   │
│ │ 2026-06-07 10:00:03     TEMP_CH01    25.6      ℃      Good      Normal  │   │
│ │ 2026-06-07 10:00:04     TEMP_CH01    26.0      ℃      Good      Normal  │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│ [Previous] [Next]    Page: 1    Rows: 1800    Export: history_20260607.csv   │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 10.2 Qt 组件选择

```text
HistoryPage:
  QWidget

查询条件:
  QDateTimeEdit startTime
  QDateTimeEdit endTime
  QComboBox Tag
  QComboBox Quality
  QComboBox AlarmState

操作按钮:
  QPushButton Query
  QPushButton Cancel
  QPushButton Export CSV
  QPushButton Clear

历史趋势:
  TrendChartWidget
  或 HistoryTrendChartWidget 继承同一基类

历史表格:
  QTableView
  HistorySamplesTableModel

分页:
  QPushButton Previous
  QPushButton Next
  QLabel PageInfo
```

## 10.3 Presenter 职责

```text
HistoryPresenter
  ├── 管理查询参数
  ├── 校验时间范围
  ├── 调用 QueryHistorySamplesUseCase
  ├── 管理 Busy / Cancel 状态
  ├── 更新 HistorySamplesTableModel
  ├── 构造 HistoryTrendSnapshot
  ├── 更新 TrendChartWidget
  ├── 管理分页状态
  └── 调用 ExportHistoryCsvUseCase
```

## 10.4 异步查询策略

```text
用户点击 Query
  -> 禁用 Query，启用 Cancel
  -> 后台执行查询
  -> 查询完成后通过 queued signal 回 UI 线程
  -> tableModel.setRows()
  -> trendWidget.setSnapshot()
  -> 恢复按钮状态
```

---

# 11. Measurement Map 页面设计

Measurement Map 是二维测量矩阵页面。它不是普通图片，而是可交互的业务矩阵视图。

## 11.1 UX 草图

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Measurement Matrix Map                                                       │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ Matrix Type: [Light Intensity v]   Scale: [Auto v]   Colormap: [Industrial]  │
│ Show Values: [x]   Freeze: [ ]                                               │
│                                                                              │
│ ┌────────────────────────────────────────────┐ ┌─────────────────────────┐   │
│ │ Heatmap                                    │ │ Statistics               │   │
│ │                                            │ │                         │   │
│ │     0  1  2  3  4  5  6  7  8  9          │ │ Max:       890.4 lux     │   │
│ │  0  ░  ░  ▒  ▒  ▒  ▒  ▒  ░  ░  ░          │ │ Min:       420.2 lux     │   │
│ │  1  ░  ▒  ▒  ▓  ▓  ▓  ▒  ▒  ░  ░          │ │ Avg:       610.3 lux     │   │
│ │  2  ▒  ▒  ▓  ▓  █  ▓  ▓  ▒  ▒  ░          │ │ Uniformity: 82.5%        │   │
│ │  3  ▒  ▓  ▓  █  █  █  ▓  ▓  ▒  ░          │ │ Abnormal:  3 points      │   │
│ │  4  ▒  ▓  █  █  X  █  █  ▓  ▒  ░          │ │ Quality:   Good          │   │
│ │  5  ▒  ▓  ▓  █  █  █  ▓  ▓  ▒  ░          │ │                         │   │
│ │  6  ▒  ▒  ▓  ▓  █  ▓  ▓  ▒  ▒  ░          │ │ Selected:  R4 C4         │   │
│ │  7  ░  ▒  ▒  ▓  ▓  ▓  ▒  ▒  ░  ░          │ │ Value:     890.4 lux     │   │
│ │  8  ░  ░  ▒  ▒  ▒  ▒  ▒  ░  ░  ░          │ │                         │   │
│ │                                            │ │ [View Related Tag]       │   │
│ └────────────────────────────────────────────┘ └─────────────────────────┘   │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ Abnormal Points                                                         │   │
│ │ Row   Col   Value       Type       Message                              │   │
│ │ 4     4     890.4 lux   Hotspot    Local intensity too high             │   │
│ │ 4     5     872.1 lux   Hotspot    Local intensity too high             │   │
│ │ 5     4     861.7 lux   Hotspot    Local intensity too high             │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 11.2 Qt 组件选择

```text
MeasurementMapPage:
  QWidget

顶部工具条:
  QComboBox MatrixType
  QComboBox ScaleMode
  QComboBox Palette
  QCheckBox ShowValues
  QCheckBox Freeze

热力图:
  HeatmapWidget : QWidget + QPainter

统计区:
  QGroupBox / SectionPanelWidget + QLabel

异常点表格:
  QTableView
  AbnormalPointsTableModel

操作:
  QPushButton View Related Tag
  QPushButton Open Trend
```

## 11.3 HeatmapWidget 设计

```cpp
class HeatmapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HeatmapWidget(QWidget* parent = nullptr);

    void setSnapshot(const MeasurementMapSnapshot& snapshot);
    void setShowValues(bool show);
    void setSelectedCell(std::optional<MatrixCellIndex> cell);

signals:
    void cellClicked(int row, int column);
    void cellHovered(int row, int column);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    MeasurementMapSnapshot m_snapshot;
    bool m_showValues = false;
    std::optional<MatrixCellIndex> m_selectedCell;
    std::optional<MatrixCellIndex> m_hoverCell;

    QRectF heatmapArea() const;
    QRectF cellRect(int row, int column) const;
    std::optional<MatrixCellIndex> hitTest(const QPoint& pos) const;

    void drawBackground(QPainter& p);
    void drawHeaders(QPainter& p);
    void drawCells(QPainter& p);
    void drawCellValue(QPainter& p, const HeatmapCell& cell, const QRectF& rect);
    void drawAbnormalMarker(QPainter& p, const HeatmapCell& cell, const QRectF& rect);
    void drawSelectedBorder(QPainter& p);
    void drawHoverBorder(QPainter& p);
    void drawLegend(QPainter& p);
};
```

## 11.4 绘图规则

```text
HeatmapWidget 不负责矩阵分析
HeatmapWidget 不负责异常检测
HeatmapWidget 不负责颜色业务规则
HeatmapWidget 只负责把 HeatmapCell 的颜色和值画出来
HeatmapWidget 的点击事件只发出 row / column
选中逻辑由 MeasurementMapPresenter 管理
```

## 11.5 热力图绘制顺序

```text
1. 背景
2. 行列标题
3. 每个 cell 的背景色
4. 无效值斜线或特殊标记
5. 异常点角标
6. cell 数值文本
7. hover 边框
8. selected 边框
9. 色带图例
10. tooltip 或选中详情
```

## 11.6 什么时候考虑 QGraphicsView

第一版推荐 QWidget + QPainter。

只有当后续出现这些需求时，再考虑 QGraphicsView：

```text
1. 大量缩放和平移
2. 多层图元叠加
3. 每个测量点都有复杂交互
4. 需要框选区域
5. 需要拖拽编辑测量点布局
6. 矩阵不再是规则网格，而是任意空间坐标
```

---

# 12. Logs & Settings 页面设计

Logs & Settings 用 QTabWidget 分为三块：

```text
Operation Logs
Alarm Thresholds
Runtime Settings
```

## 12.1 UX 草图

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Logs & Settings                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ [Operation Logs] [Alarm Thresholds] [Runtime Settings]                       │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ Operation Logs                                                          │   │
│ │                                                                        │   │
│ │ Time Range: [Today v]   Level: [All v]   Category: [All v]              │   │
│ │ Search: [________________________]   [Query]                            │   │
│ │                                                                        │   │
│ │ Timestamp              Level   Category      Message                    │   │
│ │ 2026-06-07 10:30:01    Info    System        Simulator started          │   │
│ │ 2026-06-07 10:30:12    Warn    Alarm         TEMP_CH01 AlarmHigh        │   │
│ │ 2026-06-07 10:30:15    Info    User          Alarm acknowledged         │   │
│ │ 2026-06-07 10:31:00    Error   Database      History write failed       │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

Alarm Thresholds Tab：

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Logs & Settings > Alarm Thresholds                                           │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ Tag Category: [Measurement v]   Search: [TEMP____]                           │
│                                                                              │
│ ┌────────────────────────────────────────────────────────────────────────┐   │
│ │ TagId        WarnLow  AlarmLow  WarnHigh  AlarmHigh  Enabled            │   │
│ ├────────────────────────────────────────────────────────────────────────┤   │
│ │ TEMP_CH01    5        0         60        80         [x]                │   │
│ │ PRESSURE     90       85        115       125        [x]                │   │
│ │ VOLTAGE      10       8         14        16         [x]                │   │
│ │ VIBRATION    --       --        1.5       3.0        [x]                │   │
│ └────────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│ [Save Changes]   [Reset Default]                                             │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

Runtime Settings Tab：

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Logs & Settings > Runtime Settings                                           │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ Data Generate Interval      [500 ms    ]                                     │
│ UI Refresh Interval         [1000 ms   ]                                     │
│ Trend Refresh Interval      [1000 ms   ]                                     │
│ History Batch Interval      [5000 ms   ]                                     │
│ Log Batch Interval          [5000 ms   ]                                     │
│ Matrix Refresh Interval     [1000 ms   ]                                     │
│                                                                              │
│ Database Path               [monitor.db____________________]                 │
│ Export Directory            [C:\Exports____________________] [Browse]        │
│                                                                              │
│ [Apply]   [Restart Acquisition]                                              │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 12.2 Qt 组件选择

```text
LogsSettingsPage:
  QWidget

主控件:
  QTabWidget

Operation Logs:
  QComboBox timeRange
  QComboBox level
  QComboBox category
  QLineEdit search
  QPushButton query
  QTableView + OperationLogsTableModel

Alarm Thresholds:
  QComboBox tagCategory
  QLineEdit search
  QTableView + ThresholdSettingsTableModel
  ThresholdEditDelegate
  QPushButton Save
  QPushButton Reset

Runtime Settings:
  QFormLayout
  QSpinBox / QDoubleSpinBox
  QLineEdit databasePath
  QLineEdit exportDirectory
  QPushButton Browse
  QPushButton Apply
```

## 12.3 Presenter 职责

```text
LogsSettingsPresenter
  ├── queryOperationLogs()
  ├── filterThresholdSettings()
  ├── saveThresholds()
  ├── resetThresholds()
  ├── loadRuntimeSettings()
  ├── saveRuntimeSettings()
  ├── emit runtimeOptionsChanged
  └── 通知 ShellController 修改 QTimer 刷新间隔
```

---

# 13. 页面输入输出设计

## 13.1 Dashboard

```text
输入：
  DashboardSnapshot
  TrendSeriesSnapshot
  MatrixPreviewSnapshot
  AlarmSummary
  DataQualitySummary

输出：
  navigateToAlarmCenter()
  navigateToTrend(tagId)
  navigateToMeasurementMap()
  navigateToRealtimeTags(tagId)
```

## 13.2 Realtime Tags

```text
输入：
  RealtimeTagRow[]
  TagDefinitionMap

输出：
  filterChanged()
  selectedTagChanged(tagId)
  navigateToTrend(tagId)
  navigateToHistory(tagId)
```

## 13.3 Trend

```text
输入：
  AvailableTags
  SelectedTagId
  SelectedWindow
  TrendSnapshot

输出：
  selectedTagChanged(tagId)
  selectedWindowChanged(window)
  autoRefreshChanged(enabled)
  navigateToHistory(tagId)
```

## 13.4 Alarm Center

```text
输入：
  AlarmCenterSnapshot
  AlarmHistoryQueryResult

输出：
  acknowledgeAlarm(alarmId)
  queryHistoryAlarms(filter)
  navigateToTrend(tagId)
  navigateToHistory(tagId, timeRange)
```

## 13.5 History

```text
输入：
  AvailableTags
  HistoryQueryParameter
  HistoryQueryResult
  HistoryTrendSnapshot

输出：
  queryHistory(parameter)
  cancelQuery()
  exportCsv(parameter)
  navigateToTrend(tagId)
```

## 13.6 Measurement Map

```text
输入：
  MeasurementMapSnapshot
  MatrixDisplayOptions
  AbnormalPointRow[]

输出：
  selectedCellChanged(row, column)
  displayOptionsChanged(options)
  freezeChanged(enabled)
  navigateToRealtimeTags(tagId)
  navigateToTrend(tagId)
```

## 13.7 Logs & Settings

```text
输入：
  OperationLogRow[]
  ThresholdSettingRow[]
  RuntimeSettings

输出：
  queryLogs(filter)
  saveThresholds(rows)
  saveRuntimeSettings(settings)
  runtimeOptionsChanged(settings)
```

---

# 14. 数据刷新策略

## 14.1 刷新原则

```text
实时页面读快照
历史页面查数据库
设置页面改配置
图表控件只画快照
表格控件只显示 Model
UI 不直接订阅原始设备帧
```

## 14.2 页面刷新表

```text
┌──────────────────────┬──────────────────────┬────────────────────────────┐
│ 页面                 │ 刷新方式             │ Qt 实现                    │
├──────────────────────┼──────────────────────┼────────────────────────────┤
│ Dashboard            │ 1s 定时刷新          │ QTimer + Presenter         │
│ Realtime Tags        │ 1s 定时刷新          │ QTimer + QTableView Model  │
│ Trend                │ 1s 定时刷新          │ QTimer + TrendChartWidget  │
│ Alarm Center 当前报警 │ 事件 + 1s 刷新       │ signal + TableModel        │
│ Alarm Center 历史报警 │ 查询触发             │ async query + TableModel   │
│ History              │ 查询触发             │ async query + TableModel   │
│ Measurement Map      │ 1s 或 2s 定时刷新    │ QTimer + HeatmapWidget     │
│ Logs                 │ 查询触发 / 手动刷新  │ async query + TableModel   │
│ Settings             │ 用户操作触发         │ Form widgets + UseCase     │
└──────────────────────┴──────────────────────┴────────────────────────────┘
```

## 14.3 跨线程 UI 更新规则

```text
后台线程：
  采集、清洗、报警、矩阵分析、历史查询、日志查询

UI 主线程：
  QWidget、QTableView、QPainter、QTimer、页面状态显示

线程边界：
  后台线程不能直接操作 QWidget
  后台线程通过 signal 发出结果
  UI 对象的 slot 在主线程执行
```

典型流程：

```text
HistoryQueryWorker 查询完成
  -> emit queryFinished(result)
  -> HistoryPresenter::onQueryFinished(result)
  -> tableModel.setRows(result.rows)
  -> trendWidget.setSnapshot(result.trend)
```

---

# 15. Qt 自定义绘图组件设计原则

## 15.1 TrendChartWidget

适合场景：

```text
实时趋势图
历史趋势预览
Dashboard 小趋势图
```

职责：

```text
1. 保存 TrendSnapshot
2. 根据 snapshot 计算坐标映射
3. 绘制坐标轴、网格、阈值线、折线、质量点、当前点
4. 支持 hover 查看点信息
5. 支持 resize 后重新绘制
```

不负责：

```text
1. 不查数据库
2. 不判断报警
3. 不清洗数据
4. 不管理 Tag 选择
5. 不管理时间窗口选择
```

## 15.2 HeatmapWidget

适合场景：

```text
Measurement Map 主热力图
Dashboard 矩阵预览
```

职责：

```text
1. 保存 MeasurementMapSnapshot
2. 根据 cell 的颜色和值绘制矩阵
3. 支持点击 cell
4. 支持 hover cell
5. 绘制异常点、选中边框、图例
```

不负责：

```text
1. 不做异常检测
2. 不做矩阵统计
3. 不做业务颜色映射
4. 不直接跳转页面
```

## 15.3 Delegate 绘制

对于表格中的质量、报警、状态，可以用 QStyledItemDelegate：

```text
QualityStateDelegate:
  Good / Bad / Timeout / Offline 状态徽章

AlarmStateDelegate:
  Normal / Warning / Alarm / Recovered 状态徽章

NumericValueDelegate:
  数值格式化、单位显示、超限强调

ThresholdEditDelegate:
  阈值编辑控件
```

---

# 16. 组件复用设计

为了避免每个页面随意拼控件，Qt 版本建议抽象以下复用组件：

```text
MetricCardWidget
  Dashboard 顶部指标卡
  Alarm Center 摘要卡

StatusBadgeWidget
  Running / Stopped / Offline / Error

AlarmBadgeWidget
  Normal / Warning / Alarm / Recovered

QualityBadgeWidget
  Good / Bad / Timeout / DeviceError / Offline

SectionPanelWidget
  页面分块容器，替代 WPF Border + Header

PageToolbarWidget
  查询条件、筛选条件、操作按钮区域

TrendChartWidget
  趋势图主控件

MiniTrendChartWidget
  Dashboard 小趋势图

HeatmapWidget
  Measurement Map 主热力图

MiniHeatmapWidget
  Dashboard 矩阵预览

DataTableView
  对 QTableView 做统一样式和默认行为封装
```

---

# 17. 推荐文件夹结构

```text
MultiChannelMonitorQt
├── CMakeLists.txt
├── src
│   ├── main.cpp
│   │
│   ├── app
│   │   ├── QtAppBootstrapper.h
│   │   ├── QtAppBootstrapper.cpp
│   │   ├── RuntimeComposition.h
│   │   └── RuntimeComposition.cpp
│   │
│   ├── shell
│   │   ├── MainWindow.h
│   │   ├── MainWindow.cpp
│   │   ├── ShellController.h
│   │   ├── ShellController.cpp
│   │   ├── SideNavigationWidget.h
│   │   ├── SideNavigationWidget.cpp
│   │   ├── TopStatusBarWidget.h
│   │   ├── TopStatusBarWidget.cpp
│   │   ├── BottomStatusBarWidget.h
│   │   └── BottomStatusBarWidget.cpp
│   │
│   ├── navigation
│   │   ├── NavigationPage.h
│   │   ├── NavigationItem.h
│   │   ├── NavigationParameter.h
│   │   ├── NavigationService.h
│   │   └── NavigationService.cpp
│   │
│   ├── pages
│   │   ├── dashboard
│   │   │   ├── DashboardPage.h
│   │   │   ├── DashboardPage.cpp
│   │   │   ├── DashboardPresenter.h
│   │   │   └── DashboardPresenter.cpp
│   │   │
│   │   ├── realtime_tags
│   │   │   ├── RealtimeTagsPage.h
│   │   │   ├── RealtimeTagsPage.cpp
│   │   │   ├── RealtimeTagsPresenter.h
│   │   │   └── RealtimeTagsPresenter.cpp
│   │   │
│   │   ├── trend
│   │   │   ├── TrendPage.h
│   │   │   ├── TrendPage.cpp
│   │   │   ├── TrendPresenter.h
│   │   │   └── TrendPresenter.cpp
│   │   │
│   │   ├── alarm_center
│   │   │   ├── AlarmCenterPage.h
│   │   │   ├── AlarmCenterPage.cpp
│   │   │   ├── AlarmCenterPresenter.h
│   │   │   └── AlarmCenterPresenter.cpp
│   │   │
│   │   ├── history
│   │   │   ├── HistoryPage.h
│   │   │   ├── HistoryPage.cpp
│   │   │   ├── HistoryPresenter.h
│   │   │   └── HistoryPresenter.cpp
│   │   │
│   │   ├── measurement_map
│   │   │   ├── MeasurementMapPage.h
│   │   │   ├── MeasurementMapPage.cpp
│   │   │   ├── MeasurementMapPresenter.h
│   │   │   └── MeasurementMapPresenter.cpp
│   │   │
│   │   └── logs_settings
│   │       ├── LogsSettingsPage.h
│   │       ├── LogsSettingsPage.cpp
│   │       ├── LogsSettingsPresenter.h
│   │       └── LogsSettingsPresenter.cpp
│   │
│   ├── models
│   │   ├── RealtimeTagsTableModel.h
│   │   ├── RealtimeTagsTableModel.cpp
│   │   ├── AlarmTableModel.h
│   │   ├── AlarmTableModel.cpp
│   │   ├── HistorySamplesTableModel.h
│   │   ├── HistorySamplesTableModel.cpp
│   │   ├── OperationLogsTableModel.h
│   │   ├── OperationLogsTableModel.cpp
│   │   ├── ThresholdSettingsTableModel.h
│   │   ├── ThresholdSettingsTableModel.cpp
│   │   ├── AbnormalPointsTableModel.h
│   │   └── AbnormalPointsTableModel.cpp
│   │
│   ├── widgets
│   │   ├── charts
│   │   │   ├── TrendChartWidget.h
│   │   │   ├── TrendChartWidget.cpp
│   │   │   ├── MiniTrendChartWidget.h
│   │   │   ├── MiniTrendChartWidget.cpp
│   │   │   ├── ChartAxisMapper.h
│   │   │   └── ChartAxisMapper.cpp
│   │   │
│   │   ├── heatmap
│   │   │   ├── HeatmapWidget.h
│   │   │   ├── HeatmapWidget.cpp
│   │   │   ├── MiniHeatmapWidget.h
│   │   │   ├── MiniHeatmapWidget.cpp
│   │   │   ├── HeatmapColorMapper.h
│   │   │   ├── HeatmapColorMapper.cpp
│   │   │   ├── HeatmapHitTester.h
│   │   │   └── HeatmapHitTester.cpp
│   │   │
│   │   ├── common
│   │   │   ├── MetricCardWidget.h
│   │   │   ├── MetricCardWidget.cpp
│   │   │   ├── StatusBadgeWidget.h
│   │   │   ├── StatusBadgeWidget.cpp
│   │   │   ├── SectionPanelWidget.h
│   │   │   ├── SectionPanelWidget.cpp
│   │   │   ├── PageToolbarWidget.h
│   │   │   └── PageToolbarWidget.cpp
│   │   │
│   │   └── delegates
│   │       ├── QualityStateDelegate.h
│   │       ├── QualityStateDelegate.cpp
│   │       ├── AlarmStateDelegate.h
│   │       ├── AlarmStateDelegate.cpp
│   │       ├── NumericValueDelegate.h
│   │       └── NumericValueDelegate.cpp
│   │
│   ├── services
│   │   ├── UiRefreshTimerService.h
│   │   ├── UiRefreshTimerService.cpp
│   │   ├── UiThreadDispatcher.h
│   │   ├── UiThreadDispatcher.cpp
│   │   ├── DialogService.h
│   │   ├── DialogService.cpp
│   │   ├── FilePickerService.h
│   │   └── FilePickerService.cpp
│   │
│   ├── dto
│   │   ├── UiCompositeSnapshot.h
│   │   ├── DashboardSnapshot.h
│   │   ├── TrendSnapshot.h
│   │   ├── MeasurementMapSnapshot.h
│   │   ├── AlarmCenterSnapshot.h
│   │   └── RuntimeSettings.h
│   │
│   └── style
│       ├── StyleManager.h
│       └── StyleManager.cpp
│
├── resources
│   ├── resources.qrc
│   ├── icons
│   └── qss
│       ├── app.qss
│       ├── colors.qss
│       ├── tables.qss
│       ├── buttons.qss
│       └── cards.qss
│
└── tests
    ├── model_tests
    ├── presenter_tests
    └── widget_tests
```

---

# 18. CMake 模块建议

```cmake
cmake_minimum_required(VERSION 3.25)

project(MultiChannelMonitorQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
)

qt_standard_project_setup()

qt_add_executable(MultiChannelMonitorQt
    src/main.cpp
    resources/resources.qrc
)

target_link_libraries(MultiChannelMonitorQt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
```

---

# 19. Qt 版本迁移开发顺序

第一阶段：搭 Shell

```text
1. 建立 CMake + Qt6 Widgets 项目
2. 创建 MainWindow
3. 创建 SideNavigationWidget
4. 创建 QStackedWidget
5. 放入 7 个空 Page
6. 实现 NavigationService
7. 实现顶部和底部状态栏
```

第二阶段：搭静态页面

```text
1. Dashboard 静态布局
2. Realtime Tags 静态表格
3. Trend 静态布局
4. Alarm Center 静态布局
5. History 静态布局
6. Measurement Map 静态布局
7. Logs & Settings 静态 Tab
```

第三阶段：接入表格 Model

```text
1. RealtimeTagsTableModel
2. AlarmTableModel
3. HistorySamplesTableModel
4. OperationLogsTableModel
5. ThresholdSettingsTableModel
6. AbnormalPointsTableModel
```

第四阶段：实现自绘控件

```text
1. TrendChartWidget
2. MiniTrendChartWidget
3. HeatmapWidget
4. MiniHeatmapWidget
5. Delegate 状态徽章绘制
```

第五阶段：接入业务快照

```text
1. UiSnapshotProvider Adapter
2. ShellController QTimer 定时拉取
3. DashboardPresenter.applySnapshot
4. RealtimeTagsPresenter.applySnapshot
5. TrendPresenter.applySnapshot
6. AlarmCenterPresenter.applySnapshot
7. MeasurementMapPresenter.applySnapshot
```

第六阶段：接入查询和设置

```text
1. History 查询
2. Alarm 历史查询
3. Operation Logs 查询
4. CSV 导出
5. 阈值保存
6. Runtime Settings 保存
7. UI Refresh Interval 动态调整 QTimer
```

---

# 20. 面试表达模型

Qt6 版本可以这样对外解释：

```text
这个项目从 WPF 迁移到 Qt6 时，我没有把 XAML 和 ViewModel 逐行翻译成 C++，
而是先保留原上位机的数据闭环和页面闭环，再用 Qt 的对象模型重建 UI 层。

原来的 WPF 是：
UiSnapshot -> ShellViewModel -> PageViewModel -> Binding -> XAML / Renderer。

Qt6 版本变成：
UiSnapshot -> ShellController -> PagePresenter -> QAbstractTableModel / QWidget State -> QTableView / paintEvent。

普通表格用 QTableView + QAbstractTableModel；
趋势图和热力图使用 QWidget + QPainter 自绘；
主窗口使用 QMainWindow + QStackedWidget 做 Shell 和页面切换；
后台线程通过 signal/slot 把结果切回 UI 线程，避免直接操作控件。

这样迁移后，业务边界没有被破坏：
Application 层仍然负责清洗、报警、矩阵分析和快照；
Qt Presentation 层只负责状态展示、用户交互和图像绘制。
```

最终压缩成一句话：

```text
Qt6 Widgets 版本不是把 WPF MVVM 生搬硬套过来，
而是把原来的“UI 快照驱动页面”思想，
迁移成 Qt 的“信号槽驱动状态，paintEvent 根据状态重绘”的桌面 UI 架构。
```
