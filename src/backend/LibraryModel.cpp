#include "LibraryModel.h"

#include "HexInput.h"

namespace usbcan {

LibraryModel::LibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_library.size());
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const SavedMessage &m = m_library.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return m.name;
    case HandleRole:
        return m_handles.at(index.row());
    case IdRole:
        return formatId(m.id, m.extended);
    case ExtendedRole:
        return m.extended;
    case PayloadRole:
        return formatPayload(m.payload);
    case PeriodRole:
        return m.periodMs;
    case CommentRole:
        return m.comment;
    case RunningRole:
        return m_running.contains(m_handles.at(index.row()));
    default:
        return {};
    }
}

bool LibraryModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return false;
    SavedMessage m = m_library.at(index.row());
    switch (role) {
    case NameRole:
        m.name = value.toString().trimmed();
        break;
    case IdRole: {
        const auto id = hexinput::parseId(value.toString(), m.extended);
        if (!id.ok)
            return false;
        m.id = id.id;
        break;
    }
    case ExtendedRole:
        if (!value.toBool() && m.id > kMaxStandardId)
            return false;
        m.extended = value.toBool();
        break;
    case PayloadRole: {
        const auto payload = hexinput::parsePayload(value.toString());
        if (!payload.ok)
            return false;
        m.payload = payload.bytes;
        break;
    }
    case PeriodRole:
        m.periodMs = std::max(0, value.toInt());
        break;
    case CommentRole:
        m.comment = value.toString();
        break;
    default:
        return false;
    }
    if (m == m_library.at(index.row()))
        return true;
    m_library.replace(index.row(), m);
    emit dataChanged(index, index);
    emit libraryChanged();
    return true;
}

Qt::ItemFlags LibraryModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {
        {HandleRole, "handle"},   {NameRole, "name"},       {IdRole, "canId"},
        {ExtendedRole, "extended"}, {PayloadRole, "payload"}, {PeriodRole, "periodMs"},
        {CommentRole, "comment"}, {RunningRole, "running"},
    };
}

void LibraryModel::setLibrary(const MessageLibrary &library)
{
    beginResetModel();
    m_library = library;
    m_handles.clear();
    m_running.clear();
    for (qsizetype i = 0; i < m_library.size(); ++i)
        m_handles.append(m_nextHandle++);
    endResetModel();
    emit countChanged();
    emit libraryChanged();
}

int LibraryModel::add(const SavedMessage &message)
{
    const int row = rowCount();
    beginInsertRows({}, row, row);
    m_library.add(message);
    m_handles.append(m_nextHandle++);
    endInsertRows();
    emit countChanged();
    emit libraryChanged();
    return row;
}

void LibraryModel::remove(int row)
{
    if (row < 0 || row >= rowCount())
        return;
    beginRemoveRows({}, row, row);
    m_library.remove(row);
    m_running.remove(m_handles.takeAt(row));
    endRemoveRows();
    emit countChanged();
    emit libraryChanged();
}

void LibraryModel::move(int from, int to)
{
    if (from < 0 || from >= rowCount() || to < 0 || to >= rowCount() || from == to)
        return;
    // beginMoveRows wants the destination as "insert before" in the old layout.
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    MessageLibrary lib;
    QList<SavedMessage> list = m_library.messages();
    list.move(from, to);
    for (const SavedMessage &m : list)
        lib.add(m);
    m_library = lib;
    m_handles.move(from, to);
    endMoveRows();
    emit libraryChanged();
}

int LibraryModel::rowOfHandle(quint64 handle) const
{
    return int(m_handles.indexOf(handle));
}

void LibraryModel::setRunning(quint64 handle, bool running)
{
    const int row = rowOfHandle(handle);
    if (row < 0)
        return;
    if (running)
        m_running.insert(handle);
    else
        m_running.remove(handle);
    emit dataChanged(index(row), index(row), {RunningRole});
}

} // namespace usbcan
