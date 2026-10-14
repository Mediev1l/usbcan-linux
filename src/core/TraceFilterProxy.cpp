#include "TraceFilterProxy.h"

#include "TraceModel.h"

namespace usbcan {

TraceFilterProxy::TraceFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved})
        connect(this, signal, this, &TraceFilterProxy::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &TraceFilterProxy::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &TraceFilterProxy::countChanged);
}

void TraceFilterProxy::setFilterModel(FilterModel *filters)
{
    if (m_filters)
        m_filters->disconnect(this);
    m_filters = filters;
    if (m_filters)
        connect(m_filters, &FilterModel::filterChanged, this, &TraceFilterProxy::invalidateFilter);
    invalidateFilter();
}

void TraceFilterProxy::setShowRx(bool show)
{
    if (show == m_showRx)
        return;
    m_showRx = show;
    invalidateFilter();
    emit showRxChanged();
}

void TraceFilterProxy::setShowTx(bool show)
{
    if (show == m_showTx)
        return;
    m_showTx = show;
    invalidateFilter();
    emit showTxChanged();
}

void TraceFilterProxy::setSearchText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed == m_search)
        return;
    m_search = trimmed;
    invalidateFilter();
    emit searchTextChanged();
}

int TraceFilterProxy::sourceRow(int proxyRow) const
{
    const QModelIndex i = index(proxyRow, 0);
    return i.isValid() ? mapToSource(i).row() : -1;
}

bool TraceFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex i = sourceModel()->index(sourceRow, 0, sourceParent);
    if (m_filters && !m_filters->accepts(i.data(TraceModel::KeyRole).toULongLong()))
        return false;

    const bool tx = i.data(TraceModel::DirectionRole).toString() == QLatin1String("TX");
    if ((tx && !m_showTx) || (!tx && !m_showRx))
        return false;

    if (m_search.isEmpty())
        return true;
    for (int role : {TraceModel::IdRole, TraceModel::NameRole, TraceModel::PayloadRole,
                     TraceModel::PgnRole, TraceModel::PgnNameRole}) {
        if (i.data(role).toString().contains(m_search, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace usbcan
