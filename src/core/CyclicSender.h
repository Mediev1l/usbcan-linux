#pragma once

#include "CanFrame.h"

#include <QObject>
#include <QTimer>

#include <map>
#include <memory>

namespace usbcan {

// Periodic transmission of frames; each job is identified by a caller-chosen handle.
class CyclicSender : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Sends the frame now and then every periodMs; restarts an existing job.
    // periodMs <= 0 sends once and starts nothing.
    void start(quint64 handle, const CanFrame &frame, int periodMs);
    // Changes frame/period of a running job without restarting its phase.
    void update(quint64 handle, const CanFrame &frame, int periodMs);
    void stop(quint64 handle);
    void stopAll();

    bool isRunning(quint64 handle) const { return m_jobs.count(handle) > 0; }
    int runningCount() const { return int(m_jobs.size()); }

signals:
    void sendRequested(const usbcan::CanFrame &frame);
    void runningChanged(quint64 handle, bool running);

private:
    struct Job
    {
        std::unique_ptr<QTimer> timer;
        CanFrame frame;
    };
    std::map<quint64, Job> m_jobs;
};

} // namespace usbcan
