#include "MonitoringPages.h"

#include "presentation/export/CsvExportWriter.h"

#include "domain/alarms/AlarmModels.h"
#include "domain/logs/LogModels.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

using Monitor::Application::Dtos::MeasurementMapSnapshot;
using Monitor::Application::Dtos::TrendPointDto;
using Monitor::Application::Dtos::UiSnapshot;
using Monitor::Application::Configuration::TagRuntimeConfiguration;
using Monitor::Domain::Alarms::AlarmEvent;
using Monitor::Domain::Tags::TagRuntimeState;
using Monitor::Domain::Tags::TagValue;

enum class TrendPointSource {
    RealtimeBuffer,
    HistorySamples
};

QString localTime(const QDateTime &timestampUtc)
{
    return timestampUtc.isValid()
        ? timestampUtc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");
}

QString optionalText(const std::optional<QString> &value)
{
    return value.value_or(QString());
}

QString valueText(const TagRuntimeState &state)
{
    if (state.numericValue.has_value()) {
        return QString::number(state.numericValue.value(), 'f', 2);
    }
    if (state.boolValue.has_value()) {
        return state.boolValue.value() ? QStringLiteral("True") : QStringLiteral("False");
    }
    return state.textValue.value_or(QStringLiteral("-"));
}

QString sampleValueText(const TagValue &sample)
{
    return QString::number(sample.value, 'f', 3);
}

QString qualityStateText(Monitor::Application::Dtos::MatrixQualityState state)
{
    using Monitor::Application::Dtos::MatrixQualityState;

    switch (state) {
    case MatrixQualityState::Good:
        return QStringLiteral("Good");
    case MatrixQualityState::Attention:
        return QStringLiteral("Attention");
    case MatrixQualityState::Warning:
        return QStringLiteral("Warning");
    case MatrixQualityState::Alarm:
        return QStringLiteral("Alarm");
    }

    return QStringLiteral("Unknown");
}

QString matrixSeverityText(Monitor::Application::Dtos::MatrixSeverity severity)
{
    using Monitor::Application::Dtos::MatrixSeverity;

    switch (severity) {
    case MatrixSeverity::Info:
        return QStringLiteral("Info");
    case MatrixSeverity::Warning:
        return QStringLiteral("Warning");
    case MatrixSeverity::Alarm:
        return QStringLiteral("Alarm");
    }

    return QStringLiteral("Unknown");
}

QString abnormalTypeText(Monitor::Application::Dtos::MatrixAbnormalType type)
{
    using Monitor::Application::Dtos::MatrixAbnormalType;

    switch (type) {
    case MatrixAbnormalType::InvalidValue:
        return QStringLiteral("InvalidValue");
    case MatrixAbnormalType::HighLimit:
        return QStringLiteral("HighLimit");
    case MatrixAbnormalType::LowLimit:
        return QStringLiteral("LowLimit");
    case MatrixAbnormalType::StatisticalHotspot:
        return QStringLiteral("StatisticalHotspot");
    case MatrixAbnormalType::StatisticalColdspot:
        return QStringLiteral("StatisticalColdspot");
    case MatrixAbnormalType::LocalHotspot:
        return QStringLiteral("LocalHotspot");
    case MatrixAbnormalType::LocalColdspot:
        return QStringLiteral("LocalColdspot");
    }

    return QStringLiteral("Unknown");
}

QString displayNameFor(const UiSnapshot &snapshot, const QString &tagId)
{
    const auto definitionIt = std::find_if(snapshot.tagDefinitions.cbegin(), snapshot.tagDefinitions.cend(), [&tagId](const auto &definition) {
        return definition.tagId == tagId;
    });
    return definitionIt == snapshot.tagDefinitions.cend() ? tagId : definitionIt->displayName;
}

std::optional<TagRuntimeConfiguration> configurationFor(const UiSnapshot &snapshot, const QString &tagId)
{
    const auto it = std::find_if(snapshot.tagConfigurations.cbegin(), snapshot.tagConfigurations.cend(), [&tagId](const auto &configuration) {
        return configuration.tagId == tagId;
    });
    if (it == snapshot.tagConfigurations.cend()) {
        return std::nullopt;
    }
    return *it;
}

QTableWidgetItem *item(const QString &text)
{
    auto *tableItem = new QTableWidgetItem(text);
    tableItem->setFlags(tableItem->flags() & ~Qt::ItemIsEditable);
    tableItem->setForeground(QColor(QStringLiteral("#1E293B")));
    return tableItem;
}

int limitedRowCount(qsizetype size, int maximum)
{
    return static_cast<int>(std::min<qsizetype>(size, maximum));
}

std::optional<double> optionalDoubleFromItem(const QTableWidgetItem *tableItem)
{
    if (tableItem == nullptr) {
        return std::nullopt;
    }

    const auto text = tableItem->text().trimmed();
    if (text.isEmpty()) {
        return std::nullopt;
    }

    bool ok = false;
    const auto value = text.toDouble(&ok);
    return ok ? std::optional<double>(value) : std::nullopt;
}

int intFromItem(const QTableWidgetItem *tableItem, int fallback)
{
    if (tableItem == nullptr) {
        return fallback;
    }

    bool ok = false;
    const auto value = tableItem->text().trimmed().toInt(&ok);
    return ok ? value : fallback;
}

int badQualityCount(const QVector<TrendPointDto> &points)
{
    return static_cast<int>(std::count_if(points.cbegin(), points.cend(), [](const auto &point) {
        return point.quality != Monitor::Domain::Tags::TagQuality::Good;
    }));
}

int spikeCount(const QVector<TrendPointDto> &points)
{
    return static_cast<int>(std::count_if(points.cbegin(), points.cend(), [](const auto &point) {
        return point.isSpike;
    }));
}

double normalizedValue(double value, const Monitor::Application::Dtos::ScaleRange &range)
{
    if (!std::isfinite(value) || !std::isfinite(range.minValue) || !std::isfinite(range.maxValue)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (std::abs(range.range()) < 0.000001) {
        return 0.5;
    }

    return std::clamp((value - range.minValue) / range.range(), 0.0, 1.0);
}

QColor heatColorFor(double normalized)
{
    if (!std::isfinite(normalized)) {
        return QColor(96, 96, 96);
    }

    const auto clamped = std::clamp(normalized, 0.0, 1.0);
    if (clamped < 0.25) {
        const auto t = clamped / 0.25;
        return QColor(
            static_cast<int>(18 + 28 * t),
            static_cast<int>(38 + 48 * t),
            static_cast<int>(79 + 54 * t));
    }

    if (clamped < 0.50) {
        const auto t = (clamped - 0.25) / 0.25;
        return QColor(
            static_cast<int>(46 + 14 * t),
            static_cast<int>(86 + 118 * t),
            static_cast<int>(133 - 42 * t));
    }

    if (clamped < 0.75) {
        const auto t = (clamped - 0.50) / 0.25;
        return QColor(
            static_cast<int>(60 + 185 * t),
            static_cast<int>(204 - 76 * t),
            static_cast<int>(91 - 48 * t));
    }

    const auto t = (clamped - 0.75) / 0.25;
    return QColor(
        static_cast<int>(245 + 10 * t),
        static_cast<int>(128 + 82 * t),
        static_cast<int>(43 + 99 * t));
}

QColor heatColorFor(const Monitor::Application::Dtos::HeatmapCell &cell, const std::optional<Monitor::Application::Dtos::ScaleRange> &range)
{
    if (range.has_value()) {
        return heatColorFor(normalizedValue(cell.value, range.value()));
    }

    return QColor(cell.color.r, cell.color.g, cell.color.b);
}

QTableWidgetItem *numericItem(double value, int decimals = 2)
{
    auto *tableItem = item(QString::number(value, 'f', decimals));
    tableItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return tableItem;
}

QWidget *card(const QString &title, QLabel **valueLabel, QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("summaryCard"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(4);
    auto *titleLabel = new QLabel(title, frame);
    titleLabel->setObjectName(QStringLiteral("cardTitle"));
    *valueLabel = new QLabel(QStringLiteral("-"), frame);
    (*valueLabel)->setObjectName(QStringLiteral("cardValue"));
    layout->addWidget(titleLabel);
    layout->addWidget(*valueLabel);
    return frame;
}

void setupTable(QTableWidget *table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

QScrollArea *scrollPage(QWidget *content)
{
    auto *scroll = new QScrollArea(content->parentWidget());
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

QVector<TagValue> filteredSamples(const UiSnapshot &snapshot, const QString &tagId, int limit)
{
    QVector<TagValue> result;
    for (auto it = snapshot.historySamples.crbegin(); it != snapshot.historySamples.crend(); ++it) {
        if (!tagId.isEmpty() && it->tagId != tagId) {
            continue;
        }
        result.append(*it);
        if (result.size() >= limit) {
            break;
        }
    }
    return result;
}

void markSpikePoints(QVector<TrendPointDto> *points)
{
    if (!points || points->size() < 3) {
        return;
    }

    double totalDelta = 0.0;
    for (auto index = 1; index < points->size(); ++index) {
        totalDelta += std::abs(points->at(index).value - points->at(index - 1).value);
    }
    const auto averageDelta = totalDelta / static_cast<double>(points->size() - 1);
    const auto spikeThreshold = std::max(averageDelta * 3.0, 0.1);
    for (auto index = 1; index < points->size(); ++index) {
        (*points)[index].isSpike = std::abs(points->at(index).value - points->at(index - 1).value) > spikeThreshold;
    }
}

QVector<TrendPointDto> trendPointsFor(
    const UiSnapshot &snapshot,
    const QString &tagId,
    int maxPoints,
    TrendPointSource source)
{
    QVector<TrendPointDto> result;
    if (source == TrendPointSource::HistorySamples) {
        const auto samples = filteredSamples(snapshot, tagId, maxPoints);
        result.reserve(samples.size());
        for (auto it = samples.crbegin(); it != samples.crend(); ++it) {
            result.append(TrendPointDto{it->timestampUtc, it->value, it->quality, false});
        }
        markSpikePoints(&result);
        return result;
    }

    const auto bufferIt = snapshot.tags.recentBuffers.constFind(tagId);
    if (bufferIt == snapshot.tags.recentBuffers.cend()) {
        return result;
    }
    const auto start = std::max<qsizetype>(0, bufferIt.value().size() - maxPoints);
    for (auto index = start; index < bufferIt.value().size(); ++index) {
        const auto &point = bufferIt.value().at(index);
        result.append(TrendPointDto{point.timestampUtc, point.value, point.quality, false});
    }
    markSpikePoints(&result);
    return result;
}

QString hotspotText(const MeasurementMapSnapshot &map)
{
    if (!map.abnormalPoints.isEmpty()) {
        const auto best = std::max_element(map.abnormalPoints.cbegin(), map.abnormalPoints.cend(), [](const auto &left, const auto &right) {
            if (left.severity != right.severity) {
                return static_cast<int>(left.severity) < static_cast<int>(right.severity);
            }
            return left.value < right.value;
        });
        if (best != map.abnormalPoints.cend()) {
            return QStringLiteral("Hotspot: [%1, %2] %3 %4 (%5)")
                .arg(best->row)
                .arg(best->column)
                .arg(best->value, 0, 'f', 2)
                .arg(map.unit, abnormalTypeText(best->type));
        }
    }

    const auto bestCell = std::max_element(map.cells.cbegin(), map.cells.cend(), [](const auto &left, const auto &right) {
        return left.value < right.value;
    });
    if (bestCell == map.cells.cend()) {
        return QStringLiteral("Hotspot: -");
    }

    return QStringLiteral("Hotspot: [%1, %2] %3 %4")
        .arg(bestCell->row)
        .arg(bestCell->column)
        .arg(bestCell->value, 0, 'f', 2)
        .arg(map.unit);
}

} // namespace

TrendChartWidget::TrendChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(260);
    setObjectName(QStringLiteral("trendChart"));
}

void TrendChartWidget::setSeries(
    const QVector<TrendPointDto> &points,
    const std::optional<TagRuntimeConfiguration> &configuration)
{
    m_points = points;
    m_configuration = configuration;
    update();
}

void TrendChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#F8FAFC")));
    const QRect plot = rect().adjusted(42, 20, -18, -34);
    painter.setPen(QPen(QColor(QStringLiteral("#CBD5E1")), 1));
    painter.drawRect(plot);
    if (m_points.size() < 2) {
        painter.setPen(QColor(QStringLiteral("#64748B")));
        painter.drawText(plot, Qt::AlignCenter, tr("Waiting for trend samples"));
        return;
    }

    auto minValue = m_points.first().value;
    auto maxValue = m_points.first().value;
    for (const auto &point : m_points) {
        minValue = std::min(minValue, point.value);
        maxValue = std::max(maxValue, point.value);
    }
    const auto includeThreshold = [&minValue, &maxValue](const std::optional<double> &threshold) {
        if (threshold.has_value()) {
            minValue = std::min(minValue, threshold.value());
            maxValue = std::max(maxValue, threshold.value());
        }
    };
    if (m_configuration.has_value()) {
        includeThreshold(m_configuration->warningLow);
        includeThreshold(m_configuration->alarmLow);
        includeThreshold(m_configuration->warningHigh);
        includeThreshold(m_configuration->alarmHigh);
    }
    if (std::abs(maxValue - minValue) < 0.001) {
        maxValue += 1.0;
        minValue -= 1.0;
    }

    painter.setPen(QPen(QColor(QStringLiteral("#E2E8F0")), 1));
    for (auto i = 1; i < 4; ++i) {
        const auto y = plot.top() + plot.height() * i / 4;
        painter.drawLine(plot.left(), y, plot.right(), y);
    }

    const auto valueToY = [&plot, minValue, maxValue](double value) {
        const auto normalized = (value - minValue) / (maxValue - minValue);
        return plot.bottom() - normalized * plot.height();
    };
    const auto denominator = std::max<qsizetype>(1, m_points.size() - 1);
    const auto pointPosition = [&plot, &valueToY, denominator](qsizetype index, double value) {
        const auto x = plot.left() + (plot.width() * index) / denominator;
        return QPointF(x, valueToY(value));
    };
    const auto drawThreshold = [&painter, &plot, &valueToY](const std::optional<double> &threshold, const QColor &color, const QString &label) {
        if (!threshold.has_value()) {
            return;
        }
        const auto y = valueToY(threshold.value());
        QPen pen(color, 1);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.drawText(
            QRectF(plot.left() + 6, y - 16, plot.width() - 12, 16),
            Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("%1 %2").arg(label, QString::number(threshold.value(), 'f', 1)));
    };
    if (m_configuration.has_value()) {
        drawThreshold(m_configuration->warningLow, QColor(QStringLiteral("#D97706")), tr("Warn Low"));
        drawThreshold(m_configuration->alarmLow, QColor(QStringLiteral("#DC2626")), tr("Alarm Low"));
        drawThreshold(m_configuration->warningHigh, QColor(QStringLiteral("#D97706")), tr("Warn High"));
        drawThreshold(m_configuration->alarmHigh, QColor(QStringLiteral("#DC2626")), tr("Alarm High"));
    }

    QPainterPath path;
    for (auto i = 0; i < m_points.size(); ++i) {
        const auto position = pointPosition(i, m_points.at(i).value);
        if (i == 0) {
            path.moveTo(position);
        } else {
            path.lineTo(position);
        }
    }
    painter.setPen(QPen(QColor(QStringLiteral("#256D85")), 2));
    painter.drawPath(path);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#F59E0B")));
    for (auto i = 0; i < m_points.size(); ++i) {
        if (m_points.at(i).quality == Monitor::Domain::Tags::TagQuality::Good) {
            continue;
        }
        const auto position = pointPosition(i, m_points.at(i).value);
        painter.drawRect(QRectF(position.x() - 3.5, position.y() - 3.5, 7.0, 7.0));
    }
    painter.setBrush(QColor(QStringLiteral("#DC2626")));
    for (auto i = 0; i < m_points.size(); ++i) {
        if (!m_points.at(i).isSpike) {
            continue;
        }
        painter.drawEllipse(pointPosition(i, m_points.at(i).value), 4.0, 4.0);
    }
    painter.setPen(QColor(QStringLiteral("#475569")));
    painter.drawText(QRect(4, plot.top(), 36, 20), Qt::AlignRight, QString::number(maxValue, 'f', 1));
    painter.drawText(QRect(4, plot.bottom() - 20, 36, 20), Qt::AlignRight, QString::number(minValue, 'f', 1));
    painter.drawText(QRect(plot.left(), plot.bottom() + 8, plot.width(), 20), Qt::AlignCenter,
                     tr("%1 points").arg(m_points.size()));
    painter.setPen(QColor(QStringLiteral("#64748B")));
    painter.drawText(QRect(plot.right() - 230, plot.top() + 6, 220, 18), Qt::AlignRight,
                     tr("line: value | square: quality | dot: spike"));
}

HeatmapWidget::HeatmapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(220, 220);
    setObjectName(QStringLiteral("heatmapWidget"));
}

void HeatmapWidget::setSnapshot(const std::optional<MeasurementMapSnapshot> &snapshot)
{
    m_snapshot = snapshot;
    if (!m_snapshot.has_value()) {
        m_selectedRow = -1;
        m_selectedColumn = -1;
    }
    update();
}

void HeatmapWidget::setRenderRange(const std::optional<Monitor::Application::Dtos::ScaleRange> &range)
{
    m_renderRange = range;
    update();
}

QRectF HeatmapWidget::heatmapArea() const
{
    if (!m_snapshot.has_value() || m_snapshot->frame.rows <= 0 || m_snapshot->frame.columns <= 0) {
        return QRectF();
    }

    const auto size = std::max(1, std::min(width() - 20, height() - 44));
    return QRectF((width() - size) / 2.0, 10.0, size, size);
}

std::optional<Monitor::Application::Dtos::HeatmapCell> HeatmapWidget::cellAtPosition(const QPoint &position) const
{
    if (!m_snapshot.has_value() || m_snapshot->frame.rows <= 0 || m_snapshot->frame.columns <= 0) {
        return std::nullopt;
    }

    const auto area = heatmapArea();
    if (!area.contains(position)) {
        return std::nullopt;
    }

    const auto row = std::clamp(
        static_cast<int>((position.y() - area.top()) / (area.height() / m_snapshot->frame.rows)),
        0,
        m_snapshot->frame.rows - 1);
    const auto column = std::clamp(
        static_cast<int>((position.x() - area.left()) / (area.width() / m_snapshot->frame.columns)),
        0,
        m_snapshot->frame.columns - 1);
    const auto it = std::find_if(m_snapshot->cells.cbegin(), m_snapshot->cells.cend(), [row, column](const auto &cell) {
        return cell.row == row && cell.column == column;
    });
    if (it == m_snapshot->cells.cend()) {
        return std::nullopt;
    }
    return *it;
}

void HeatmapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#F8FAFC")));
    if (!m_snapshot.has_value() || m_snapshot->frame.rows <= 0 || m_snapshot->frame.columns <= 0) {
        painter.setPen(QColor(QStringLiteral("#64748B")));
        painter.drawText(rect(), Qt::AlignCenter, tr("No matrix frame"));
        return;
    }

    const auto rows = m_snapshot->frame.rows;
    const auto columns = m_snapshot->frame.columns;
    const auto area = heatmapArea();
    const auto cellWidth = area.width() / columns;
    const auto cellHeight = area.height() / rows;
    for (const auto &cell : m_snapshot->cells) {
        QRectF cellRect(
            area.left() + cell.column * cellWidth,
            area.top() + cell.row * cellHeight,
            cellWidth,
            cellHeight);
        painter.fillRect(cellRect.adjusted(0.5, 0.5, -0.5, -0.5), heatColorFor(cell, m_renderRange));
        if (cell.isAbnormal) {
            painter.setPen(QPen(QColor(QStringLiteral("#DC2626")), 1.5));
            painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
        }
        if (cell.row == m_selectedRow && cell.column == m_selectedColumn) {
            painter.setPen(QPen(QColor(QStringLiteral("#111827")), 2));
            painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
        }
    }
    painter.setPen(QPen(QColor(QStringLiteral("#CBD5E1")), 1));
    painter.drawRect(area);

    const auto range = m_renderRange.value_or(m_snapshot->scaleRange);
    const QRectF legend(area.left(), area.bottom() + 10, area.width(), 10);
    for (auto x = 0; x < static_cast<int>(legend.width()); ++x) {
        const auto normalized = legend.width() <= 1 ? 0.0 : x / (legend.width() - 1);
        painter.setPen(heatColorFor(normalized));
        painter.drawLine(QPointF(legend.left() + x, legend.top()), QPointF(legend.left() + x, legend.bottom()));
    }
    painter.setPen(QColor(QStringLiteral("#475569")));
    painter.drawText(QRectF(legend.left(), legend.bottom() + 2, legend.width() / 2.0, 18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString::number(range.minValue, 'f', 1));
    painter.drawText(QRectF(legend.center().x(), legend.bottom() + 2, legend.width() / 2.0, 18),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString::number(range.maxValue, 'f', 1));
}

void HeatmapWidget::mousePressEvent(QMouseEvent *event)
{
    const auto cell = cellAtPosition(event->pos());
    if (!cell.has_value()) {
        return;
    }

    m_selectedRow = cell->row;
    m_selectedColumn = cell->column;
    emit cellSelected(cell->row, cell->column, cell->value);
    update();
}

DashboardPageWidget::DashboardPageWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("pageSurface"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    auto *cardsLayout = new QGridLayout;
    cardsLayout->setSpacing(10);
    cardsLayout->addWidget(card(tr("Total Tags"), &m_totalTagsLabel, content), 0, 0);
    cardsLayout->addWidget(card(tr("Bad Quality"), &m_badQualityLabel, content), 0, 1);
    cardsLayout->addWidget(card(tr("Active Alarms"), &m_activeAlarmsLabel, content), 0, 2);
    cardsLayout->addWidget(card(tr("Matrix Frame"), &m_matrixLabel, content), 0, 3);
    layout->addLayout(cardsLayout);

    auto *middleLayout = new QGridLayout;
    middleLayout->setSpacing(12);
    m_keyTagsTable = new QTableWidget(content);
    m_keyTagsTable->setColumnCount(5);
    m_keyTagsTable->setHorizontalHeaderLabels({tr("Tag"), tr("Value"), tr("Quality"), tr("Alarm"), tr("Time")});
    setupTable(m_keyTagsTable);
    m_alarmTable = new QTableWidget(content);
    m_alarmTable->setColumnCount(5);
    m_alarmTable->setHorizontalHeaderLabels({tr("Tag"), tr("Level"), tr("State"), tr("Value"), tr("Message")});
    setupTable(m_alarmTable);
    m_previewMap = new HeatmapWidget(content);
    middleLayout->addWidget(m_keyTagsTable, 0, 0, 2, 1);
    middleLayout->addWidget(m_alarmTable, 0, 1);
    middleLayout->addWidget(m_previewMap, 1, 1);
    layout->addLayout(middleLayout, 1);
    outerLayout->addWidget(scrollPage(content));
}

void DashboardPageWidget::refresh(const UiSnapshot &snapshot)
{
    m_totalTagsLabel->setText(QString::number(snapshot.dashboard.totalTagCount));
    m_badQualityLabel->setText(QString::number(snapshot.dashboard.badQualityCount));
    m_activeAlarmsLabel->setText(QString::number(snapshot.currentAlarms.size()));
    m_matrixLabel->setText(QString::number(snapshot.shell.matrixFrameIndex));
    m_previewMap->setSnapshot(snapshot.measurementMap);

    const QStringList preferredTags = {
        QStringLiteral("MEAS.TEMP.CH01"),
        QStringLiteral("MEAS.PRESSURE.CH01"),
        QStringLiteral("MEAS.VIBRATION.CH01"),
        QStringLiteral("MEAS.POWER.CH01"),
        QStringLiteral("MATRIX.LIGHT.AVG"),
        QStringLiteral("MATRIX.LIGHT.UNIFORMITY")
    };
    QVector<TagRuntimeState> states;
    for (const auto &tagId : preferredTags) {
        const auto it = std::find_if(snapshot.tags.currentValues.cbegin(), snapshot.tags.currentValues.cend(), [&tagId](const auto &state) {
            return state.tagId == tagId;
        });
        if (it != snapshot.tags.currentValues.cend()) {
            states.append(*it);
        }
    }
    m_keyTagsTable->setRowCount(states.size());
    for (auto row = 0; row < states.size(); ++row) {
        const auto &state = states.at(row);
        m_keyTagsTable->setItem(row, 0, item(displayNameFor(snapshot, state.tagId)));
        m_keyTagsTable->setItem(row, 1, item(valueText(state) + (state.unit.has_value() ? QStringLiteral(" %1").arg(state.unit.value()) : QString())));
        m_keyTagsTable->setItem(row, 2, item(Monitor::Domain::Tags::toString(state.quality)));
        m_keyTagsTable->setItem(row, 3, item(Monitor::Domain::Tags::toString(state.alarmState)));
        m_keyTagsTable->setItem(row, 4, item(localTime(state.timestampUtc)));
    }

    m_alarmTable->setRowCount(limitedRowCount(snapshot.currentAlarms.size(), 8));
    for (auto row = 0; row < m_alarmTable->rowCount(); ++row) {
        const auto &alarm = snapshot.currentAlarms.at(row);
        m_alarmTable->setItem(row, 0, item(alarm.tagId));
        m_alarmTable->setItem(row, 1, item(Monitor::Domain::Alarms::toString(alarm.level)));
        m_alarmTable->setItem(row, 2, item(Monitor::Domain::Alarms::toString(alarm.state)));
        m_alarmTable->setItem(row, 3, numericItem(alarm.triggerValue));
        m_alarmTable->setItem(row, 4, item(alarm.message));
    }
}

RealtimeTagsPageWidget::RealtimeTagsPageWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *filterLayout = new QGridLayout;
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter tag id or display name"));
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->addItems({tr("All Categories"), QStringLiteral("Device"), QStringLiteral("Measurement"), QStringLiteral("Electrical"), QStringLiteral("Mechanical"), QStringLiteral("Matrix"), QStringLiteral("Derived"), QStringLiteral("Runtime")});
    filterLayout->addWidget(m_filterEdit, 0, 0);
    filterLayout->addWidget(m_categoryCombo, 0, 1);
    layout->addLayout(filterLayout);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({tr("Tag"), tr("Name"), tr("Category"), tr("Value"), tr("Quality"), tr("Alarm"), tr("Sequence")});
    setupTable(m_table);
    layout->addWidget(m_table, 1);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &RealtimeTagsPageWidget::applyFilters);
    connect(m_categoryCombo, &QComboBox::currentTextChanged, this, &RealtimeTagsPageWidget::applyFilters);
}

void RealtimeTagsPageWidget::refresh(const UiSnapshot &snapshot)
{
    m_snapshot = snapshot;
    applyFilters();
}

void RealtimeTagsPageWidget::applyFilters()
{
    const auto filter = m_filterEdit->text().trimmed();
    const auto category = m_categoryCombo->currentText();
    QVector<TagRuntimeState> rows;
    for (const auto &state : m_snapshot.tags.currentValues) {
        const auto name = displayNameFor(m_snapshot, state.tagId);
        if (!filter.isEmpty() && !state.tagId.contains(filter, Qt::CaseInsensitive) && !name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        const auto categoryText = Monitor::Domain::Tags::toString(state.category);
        if (category != tr("All Categories") && categoryText != category) {
            continue;
        }
        rows.append(state);
    }
    std::sort(rows.begin(), rows.end(), [](const auto &left, const auto &right) {
        return left.tagId < right.tagId;
    });
    m_table->setRowCount(rows.size());
    for (auto row = 0; row < rows.size(); ++row) {
        const auto &state = rows.at(row);
        m_table->setItem(row, 0, item(state.tagId));
        m_table->setItem(row, 1, item(displayNameFor(m_snapshot, state.tagId)));
        m_table->setItem(row, 2, item(Monitor::Domain::Tags::toString(state.category)));
        m_table->setItem(row, 3, item(valueText(state)));
        m_table->setItem(row, 4, item(Monitor::Domain::Tags::toString(state.quality)));
        m_table->setItem(row, 5, item(Monitor::Domain::Tags::toString(state.alarmState)));
        m_table->setItem(row, 6, item(QString::number(state.sequenceNo)));
    }
}

TrendPageWidget::TrendPageWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *controls = new QGridLayout;
    m_tagCombo = new QComboBox(this);
    m_windowCombo = new QComboBox(this);
    m_windowCombo->addItem(tr("1 minute"), 120);
    m_windowCombo->addItem(tr("5 minutes"), 600);
    m_windowCombo->addItem(tr("30 minutes"), 3600);
    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->addItem(tr("Realtime"), static_cast<int>(TrendPointSource::RealtimeBuffer));
    m_sourceCombo->addItem(tr("History"), static_cast<int>(TrendPointSource::HistorySamples));
    auto *exportButton = new QPushButton(tr("Export CSV"), this);
    exportButton->setObjectName(QStringLiteral("secondaryButton"));
    controls->addWidget(m_tagCombo, 0, 0);
    controls->addWidget(m_windowCombo, 0, 1);
    controls->addWidget(m_sourceCombo, 0, 2);
    m_summaryLabel = new QLabel(this);
    controls->addWidget(m_summaryLabel, 0, 3);
    controls->addWidget(exportButton, 0, 4);
    layout->addLayout(controls);
    m_chart = new TrendChartWidget(this);
    layout->addWidget(m_chart);
    m_pointsTable = new QTableWidget(this);
    m_pointsTable->setColumnCount(4);
    m_pointsTable->setHorizontalHeaderLabels({tr("Time"), tr("Value"), tr("Quality"), tr("Diagnostic")});
    setupTable(m_pointsTable);
    layout->addWidget(m_pointsTable, 1);
    connect(m_tagCombo, &QComboBox::currentTextChanged, this, &TrendPageWidget::updateChart);
    connect(m_windowCombo, &QComboBox::currentTextChanged, this, &TrendPageWidget::updateChart);
    connect(m_sourceCombo, &QComboBox::currentTextChanged, this, &TrendPageWidget::updateChart);
    connect(exportButton, &QPushButton::clicked, this, &TrendPageWidget::exportTrendRows);
}

void TrendPageWidget::refresh(const UiSnapshot &snapshot)
{
    m_snapshot = snapshot;
    rebuildTagList(snapshot);
    updateChart();
}

void TrendPageWidget::rebuildTagList(const UiSnapshot &snapshot)
{
    const auto currentTag = m_tagCombo->currentData().toString();
    if (m_tagCombo->count() == snapshot.tagDefinitions.size()) {
        return;
    }
    const QSignalBlocker blocker(m_tagCombo);
    m_tagCombo->clear();
    for (const auto &definition : snapshot.tagDefinitions) {
        if (Monitor::Domain::Tags::isNumeric(definition.dataType)) {
            m_tagCombo->addItem(QStringLiteral("%1 (%2)").arg(definition.displayName, definition.tagId), definition.tagId);
        }
    }
    const auto index = currentTag.isEmpty()
        ? m_tagCombo->findData(QStringLiteral("MEAS.TEMP.CH01"))
        : m_tagCombo->findData(currentTag);
    if (index >= 0) {
        m_tagCombo->setCurrentIndex(index);
    }
}

void TrendPageWidget::updateChart()
{
    const auto tagId = m_tagCombo->currentData().toString();
    const auto pointCount = m_windowCombo->currentData().toInt();
    const auto source = static_cast<TrendPointSource>(m_sourceCombo->currentData().toInt());
    const auto points = trendPointsFor(m_snapshot, tagId, pointCount, source);
    m_chart->setSeries(points, configurationFor(m_snapshot, tagId));
    m_summaryLabel->setText(tr("%1 samples | %2 spikes | %3 bad quality | %4")
                                .arg(points.size())
                                .arg(spikeCount(points))
                                .arg(badQualityCount(points))
                                .arg(m_sourceCombo->currentText()));
    m_pointsTable->setRowCount(limitedRowCount(points.size(), 80));
    for (auto row = 0; row < m_pointsTable->rowCount(); ++row) {
        const auto &point = points.at(points.size() - 1 - row);
        m_pointsTable->setItem(row, 0, item(localTime(point.timestampUtc)));
        m_pointsTable->setItem(row, 1, numericItem(point.value, 3));
        m_pointsTable->setItem(row, 2, item(Monitor::Domain::Tags::toString(point.quality)));
        m_pointsTable->setItem(row, 3, item(point.isSpike ? tr("Spike") : tr("Normal")));
    }
}

void TrendPageWidget::exportTrendRows()
{
    const auto path = QFileDialog::getSaveFileName(this, tr("Export Trend"), QString(), tr("CSV Files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    QVector<QStringList> rows;
    rows.reserve(m_pointsTable->rowCount());
    for (auto row = 0; row < m_pointsTable->rowCount(); ++row) {
        rows.append({
            m_tagCombo->currentData().toString(),
            m_tagCombo->currentText(),
            m_sourceCombo->currentText(),
            m_pointsTable->item(row, 0) ? m_pointsTable->item(row, 0)->text() : QString(),
            m_pointsTable->item(row, 1) ? m_pointsTable->item(row, 1)->text() : QString(),
            m_pointsTable->item(row, 2) ? m_pointsTable->item(row, 2)->text() : QString(),
            m_pointsTable->item(row, 3) ? m_pointsTable->item(row, 3)->text() : QString()
        });
    }

    QString error;
    if (!Monitor::Presentation::Export::CsvExportWriter::write(
            path,
            {QStringLiteral("TagId"), QStringLiteral("Tag"), QStringLiteral("Source"), QStringLiteral("Time"), QStringLiteral("Value"), QStringLiteral("Quality"), QStringLiteral("Diagnostic")},
            rows,
            &error)) {
        QMessageBox::warning(this, tr("Export Trend"), tr("Unable to write CSV file: %1").arg(error));
        return;
    }
    QMessageBox::information(this, tr("Export Trend"), tr("Trend CSV exported."));
}

AlarmCenterPageWidget::AlarmCenterPageWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *ackButton = new QPushButton(tr("Acknowledge Selected"), this);
    ackButton->setObjectName(QStringLiteral("primaryButton"));
    layout->addWidget(ackButton, 0, Qt::AlignLeft);
    m_currentTable = new QTableWidget(this);
    m_currentTable->setColumnCount(6);
    m_currentTable->setHorizontalHeaderLabels({tr("Alarm Id"), tr("Tag"), tr("Level"), tr("State"), tr("Value"), tr("Message")});
    setupTable(m_currentTable);
    layout->addWidget(m_currentTable, 1);
    m_historyFilterEdit = new QLineEdit(this);
    m_historyFilterEdit->setPlaceholderText(tr("Filter alarm history by tag or message"));
    layout->addWidget(m_historyFilterEdit);
    m_historyTable = new QTableWidget(this);
    m_historyTable->setColumnCount(6);
    m_historyTable->setHorizontalHeaderLabels({tr("Time"), tr("Tag"), tr("Level"), tr("State"), tr("Value"), tr("Message")});
    setupTable(m_historyTable);
    layout->addWidget(m_historyTable, 1);
    connect(ackButton, &QPushButton::clicked, this, &AlarmCenterPageWidget::acknowledgeSelected);
    connect(m_historyFilterEdit, &QLineEdit::textChanged, this, &AlarmCenterPageWidget::applyHistoryFilter);
}

void AlarmCenterPageWidget::refresh(const UiSnapshot &snapshot)
{
    m_snapshot = snapshot;
    m_currentTable->setRowCount(snapshot.currentAlarms.size());
    for (auto row = 0; row < snapshot.currentAlarms.size(); ++row) {
        const auto &alarm = snapshot.currentAlarms.at(row);
        m_currentTable->setItem(row, 0, item(alarm.alarmId.toString(QUuid::WithoutBraces)));
        m_currentTable->setItem(row, 1, item(alarm.tagId));
        m_currentTable->setItem(row, 2, item(Monitor::Domain::Alarms::toString(alarm.level)));
        m_currentTable->setItem(row, 3, item(Monitor::Domain::Alarms::toString(alarm.state)));
        m_currentTable->setItem(row, 4, numericItem(alarm.triggerValue));
        m_currentTable->setItem(row, 5, item(alarm.message));
    }
    applyHistoryFilter();
}

void AlarmCenterPageWidget::acknowledgeSelected()
{
    const auto row = m_currentTable->currentRow();
    if (row < 0) {
        return;
    }
    emit acknowledgeRequested(QUuid(QStringLiteral("{%1}").arg(m_currentTable->item(row, 0)->text())));
}

void AlarmCenterPageWidget::applyHistoryFilter()
{
    const auto filter = m_historyFilterEdit->text().trimmed();
    QVector<AlarmEvent> alarms;
    for (const auto &alarm : m_snapshot.alarmHistory) {
        if (!filter.isEmpty() && !alarm.tagId.contains(filter, Qt::CaseInsensitive) && !alarm.message.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        alarms.append(alarm);
    }
    m_historyTable->setRowCount(limitedRowCount(alarms.size(), 100));
    for (auto row = 0; row < m_historyTable->rowCount(); ++row) {
        const auto &alarm = alarms.at(row);
        m_historyTable->setItem(row, 0, item(localTime(alarm.triggerTimeUtc)));
        m_historyTable->setItem(row, 1, item(alarm.tagId));
        m_historyTable->setItem(row, 2, item(Monitor::Domain::Alarms::toString(alarm.level)));
        m_historyTable->setItem(row, 3, item(Monitor::Domain::Alarms::toString(alarm.state)));
        m_historyTable->setItem(row, 4, numericItem(alarm.triggerValue));
        m_historyTable->setItem(row, 5, item(alarm.message));
    }
}

HistoryPageWidget::HistoryPageWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *controls = new QGridLayout;
    m_tagCombo = new QComboBox(this);
    m_limitSpin = new QSpinBox(this);
    m_limitSpin->setRange(10, 1000);
    m_limitSpin->setValue(200);
    auto *exportButton = new QPushButton(tr("Export CSV"), this);
    exportButton->setObjectName(QStringLiteral("secondaryButton"));
    controls->addWidget(m_tagCombo, 0, 0);
    controls->addWidget(m_limitSpin, 0, 1);
    controls->addWidget(exportButton, 0, 2);
    layout->addLayout(controls);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("Tag"), tr("Value"), tr("Quality"), tr("Alarm"), tr("Source")});
    setupTable(m_table);
    layout->addWidget(m_table, 1);
    connect(m_tagCombo, &QComboBox::currentTextChanged, this, &HistoryPageWidget::applyQuery);
    connect(m_limitSpin, qOverload<int>(&QSpinBox::valueChanged), this, &HistoryPageWidget::applyQuery);
    connect(exportButton, &QPushButton::clicked, this, &HistoryPageWidget::exportCurrentRows);
}

void HistoryPageWidget::refresh(const UiSnapshot &snapshot)
{
    m_snapshot = snapshot;
    rebuildTagList(snapshot);
    applyQuery();
}

void HistoryPageWidget::rebuildTagList(const UiSnapshot &snapshot)
{
    if (m_tagCombo->count() == snapshot.tagDefinitions.size() + 1) {
        return;
    }
    const QSignalBlocker blocker(m_tagCombo);
    m_tagCombo->clear();
    m_tagCombo->addItem(tr("All Tags"), QString());
    for (const auto &definition : snapshot.tagDefinitions) {
        m_tagCombo->addItem(QStringLiteral("%1 (%2)").arg(definition.displayName, definition.tagId), definition.tagId);
    }
}

void HistoryPageWidget::applyQuery()
{
    const auto samples = filteredSamples(m_snapshot, m_tagCombo->currentData().toString(), m_limitSpin->value());
    m_table->setRowCount(samples.size());
    for (auto row = 0; row < samples.size(); ++row) {
        const auto &sample = samples.at(row);
        m_table->setItem(row, 0, item(localTime(sample.timestampUtc)));
        m_table->setItem(row, 1, item(sample.tagId));
        m_table->setItem(row, 2, item(sampleValueText(sample)));
        m_table->setItem(row, 3, item(Monitor::Domain::Tags::toString(sample.quality)));
        m_table->setItem(row, 4, item(Monitor::Domain::Tags::toString(sample.alarmState)));
        m_table->setItem(row, 5, item(sample.source));
    }
}

void HistoryPageWidget::exportCurrentRows()
{
    const auto path = QFileDialog::getSaveFileName(this, tr("Export History"), QString(), tr("CSV Files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QVector<QStringList> rows;
    rows.reserve(m_table->rowCount());
    for (auto row = 0; row < m_table->rowCount(); ++row) {
        QStringList fields;
        for (auto column = 0; column < m_table->columnCount(); ++column) {
            fields.append(m_table->item(row, column) ? m_table->item(row, column)->text() : QString());
        }
        rows.append(fields);
    }

    QString error;
    if (!Monitor::Presentation::Export::CsvExportWriter::write(
            path,
            {QStringLiteral("Time"), QStringLiteral("Tag"), QStringLiteral("Value"), QStringLiteral("Quality"), QStringLiteral("Alarm"), QStringLiteral("Source")},
            rows,
            &error)) {
        QMessageBox::warning(this, tr("Export History"), tr("Unable to write CSV file: %1").arg(error));
        return;
    }
    QMessageBox::information(this, tr("Export History"), tr("History CSV exported."));
}

MeasurementMapPageWidget::MeasurementMapPageWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QGridLayout(this);
    auto *controls = new QGridLayout;
    m_scaleModeCombo = new QComboBox(this);
    m_scaleModeCombo->addItem(tr("Auto Range"), 0);
    m_scaleModeCombo->addItem(tr("Fixed Range"), 1);
    m_fixedMinSpin = new QDoubleSpinBox(this);
    m_fixedMinSpin->setRange(-1000000.0, 1000000.0);
    m_fixedMinSpin->setDecimals(2);
    m_fixedMaxSpin = new QDoubleSpinBox(this);
    m_fixedMaxSpin->setRange(-1000000.0, 1000000.0);
    m_fixedMaxSpin->setDecimals(2);
    auto *exportButton = new QPushButton(tr("Export Matrix CSV"), this);
    exportButton->setObjectName(QStringLiteral("secondaryButton"));
    controls->addWidget(new QLabel(tr("Scale"), this), 0, 0);
    controls->addWidget(m_scaleModeCombo, 0, 1);
    controls->addWidget(new QLabel(tr("Min"), this), 0, 2);
    controls->addWidget(m_fixedMinSpin, 0, 3);
    controls->addWidget(new QLabel(tr("Max"), this), 0, 4);
    controls->addWidget(m_fixedMaxSpin, 0, 5);
    controls->addWidget(exportButton, 0, 6);
    m_heatmap = new HeatmapWidget(this);
    m_qualityLabel = new QLabel(this);
    m_rangeLabel = new QLabel(this);
    m_cellLabel = new QLabel(tr("Cell: click heatmap"), this);
    m_hotspotLabel = new QLabel(tr("Hotspot: -"), this);
    m_statsTable = new QTableWidget(this);
    m_statsTable->setColumnCount(2);
    m_statsTable->setHorizontalHeaderLabels({tr("Metric"), tr("Value")});
    setupTable(m_statsTable);
    m_abnormalTable = new QTableWidget(this);
    m_abnormalTable->setColumnCount(5);
    m_abnormalTable->setHorizontalHeaderLabels({tr("Row"), tr("Column"), tr("Value"), tr("Severity"), tr("Message")});
    setupTable(m_abnormalTable);
    layout->addLayout(controls, 0, 0, 1, 2);
    layout->addWidget(m_heatmap, 1, 0, 5, 1);
    layout->addWidget(m_qualityLabel, 1, 1);
    layout->addWidget(m_rangeLabel, 2, 1);
    layout->addWidget(m_cellLabel, 3, 1);
    layout->addWidget(m_hotspotLabel, 4, 1);
    layout->addWidget(m_statsTable, 5, 1);
    layout->addWidget(m_abnormalTable, 6, 0, 1, 2);
    connect(m_heatmap, &HeatmapWidget::cellSelected, this, [this](int row, int column, double value) {
        m_cellLabel->setText(tr("Cell: [%1, %2] = %3").arg(row).arg(column).arg(value, 0, 'f', 2));
    });
    connect(m_scaleModeCombo, &QComboBox::currentTextChanged, this, &MeasurementMapPageWidget::updateHeatmapRenderOptions);
    connect(m_fixedMinSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MeasurementMapPageWidget::updateHeatmapRenderOptions);
    connect(m_fixedMaxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MeasurementMapPageWidget::updateHeatmapRenderOptions);
    connect(exportButton, &QPushButton::clicked, this, &MeasurementMapPageWidget::exportMatrixCsv);
    updateHeatmapRenderOptions();
}

void MeasurementMapPageWidget::refresh(const UiSnapshot &snapshot)
{
    m_snapshot = snapshot;
    m_heatmap->setSnapshot(snapshot.measurementMap);
    if (!snapshot.measurementMap.has_value()) {
        m_qualityLabel->setText(tr("Quality: -"));
        m_rangeLabel->setText(tr("Range: -"));
        m_cellLabel->setText(tr("Cell: -"));
        m_hotspotLabel->setText(tr("Hotspot: -"));
        m_statsTable->setRowCount(0);
        m_abnormalTable->setRowCount(0);
        return;
    }
    const auto &map = snapshot.measurementMap.value();
    m_qualityLabel->setText(tr("Quality: %1").arg(qualityStateText(map.qualityState)));
    m_rangeLabel->setText(tr("Range: %1 - %2 %3").arg(map.scaleRange.minValue, 0, 'f', 1).arg(map.scaleRange.maxValue, 0, 'f', 1).arg(map.unit));
    if (!m_fixedMinSpin->hasFocus() && !m_fixedMaxSpin->hasFocus() && m_scaleModeCombo->currentData().toInt() == 0) {
        const QSignalBlocker minBlocker(m_fixedMinSpin);
        const QSignalBlocker maxBlocker(m_fixedMaxSpin);
        m_fixedMinSpin->setValue(map.scaleRange.minValue);
        m_fixedMaxSpin->setValue(map.scaleRange.maxValue);
    }
    m_hotspotLabel->setText(hotspotText(map));
    updateHeatmapRenderOptions();
    const QVector<QPair<QString, QString>> stats = {
        {tr("Average"), QString::number(map.statistics.averageValue, 'f', 2)},
        {tr("Maximum"), QString::number(map.statistics.maxValue, 'f', 2)},
        {tr("Minimum"), QString::number(map.statistics.minValue, 'f', 2)},
        {tr("Uniformity"), QString::number(map.statistics.uniformity(), 'f', 3)},
        {tr("Abnormal Points"), QString::number(map.abnormalPoints.size())}
    };
    m_statsTable->setRowCount(stats.size());
    for (auto row = 0; row < stats.size(); ++row) {
        m_statsTable->setItem(row, 0, item(stats.at(row).first));
        m_statsTable->setItem(row, 1, item(stats.at(row).second));
    }
    m_abnormalTable->setRowCount(limitedRowCount(map.abnormalPoints.size(), 50));
    for (auto row = 0; row < m_abnormalTable->rowCount(); ++row) {
        const auto &point = map.abnormalPoints.at(row);
        m_abnormalTable->setItem(row, 0, item(QString::number(point.row)));
        m_abnormalTable->setItem(row, 1, item(QString::number(point.column)));
        m_abnormalTable->setItem(row, 2, numericItem(point.value, 2));
        m_abnormalTable->setItem(row, 3, item(matrixSeverityText(point.severity)));
        m_abnormalTable->setItem(row, 4, item(point.message));
    }
}

void MeasurementMapPageWidget::updateHeatmapRenderOptions()
{
    const auto fixed = m_scaleModeCombo && m_scaleModeCombo->currentData().toInt() == 1;
    if (m_fixedMinSpin) {
        m_fixedMinSpin->setEnabled(fixed);
    }
    if (m_fixedMaxSpin) {
        m_fixedMaxSpin->setEnabled(fixed);
    }

    if (!fixed || !m_heatmap || !m_fixedMinSpin || !m_fixedMaxSpin || m_fixedMinSpin->value() >= m_fixedMaxSpin->value()) {
        if (m_heatmap) {
            m_heatmap->setRenderRange(std::nullopt);
        }
        return;
    }

    m_heatmap->setRenderRange(Monitor::Application::Dtos::ScaleRange{m_fixedMinSpin->value(), m_fixedMaxSpin->value()});
}

void MeasurementMapPageWidget::exportMatrixCsv()
{
    if (!m_heatmap || !m_scaleModeCombo || !m_snapshot.measurementMap.has_value()) {
        return;
    }

    const auto path = QFileDialog::getSaveFileName(this, tr("Export Matrix"), QString(), tr("CSV Files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    const auto &map = m_snapshot.measurementMap.value();
    const auto fixed = m_scaleModeCombo->currentData().toInt() == 1 && m_fixedMinSpin->value() < m_fixedMaxSpin->value();
    const auto renderRange = fixed
        ? Monitor::Application::Dtos::ScaleRange{m_fixedMinSpin->value(), m_fixedMaxSpin->value()}
        : map.scaleRange;
    QVector<QStringList> rows;
    rows.reserve(map.cells.size());
    for (const auto &cell : map.cells) {
        const auto renderNormalized = normalizedValue(cell.value, renderRange);
        rows.append({
            localTime(map.timestampUtc),
            QString::number(map.sequenceNo),
            QString::number(cell.row),
            QString::number(cell.column),
            QString::number(cell.value, 'f', 3),
            QString::number(renderNormalized, 'f', 6),
            cell.isAbnormal ? QStringLiteral("True") : QStringLiteral("False"),
            cell.abnormalType.has_value() ? abnormalTypeText(cell.abnormalType.value()) : QString(),
            cell.severity.has_value() ? matrixSeverityText(cell.severity.value()) : QString(),
            cell.tooltipText,
            QString::number(renderRange.minValue, 'f', 3),
            QString::number(renderRange.maxValue, 'f', 3),
            fixed ? QStringLiteral("Fixed") : QStringLiteral("Auto")
        });
    }

    QString error;
    if (!Monitor::Presentation::Export::CsvExportWriter::write(
            path,
            {
                QStringLiteral("Time"),
                QStringLiteral("Frame"),
                QStringLiteral("Row"),
                QStringLiteral("Column"),
                QStringLiteral("Value"),
                QStringLiteral("DisplayNormalized"),
                QStringLiteral("Abnormal"),
                QStringLiteral("AbnormalType"),
                QStringLiteral("Severity"),
                QStringLiteral("Tooltip"),
                QStringLiteral("RangeMin"),
                QStringLiteral("RangeMax"),
                QStringLiteral("RangeMode")
            },
            rows,
            &error)) {
        QMessageBox::warning(this, tr("Export Matrix"), tr("Unable to write CSV file: %1").arg(error));
        return;
    }
    QMessageBox::information(this, tr("Export Matrix"), tr("Matrix CSV exported."));
}

LogsSettingsPageWidget::LogsSettingsPageWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *settingsLayout = new QGridLayout;
    m_generateIntervalSpin = new QSpinBox(this);
    m_generateIntervalSpin->setRange(50, 10000);
    m_refreshIntervalSpin = new QSpinBox(this);
    m_refreshIntervalSpin->setRange(100, 10000);
    m_historyRetentionSpin = new QSpinBox(this);
    m_historyRetentionSpin->setRange(1, 365);
    auto *saveOptionsButton = new QPushButton(tr("Save Runtime Options"), this);
    saveOptionsButton->setObjectName(QStringLiteral("primaryButton"));
    settingsLayout->addWidget(new QLabel(tr("Generate ms"), this), 0, 0);
    settingsLayout->addWidget(m_generateIntervalSpin, 0, 1);
    settingsLayout->addWidget(new QLabel(tr("Refresh ms"), this), 0, 2);
    settingsLayout->addWidget(m_refreshIntervalSpin, 0, 3);
    settingsLayout->addWidget(new QLabel(tr("Retention days"), this), 0, 4);
    settingsLayout->addWidget(m_historyRetentionSpin, 0, 5);
    settingsLayout->addWidget(saveOptionsButton, 0, 6);
    layout->addLayout(settingsLayout);

    m_logTable = new QTableWidget(this);
    m_logTable->setColumnCount(6);
    m_logTable->setHorizontalHeaderLabels({tr("Time"), tr("Level"), tr("Category"), tr("Action"), tr("Message"), tr("Detail")});
    setupTable(m_logTable);
    layout->addWidget(m_logTable, 1);

    auto *saveTagButton = new QPushButton(tr("Save Tag Configurations"), this);
    saveTagButton->setObjectName(QStringLiteral("secondaryButton"));
    layout->addWidget(saveTagButton, 0, Qt::AlignLeft);
    m_configurationTable = new QTableWidget(this);
    m_configurationTable->setColumnCount(8);
    m_configurationTable->setHorizontalHeaderLabels({tr("Tag"), tr("Alarm"), tr("Warn Low"), tr("Alarm Low"), tr("Warn High"), tr("Alarm High"), tr("Historized"), tr("Interval ms")});
    setupTable(m_configurationTable);
    layout->addWidget(m_configurationTable, 1);
    connect(saveOptionsButton, &QPushButton::clicked, this, &LogsSettingsPageWidget::saveRuntimeOptions);
    connect(saveTagButton, &QPushButton::clicked, this, &LogsSettingsPageWidget::saveTagConfigurations);
}

void LogsSettingsPageWidget::refresh(const UiSnapshot &snapshot)
{
    const auto previousConfigurationCount = m_snapshot.tagConfigurations.size();
    m_snapshot = snapshot;
    if (!m_generateIntervalSpin->hasFocus()) {
        m_generateIntervalSpin->setValue(snapshot.runtimeOptions.dataGenerateIntervalMs);
    }
    if (!m_refreshIntervalSpin->hasFocus()) {
        m_refreshIntervalSpin->setValue(snapshot.runtimeOptions.uiRefreshIntervalMs);
    }
    if (!m_historyRetentionSpin->hasFocus()) {
        m_historyRetentionSpin->setValue(snapshot.runtimeOptions.historyRetentionDays);
    }

    m_logTable->setRowCount(limitedRowCount(snapshot.operationLogs.size(), 100));
    for (auto row = 0; row < m_logTable->rowCount(); ++row) {
        const auto &log = snapshot.operationLogs.at(row);
        m_logTable->setItem(row, 0, item(localTime(log.timestampUtc)));
        m_logTable->setItem(row, 1, item(Monitor::Domain::Logs::toString(log.level)));
        m_logTable->setItem(row, 2, item(log.category));
        m_logTable->setItem(row, 3, item(log.action));
        m_logTable->setItem(row, 4, item(log.message));
        m_logTable->setItem(row, 5, item(optionalText(log.detail)));
    }

    if (previousConfigurationCount != snapshot.tagConfigurations.size() || m_configurationTable->rowCount() == 0) {
        populateConfigurationRows();
    }
}

void LogsSettingsPageWidget::saveRuntimeOptions()
{
    auto options = m_snapshot.runtimeOptions;
    options.dataGenerateIntervalMs = m_generateIntervalSpin->value();
    options.uiRefreshIntervalMs = m_refreshIntervalSpin->value();
    options.historyRetentionDays = m_historyRetentionSpin->value();
    emit runtimeOptionsSaveRequested(options);
    QMessageBox::information(this, tr("Settings"), tr("Runtime options saved."));
}

void LogsSettingsPageWidget::saveTagConfigurations()
{
    auto configurations = m_snapshot.tagConfigurations;
    for (auto row = 0; row < m_configurationTable->rowCount() && row < configurations.size(); ++row) {
        configurations[row].alarmEnabled = m_configurationTable->item(row, 1)->checkState() == Qt::Checked;
        configurations[row].warningLow = optionalDoubleFromItem(m_configurationTable->item(row, 2));
        configurations[row].alarmLow = optionalDoubleFromItem(m_configurationTable->item(row, 3));
        configurations[row].warningHigh = optionalDoubleFromItem(m_configurationTable->item(row, 4));
        configurations[row].alarmHigh = optionalDoubleFromItem(m_configurationTable->item(row, 5));
        configurations[row].isHistorized = m_configurationTable->item(row, 6)->checkState() == Qt::Checked;
        configurations[row].historyIntervalMs = intFromItem(m_configurationTable->item(row, 7), configurations[row].historyIntervalMs);
    }
    emit tagConfigurationsSaveRequested(configurations);
    QMessageBox::information(this, tr("Settings"), tr("Tag configurations saved."));
}

void LogsSettingsPageWidget::populateConfigurationRows()
{
    m_configurationTable->setRowCount(m_snapshot.tagConfigurations.size());
    for (auto row = 0; row < m_snapshot.tagConfigurations.size(); ++row) {
        const auto &configuration = m_snapshot.tagConfigurations.at(row);
        auto *tagItem = item(configuration.tagId);
        auto *alarmEnabledItem = item(QString());
        alarmEnabledItem->setCheckState(configuration.alarmEnabled ? Qt::Checked : Qt::Unchecked);
        auto *historizedItem = item(QString());
        historizedItem->setCheckState(configuration.isHistorized ? Qt::Checked : Qt::Unchecked);
        m_configurationTable->setItem(row, 0, tagItem);
        m_configurationTable->setItem(row, 1, alarmEnabledItem);
        m_configurationTable->setItem(row, 2, new QTableWidgetItem(configuration.warningLow.has_value() ? QString::number(configuration.warningLow.value()) : QString()));
        m_configurationTable->setItem(row, 3, new QTableWidgetItem(configuration.alarmLow.has_value() ? QString::number(configuration.alarmLow.value()) : QString()));
        m_configurationTable->setItem(row, 4, new QTableWidgetItem(configuration.warningHigh.has_value() ? QString::number(configuration.warningHigh.value()) : QString()));
        m_configurationTable->setItem(row, 5, new QTableWidgetItem(configuration.alarmHigh.has_value() ? QString::number(configuration.alarmHigh.value()) : QString()));
        m_configurationTable->setItem(row, 6, historizedItem);
        m_configurationTable->setItem(row, 7, new QTableWidgetItem(QString::number(configuration.historyIntervalMs)));
    }
}
