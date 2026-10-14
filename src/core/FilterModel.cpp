#include "FilterModel.h"

#include "J1939.h"

namespace usbcan {

FilterModel::FilterModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FilterModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant FilterModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Entry &e = m_entries[index.row()];
    const quint32 id = keyId(e.key);
    const bool extended = keyExtended(e.key);
    const j1939::Id j = j1939::decode(id);
    const QString name = m_nameLookup ? m_nameLookup(id, extended) : QString();

    switch (role) {
    case Qt::DisplayRole:
    case IdRole:
        return formatId(id, extended);
    case KeyRole:
        return e.key;
    case Qt::CheckStateRole:
        return m_hidden.contains(e.key) ? Qt::Unchecked : Qt::Checked;
    case CheckedRole:
        return !m_hidden.contains(e.key);
    case ExtendedRole:
        return extended;
    case NameRole:
        return name;
    case PgnRole:
        return extended ? QStringLiteral("%1").arg(j.pgn, 4, 16, QLatin1Char('0')).toUpper() : QString();
    case PgnNameRole:
        return extended ? j1939::pgnName(j.pgn) : QString();
    case SourceRole:
        return extended ? QStringLiteral("%1").arg(j.sa, 2, 16, QLatin1Char('0')).toUpper() : QString();
    case CountRole:
        return QVariant::fromValue(e.count);
    case RateRole:
        return e.rateHz;
    case SearchRole:
        return QStringList{formatId(id, extended), name,
                           data(index, PgnRole).toString(), data(index, PgnNameRole).toString()}
            .join(QLatin1Char(' '));
    default:
        return {};
    }
}

bool FilterModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return false;
    bool visible = false;
    if (role == CheckedRole)
        visible = value.toBool();
    else if (role == Qt::CheckStateRole)
        visible = value.toInt() == Qt::Checked;
    else
        return false;
    setVisible(m_entries[index.row()].key, visible);
    return true;
}

Qt::ItemFlags FilterModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> FilterModel::roleNames() const
{
    return {
        {KeyRole, "key"},         {CheckedRole, "checked"}, {IdRole, "canId"},
        {ExtendedRole, "extended"}, {NameRole, "name"},     {PgnRole, "pgn"},
        {PgnNameRole, "pgnName"}, {SourceRole, "sourceAddress"}, {CountRole, "count"},
        {RateRole, "rateHz"},     {SearchRole, "search"},
    };
}

void FilterModel::sync(const TraceStore &store, const QList<CanFrame> &received)
{
    QList<quint64> fresh;
    for (const CanFrame &f : received) {
        const quint64 key = frameKey(f.id, f.extended);
        if (!m_rows.contains(key) && !fresh.contains(key))
            fresh.append(key);
    }

    if (!fresh.isEmpty()) {
        const int first = int(m_entries.size());
        beginInsertRows({}, first, first + int(fresh.size()) - 1);
        for (quint64 key : fresh) {
            m_rows.insert(key, int(m_entries.size()));
            m_entries.append({key, 0, 0});
            if (!m_newIdsVisible)
                m_hidden.insert(key);
        }
        endInsertRows();
        emit countChanged();
        if (!m_newIdsVisible)
            emit filterChanged();
    }

    for (Entry &e : m_entries) {
        if (!store.contains(e.key))
            continue;
        const IdStats &s = store.stats(e.key);
        e.count = s.count;
        e.rateHz = s.rateHz();
    }
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(rowCount() - 1), {CountRole, RateRole});
}

void FilterModel::clear()
{
    beginResetModel();
    m_entries.clear();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

bool FilterModel::accepts(quint64 key) const
{
    if (m_rows.contains(key) || m_hidden.contains(key))
        return !m_hidden.contains(key);
    return m_newIdsVisible;
}

void FilterModel::setVisible(quint64 key, bool visible)
{
    if (visible == !m_hidden.contains(key))
        return;
    if (visible)
        m_hidden.remove(key);
    else
        m_hidden.insert(key);
    const int row = m_rows.value(key, -1);
    if (row >= 0)
        emitChecked(row, row);
    emit filterChanged();
}

void FilterModel::setAll(bool visible)
{
    for (const Entry &e : std::as_const(m_entries)) {
        if (visible)
            m_hidden.remove(e.key);
        else
            m_hidden.insert(e.key);
    }
    emitChecked(0, rowCount() - 1);
    emit filterChanged();
}

void FilterModel::invert()
{
    for (const Entry &e : std::as_const(m_entries)) {
        if (m_hidden.contains(e.key))
            m_hidden.remove(e.key);
        else
            m_hidden.insert(e.key);
    }
    emitChecked(0, rowCount() - 1);
    emit filterChanged();
}

void FilterModel::showOnly(int row)
{
    if (row < 0 || row >= rowCount())
        return;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (i == row)
            m_hidden.remove(m_entries[i].key);
        else
            m_hidden.insert(m_entries[i].key);
    }
    emitChecked(0, rowCount() - 1);
    emit filterChanged();
}

void FilterModel::setNewIdsVisible(bool visible)
{
    if (visible == m_newIdsVisible)
        return;
    m_newIdsVisible = visible;
    emit newIdsVisibleChanged();
}

int FilterModel::hiddenCount() const
{
    int n = 0;
    for (const Entry &e : m_entries)
        n += m_hidden.contains(e.key) ? 1 : 0;
    return n;
}

void FilterModel::setHiddenKeys(const QSet<quint64> &keys)
{
    m_hidden = keys;
    emitChecked(0, rowCount() - 1);
    emit filterChanged();
}

void FilterModel::setNameLookup(IdNameLookup lookup)
{
    m_nameLookup = std::move(lookup);
    refreshNames();
}

void FilterModel::refreshNames()
{
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {NameRole, SearchRole});
}

void FilterModel::emitChecked(int first, int last)
{
    if (last >= first)
        emit dataChanged(index(first), index(last), {CheckedRole, Qt::CheckStateRole});
}

} // namespace usbcan
