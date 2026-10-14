#include "TraceModel.h"

#include "J1939.h"

#include <QDateTime>
#include <QSet>
#include <QVariantMap>

namespace usbcan {

namespace {

QString hex(quint32 value, int width)
{
    return QStringLiteral("%1").arg(value, width, 16, QLatin1Char('0')).toUpper();
}

QString timeText(qint64 us)
{
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(us / 1000);
    return t.toString(QStringLiteral("HH:mm:ss.zzz"));
}

QVariantList idFields(const CanFrame &f)
{
    QVariantList list;
    if (!f.extended) {
        list.append(QVariantMap{{QStringLiteral("label"), QStringLiteral("ID (11 bit)")},
                                {QStringLiteral("bits"), QStringLiteral("%1").arg(f.id, 11, 2, QLatin1Char('0'))},
                                {QStringLiteral("value"), hex(f.id, 3)}});
        return list;
    }
    for (const j1939::BitField &b : j1939::idBitFields(f.id)) {
        list.append(QVariantMap{{QStringLiteral("label"), b.label},
                                {QStringLiteral("bits"), b.bits},
                                {QStringLiteral("value"), hex(b.value, 2)}});
    }
    return list;
}

QVariantList bytes(const TraceEntry &e)
{
    QVariantList list;
    for (int i = 0; i < e.frame.dlc; ++i) {
        const quint8 b = e.frame.data[i];
        list.append(QVariantMap{{QStringLiteral("index"), i},
                                {QStringLiteral("hex"), hex(b, 2)},
                                {QStringLiteral("dec"), int(b)},
                                {QStringLiteral("bin"), QStringLiteral("%1").arg(b, 8, 2, QLatin1Char('0'))},
                                {QStringLiteral("changed"), bool(e.changedMask & (1u << i))}});
    }
    return list;
}

} // namespace

TraceModel::TraceModel(qsizetype capacity, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(capacity)
{
}

int TraceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return int(m_mode == Chronological ? m_store.size() : m_store.keys().size());
}

const TraceEntry &TraceModel::entryAt(int row) const
{
    if (m_mode == Chronological)
        return m_store.at(row);
    return m_store.stats(m_store.keys().at(row)).last;
}

QVariant TraceModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const TraceEntry &e = entryAt(index.row());
    const CanFrame &f = e.frame;
    const quint64 key = frameKey(f.id, f.extended);
    const j1939::Id j = j1939::decode(f.id);

    switch (role) {
    case Qt::DisplayRole:
    case IdRole:
        return formatId(f.id, f.extended);
    case KeyRole:
        return key;
    case SeqRole:
        return e.seq;
    case TimeRole:
        return timeText(f.timestampUs);
    case DirectionRole:
        return f.dir == Direction::Tx ? QStringLiteral("TX") : QStringLiteral("RX");
    case ExtendedRole:
        return f.extended;
    case RtrRole:
        return f.rtr;
    case NameRole:
        return m_nameLookup ? m_nameLookup(f.id, f.extended) : QString();
    case DlcRole:
        return f.dlc;
    case PayloadRole:
        return f.rtr ? QStringLiteral("remote request") : formatPayload(f);
    case ChangedMaskRole:
        return e.changedMask;
    case CountRole:
        return QVariant::fromValue(m_store.stats(key).count);
    case PeriodRole:
        return m_store.stats(key).periodMs;
    case IsJ1939Role:
        return f.extended;
    case PgnRole:
        return f.extended ? hex(j.pgn, 4) : QString();
    case PgnDecimalRole:
        return f.extended ? int(j.pgn) : 0;
    case PgnNameRole:
        return f.extended ? j1939::pgnName(j.pgn) : QString();
    case PriorityRole:
        return f.extended ? int(j.priority) : 0;
    case SourceRole:
        return f.extended ? hex(j.sa, 2) : QString();
    case DestinationRole:
        if (!f.extended)
            return QString();
        return j.broadcast ? QStringLiteral("FF (global)") : hex(j.da, 2);
    case PduFormatRole:
        return f.extended ? j1939::pduFormatName(j) : QString();
    case IdFieldsRole:
        return idFields(f);
    case BytesRole:
        return bytes(e);
    default:
        return {};
    }
}

QHash<int, QByteArray> TraceModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {SeqRole, "seq"},
        {TimeRole, "time"},
        {DirectionRole, "direction"},
        {IdRole, "canId"},
        {ExtendedRole, "extended"},
        {RtrRole, "rtr"},
        {NameRole, "name"},
        {DlcRole, "dlc"},
        {PayloadRole, "payload"},
        {ChangedMaskRole, "changedMask"},
        {CountRole, "count"},
        {PeriodRole, "periodMs"},
        {IsJ1939Role, "isJ1939"},
        {PgnRole, "pgn"},
        {PgnDecimalRole, "pgnDecimal"},
        {PgnNameRole, "pgnName"},
        {PriorityRole, "priority"},
        {SourceRole, "sourceAddress"},
        {DestinationRole, "destinationAddress"},
        {PduFormatRole, "pduFormat"},
        {IdFieldsRole, "idFields"},
        {BytesRole, "bytes"},
    };
}

void TraceModel::appendFrames(const QList<CanFrame> &frames)
{
    if (frames.isEmpty())
        return;

    if (m_mode == LatestPerId) {
        const int before = int(m_store.keys().size());
        QSet<quint64> fresh;
        for (const CanFrame &f : frames) {
            const quint64 key = frameKey(f.id, f.extended);
            if (!m_store.contains(key))
                fresh.insert(key);
        }
        if (!fresh.isEmpty())
            beginInsertRows({}, before, before + int(fresh.size()) - 1);
        for (const CanFrame &f : frames)
            m_store.append(f);
        if (!fresh.isEmpty())
            endInsertRows();
        if (before > 0)
            emit dataChanged(index(0), index(before - 1));
        return;
    }

    if (frames.size() > m_store.capacity()) {
        beginResetModel();
        for (const CanFrame &f : frames)
            m_store.append(f);
        endResetModel();
        return;
    }

    const qsizetype overflow = m_store.overflowFor(frames.size());
    if (overflow > 0) {
        beginRemoveRows({}, 0, int(overflow) - 1);
        m_store.dropOldest(overflow);
        endRemoveRows();
    }
    const int first = int(m_store.size());
    beginInsertRows({}, first, first + int(frames.size()) - 1);
    for (const CanFrame &f : frames)
        m_store.append(f);
    endInsertRows();
}

void TraceModel::clear()
{
    beginResetModel();
    m_store.clear();
    endResetModel();
}

void TraceModel::setMode(int mode)
{
    const Mode m = mode == LatestPerId ? LatestPerId : Chronological;
    if (m == m_mode)
        return;
    beginResetModel();
    m_mode = m;
    endResetModel();
    emit modeChanged();
}

CanFrame TraceModel::frameAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return {};
    return entryAt(row).frame;
}

QList<CanFrame> TraceModel::frames() const
{
    QList<CanFrame> list;
    list.reserve(m_store.size());
    for (qsizetype i = 0; i < m_store.size(); ++i)
        list.append(m_store.at(i).frame);
    return list;
}

void TraceModel::setNameLookup(IdNameLookup lookup)
{
    m_nameLookup = std::move(lookup);
    refreshNames();
}

void TraceModel::refreshNames()
{
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {NameRole});
}

} // namespace usbcan
