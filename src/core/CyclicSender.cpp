#include "CyclicSender.h"

namespace usbcan {

void CyclicSender::start(quint64 handle, const CanFrame &frame, int periodMs)
{
    const bool wasRunning = isRunning(handle);
    m_jobs.erase(handle);
    emit sendRequested(frame);
    if (periodMs <= 0) {
        if (wasRunning)
            emit runningChanged(handle, false);
        return;
    }

    Job job;
    job.frame = frame;
    job.timer = std::make_unique<QTimer>();
    job.timer->setTimerType(Qt::PreciseTimer);
    job.timer->setInterval(periodMs);
    connect(job.timer.get(), &QTimer::timeout, this, [this, handle] {
        emit sendRequested(m_jobs.at(handle).frame);
    });
    job.timer->start();
    m_jobs.emplace(handle, std::move(job));
    if (!wasRunning)
        emit runningChanged(handle, true);
}

void CyclicSender::update(quint64 handle, const CanFrame &frame, int periodMs)
{
    auto it = m_jobs.find(handle);
    if (it == m_jobs.end())
        return;
    if (periodMs <= 0) {
        stop(handle);
        return;
    }
    it->second.frame = frame;
    it->second.timer->setInterval(periodMs);
}

void CyclicSender::stop(quint64 handle)
{
    if (m_jobs.erase(handle) > 0)
        emit runningChanged(handle, false);
}

void CyclicSender::stopAll()
{
    while (!m_jobs.empty())
        stop(m_jobs.begin()->first);
}

} // namespace usbcan
