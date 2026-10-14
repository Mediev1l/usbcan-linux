#pragma once

#include "TraceModel.h"

#include <QAbstractListModel>
#include <QSet>

namespace usbcan {

// One checkable row per identifier ever seen. New identifiers are added
// automatically; their initial state follows `newIdsVisible`.
class FilterModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool newIdsVisible READ newIdsVisible WRITE setNewIdsVisible NOTIFY newIdsVisibleChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY filterChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        CheckedRole,
        IdRole,
        ExtendedRole,
        NameRole,
        PgnRole,
        PgnNameRole,
        SourceRole,
        CountRole,
        RateRole,
        SearchRole,
    };
    Q_ENUM(Role)

    explicit FilterModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Adds identifiers seen in `received` (frames just appended to `store`) that
    // are not listed yet, and refreshes all counters from the store.
    void sync(const TraceStore &store, const QList<CanFrame> &received);
    // Forgets all rows (hidden identifiers stay remembered).
    void clear();

    bool accepts(quint64 key) const;
    void setVisible(quint64 key, bool visible);

    Q_INVOKABLE void setAll(bool visible);
    Q_INVOKABLE void invert();
    Q_INVOKABLE void showOnly(int row);
    Q_INVOKABLE void showOnlyKey(quint64 key) { showOnly(m_rows.value(key, -1)); }

    bool newIdsVisible() const { return m_newIdsVisible; }
    void setNewIdsVisible(bool visible);
    int count() const { return rowCount(); }
    int hiddenCount() const;

    // Identifiers that are filtered out, including ones not seen yet (persisted).
    QSet<quint64> hiddenKeys() const { return m_hidden; }
    void setHiddenKeys(const QSet<quint64> &keys);

    void setNameLookup(IdNameLookup lookup);
    void refreshNames();

signals:
    void filterChanged();
    void newIdsVisibleChanged();
    void countChanged();

private:
    struct Entry
    {
        quint64 key = 0;
        quint64 count = 0;
        double rateHz = 0;
    };

    void emitChecked(int first, int last);

    QList<Entry> m_entries;
    QHash<quint64, int> m_rows;
    QSet<quint64> m_hidden;
    bool m_newIdsVisible = true;
    IdNameLookup m_nameLookup;
};

} // namespace usbcan
