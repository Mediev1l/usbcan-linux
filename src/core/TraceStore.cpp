#include "TraceStore.h"

#include <algorithm>

namespace usbcan {

namespace {

constexpr double kSmoothing = 0.2; // weight of the newest interval

quint8 diffMask(const CanFrame &previous, const CanFrame &current)
{
    quint8 mask = 0;
    const int n = std::max(previous.dlc, current.dlc);
    for (int i = 0; i < n; ++i) {
        if (i >= previous.dlc || i >= current.dlc || previous.data[i] != current.data[i])
            mask |= quint8(1u << i);
    }
    return mask;
}

} // namespace

TraceStore::TraceStore(qsizetype capacity)
    : m_capacity(std::max<qsizetype>(1, capacity))
{
}

qsizetype TraceStore::overflowFor(qsizetype n) const
{
    return std::clamp<qsizetype>(size() + n - m_capacity, 0, size());
}

void TraceStore::dropOldest(qsizetype n)
{
    n = std::clamp<qsizetype>(n, 0, size());
    m_entries.erase(m_entries.begin(), m_entries.begin() + n);
}

const TraceEntry &TraceStore::append(const CanFrame &frame)
{
    const quint64 key = frameKey(frame.id, frame.extended);
    auto it = m_stats.find(key);
    TraceEntry entry{frame, 0, m_nextSeq++};

    if (it == m_stats.end()) {
        IdStats s;
        s.id = frame.id;
        s.extended = frame.extended;
        it = m_stats.insert(key, s);
        m_keys.append(key);
    } else {
        IdStats &s = it.value();
        entry.changedMask = diffMask(s.last.frame, frame);
        const double interval = double(frame.timestampUs - s.last.frame.timestampUs) / 1000.0;
        s.periodMs = s.count == 1 ? interval : (1 - kSmoothing) * s.periodMs + kSmoothing * interval;
    }
    IdStats &s = it.value();
    s.count += 1;
    s.last = entry;

    if (size() >= m_capacity)
        m_entries.pop_front();
    m_entries.push_back(entry);
    return m_entries.back();
}

void TraceStore::clear()
{
    m_entries.clear();
    m_keys.clear();
    m_stats.clear();
}

} // namespace usbcan
