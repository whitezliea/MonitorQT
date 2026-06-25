#ifndef APPLICATIONQUEUES_H
#define APPLICATIONQUEUES_H

#include "application/queues/BlockingQueue.h"
#include "domain/alarms/AlarmModels.h"
#include "domain/logs/LogModels.h"
#include "domain/measurements/MeasurementModels.h"
#include "domain/tags/TagModels.h"

namespace Monitor::Application::Queues {

using HistorySampleQueue = BlockingQueue<Monitor::Domain::Tags::TagValue>;
using AlarmEventQueue = BlockingQueue<Monitor::Domain::Alarms::AlarmEvent>;
using OperationLogQueue = BlockingQueue<Monitor::Domain::Logs::OperationLog>;
using MatrixFrameQueue = BlockingQueue<Monitor::Domain::Measurements::MatrixFrame>;

} // namespace Monitor::Application::Queues

#endif // APPLICATIONQUEUES_H
