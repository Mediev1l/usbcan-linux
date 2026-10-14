#pragma once

#include "FilterModel.h"

#include <QPointer>
#include <QSortFilterProxyModel>

namespace usbcan {

// Trace rows that pass the identifier checkboxes, the RX/TX switches and the search text.
class TraceFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(bool showRx READ showRx WRITE setShowRx NOTIFY showRxChanged)
    Q_PROPERTY(bool showTx READ showTx WRITE setShowTx NOTIFY showTxChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit TraceFilterProxy(QObject *parent = nullptr);

    void setFilterModel(FilterModel *filters);

    bool showRx() const { return m_showRx; }
    void setShowRx(bool show);
    bool showTx() const { return m_showTx; }
    void setShowTx(bool show);
    QString searchText() const { return m_search; }
    void setSearchText(const QString &text);
    int count() const { return rowCount(); }

    // Row in the source TraceModel for a proxy row (-1 if out of range).
    Q_INVOKABLE int sourceRow(int proxyRow) const;

signals:
    void showRxChanged();
    void showTxChanged();
    void searchTextChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QPointer<FilterModel> m_filters;
    bool m_showRx = true;
    bool m_showTx = true;
    QString m_search;
};

} // namespace usbcan
