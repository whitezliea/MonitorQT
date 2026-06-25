#ifndef BATCHPERSISTWORKER_H
#define BATCHPERSISTWORKER_H

#include "application/queues/BlockingQueue.h"

#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QVector>

#include <atomic>
#include <functional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace Monitor::Application::Workers {

enum class PersistWorkerState {
    Stopped,
    Running,
    Degraded,
    Faulted
};

struct PersistWorkerStatus
{
    PersistWorkerState state = PersistWorkerState::Stopped;
    QDateTime lastSuccessfulFlushUtc;
    QString lastError;
};

class IPersistWorker
{
public:
    virtual ~IPersistWorker() = default;

    virtual QString name() const = 0;
    virtual PersistWorkerStatus status() const = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool flush() = 0;
    virtual bool isRunning() const = 0;
};

template <typename T>
class BatchPersistWorker : public IPersistWorker
{
public:
    using Queue = Monitor::Application::Queues::BlockingQueue<T>;
    using PersistCallback = std::function<void(const QVector<T> &)>;

    BatchPersistWorker(
        QString name,
        Queue *queue,
        int batchIntervalMs,
        int maxBatchSize,
        PersistCallback persist)
        : m_name(std::move(name)),
          m_queue(queue),
          m_batchIntervalMs(batchIntervalMs),
          m_maxBatchSize(maxBatchSize),
          m_persist(std::move(persist))
    {
        if (m_name.trimmed().isEmpty()) {
            throw std::invalid_argument("Worker name must not be empty.");
        }

        if (!m_queue) {
            throw std::invalid_argument("Worker queue must not be null.");
        }

        if (m_batchIntervalMs <= 0) {
            throw std::out_of_range("Batch interval must be greater than zero.");
        }

        if (m_maxBatchSize <= 0) {
            throw std::out_of_range("Max batch size must be greater than zero.");
        }

        if (!m_persist) {
            throw std::invalid_argument("Persist callback must not be empty.");
        }
    }

    ~BatchPersistWorker()
    {
        stop();
    }

    BatchPersistWorker(const BatchPersistWorker &) = delete;
    BatchPersistWorker &operator=(const BatchPersistWorker &) = delete;

    QString name() const override
    {
        return m_name;
    }

    PersistWorkerStatus status() const override
    {
        QMutexLocker locker(&m_statusMutex);
        return m_status;
    }

    bool start() override
    {
        auto expected = false;
        if (!m_running.compare_exchange_strong(expected, true)) {
            return false;
        }

        m_stopRequested.store(false);
        m_queue->reopen();
        setStatus({PersistWorkerState::Running, m_status.lastSuccessfulFlushUtc, {}});
        m_thread = std::thread([this]() {
            runLoop();
        });
        return true;
    }

    bool stop() override
    {
        if (!m_running.load()) {
            return false;
        }

        m_stopRequested.store(true);
        m_queue->close();
        if (m_thread.joinable()) {
            m_thread.join();
        }

        m_running.store(false);
        if (status().state != PersistWorkerState::Faulted) {
            setStatus({PersistWorkerState::Stopped, status().lastSuccessfulFlushUtc, {}});
        }
        return true;
    }

    bool isRunning() const override
    {
        return m_running.load();
    }

    bool flush() override
    {
        drainQueueToBuffer();
        return persistBuffered(QStringLiteral("Explicit"));
    }

private:
    void runLoop()
    {
        try {
            while (!m_stopRequested.load()) {
                T item;
                const auto result = m_queue->dequeueFor(&item, m_batchIntervalMs);
                if (result == Monitor::Application::Queues::QueueTakeResult::Item) {
                    appendToBuffer(item);
                    drainQueueToBuffer();
                    if (bufferSize() >= m_maxBatchSize) {
                        persistBuffered(QStringLiteral("BatchSize"));
                    }
                    continue;
                }

                if (result == Monitor::Application::Queues::QueueTakeResult::Timeout) {
                    persistBuffered(QStringLiteral("Interval"));
                    continue;
                }

                break;
            }

            drainQueueToBuffer();
            persistBuffered(QStringLiteral("Shutdown"));
        } catch (const std::exception &exception) {
            setStatus({PersistWorkerState::Faulted, status().lastSuccessfulFlushUtc, QString::fromUtf8(exception.what())});
        } catch (...) {
            setStatus({PersistWorkerState::Faulted, status().lastSuccessfulFlushUtc, QStringLiteral("Unknown worker failure.")});
        }
    }

    void appendToBuffer(const T &item)
    {
        QMutexLocker locker(&m_bufferMutex);
        m_buffer.append(item);
    }

    void drainQueueToBuffer()
    {
        const auto drained = m_queue->drain();
        if (drained.isEmpty()) {
            return;
        }

        QMutexLocker locker(&m_bufferMutex);
        m_buffer += drained;
    }

    int bufferSize() const
    {
        QMutexLocker locker(&m_bufferMutex);
        return m_buffer.size();
    }

    bool persistBuffered(const QString &)
    {
        QMutexLocker persistLocker(&m_persistMutex);
        QVector<T> batch;
        {
            QMutexLocker bufferLocker(&m_bufferMutex);
            if (m_buffer.isEmpty()) {
                return true;
            }
            batch.swap(m_buffer);
        }

        if (batch.isEmpty()) {
            return true;
        }

        try {
            m_persist(batch);
            setStatus({PersistWorkerState::Running, QDateTime::currentDateTimeUtc(), {}});
            return true;
        } catch (const std::exception &exception) {
            QMutexLocker bufferLocker(&m_bufferMutex);
            batch += m_buffer;
            m_buffer = batch;
            setStatus({PersistWorkerState::Degraded, status().lastSuccessfulFlushUtc, QString::fromUtf8(exception.what())});
            return false;
        } catch (...) {
            QMutexLocker bufferLocker(&m_bufferMutex);
            batch += m_buffer;
            m_buffer = batch;
            setStatus({PersistWorkerState::Degraded, status().lastSuccessfulFlushUtc, QStringLiteral("Unknown persistence failure.")});
            return false;
        }
    }

    void setStatus(const PersistWorkerStatus &status)
    {
        QMutexLocker locker(&m_statusMutex);
        m_status = status;
    }

    QString m_name;
    Queue *m_queue = nullptr;
    int m_batchIntervalMs = 0;
    int m_maxBatchSize = 0;
    PersistCallback m_persist;
    std::atomic_bool m_running = false;
    std::atomic_bool m_stopRequested = false;
    std::thread m_thread;
    mutable QMutex m_statusMutex;
    mutable QMutex m_bufferMutex;
    QMutex m_persistMutex;
    QVector<T> m_buffer;
    PersistWorkerStatus m_status;
};

} // namespace Monitor::Application::Workers

#endif // BATCHPERSISTWORKER_H
