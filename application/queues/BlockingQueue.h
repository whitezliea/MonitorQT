#ifndef BLOCKINGQUEUE_H
#define BLOCKINGQUEUE_H

#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QWaitCondition>
#include <QVector>

#include <stdexcept>
#include <utility>

namespace Monitor::Application::Queues {

enum class QueueTakeResult {
    Item,
    Timeout,
    Closed
};

template <typename T>
class BlockingQueue
{
public:
    void enqueue(const T &item)
    {
        QMutexLocker locker(&m_mutex);
        if (m_closed) {
            throw std::runtime_error("Cannot enqueue into a closed queue.");
        }

        m_queue.enqueue(item);
        m_notEmpty.wakeOne();
    }

    void enqueue(T &&item)
    {
        QMutexLocker locker(&m_mutex);
        if (m_closed) {
            throw std::runtime_error("Cannot enqueue into a closed queue.");
        }

        m_queue.enqueue(std::move(item));
        m_notEmpty.wakeOne();
    }

    QueueTakeResult dequeueFor(T *item, int timeoutMs)
    {
        QMutexLocker locker(&m_mutex);
        if (timeoutMs < 0) {
            while (m_queue.isEmpty() && !m_closed) {
                m_notEmpty.wait(&m_mutex);
            }
        } else if (m_queue.isEmpty() && !m_closed) {
            m_notEmpty.wait(&m_mutex, static_cast<unsigned long>(timeoutMs));
        }

        if (!m_queue.isEmpty()) {
            if (item) {
                *item = m_queue.dequeue();
            } else {
                m_queue.dequeue();
            }
            return QueueTakeResult::Item;
        }

        return m_closed ? QueueTakeResult::Closed : QueueTakeResult::Timeout;
    }

    bool tryDequeue(T *item)
    {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty()) {
            return false;
        }

        if (item) {
            *item = m_queue.dequeue();
        } else {
            m_queue.dequeue();
        }
        return true;
    }

    QVector<T> drain()
    {
        QMutexLocker locker(&m_mutex);
        QVector<T> result;
        result.reserve(m_queue.size());
        while (!m_queue.isEmpty()) {
            result.append(m_queue.dequeue());
        }
        return result;
    }

    void close()
    {
        QMutexLocker locker(&m_mutex);
        m_closed = true;
        m_notEmpty.wakeAll();
    }

    void reopen()
    {
        QMutexLocker locker(&m_mutex);
        m_closed = false;
    }

    bool isClosed() const
    {
        QMutexLocker locker(&m_mutex);
        return m_closed;
    }

    int size() const
    {
        QMutexLocker locker(&m_mutex);
        return m_queue.size();
    }

private:
    mutable QMutex m_mutex;
    QWaitCondition m_notEmpty;
    QQueue<T> m_queue;
    bool m_closed = false;
};

} // namespace Monitor::Application::Queues

#endif // BLOCKINGQUEUE_H
