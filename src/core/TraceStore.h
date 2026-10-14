#pragma once

#include "CanFrame.h"

#include <QHash>
#include <QList>

#include <deque>

namespace usbcan {

struct TraceEntry
{
    CanFrame frame;
    // Bit i set when byte i differs from the previous frame with the same ID.
    quint8 changedMask = 0;
    // Unique, increasing number of the frame in this store (survives eviction).
    quint64 seq = 0;
};

struct IdStats
{
    quint32 id = 0;
    bool extended = false;
    quint64 count = 0;
    TraceEntry last;
    double periodMs = 0; // smoothed interval between frames, 0 until two were seen

    double rateHz() const { return periodMs > 0 ? 1000.0 / periodMs : 0.0; }
};

// Bounded chronological frame history plus per-identifier statistics.
// Statistics survive eviction of old frames; clear() resets both.
class TraceStore
{
public:
    explicit TraceStore(qsizetype capacity = 200000);

    qsizetype capacity() const { return m_capacity; }
    qsizetype size() const { return qsizetype(m_entries.size()); }
    const TraceEntry &at(qsizetype i) const { return m_entries[size_t(i)]; } // 0 = oldest

    // Oldest entries that must go before n more fit.
    qsizetype overflowFor(qsizetype n) const;
    void dropOldest(qsizetype n);

    // Evicts the oldest entry when full.
    const TraceEntry &append(const CanFrame &frame);
    void clear();

    // Identifiers in first-seen order.
    const QList<quint64> &keys() const { return m_keys; }
    bool contains(quint64 key) const { return m_stats.contains(key); }
    const IdStats &stats(quint64 key) const { return m_stats.find(key).value(); }

private:
    qsizetype m_capacity;
    quint64 m_nextSeq = 1;
    std::deque<TraceEntry> m_entries;
    QList<quint64> m_keys;
    QHash<quint64, IdStats> m_stats;
};

} // namespace usbcan
