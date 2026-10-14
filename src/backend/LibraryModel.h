#pragma once

#include "MessageLibrary.h"

#include <QAbstractListModel>
#include <QSet>

namespace usbcan {

// Editable list of saved messages for the send panel. Each row has a stable
// handle (used by the cyclic sender) that survives inserts and removals.
class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        HandleRole = Qt::UserRole + 1,
        NameRole,
        IdRole,
        ExtendedRole,
        PayloadRole,
        PeriodRole,
        CommentRole,
        RunningRole,
    };
    Q_ENUM(Role)

    explicit LibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    // Invalid id/payload text is rejected (returns false, nothing changes).
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    const MessageLibrary &library() const { return m_library; }
    void setLibrary(const MessageLibrary &library);

    int add(const SavedMessage &message);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void move(int from, int to);
    SavedMessage message(int row) const { return m_library.at(row); }
    quint64 handle(int row) const { return m_handles.at(row); }
    int rowOfHandle(quint64 handle) const;

    void setRunning(quint64 handle, bool running);
    QString nameFor(quint32 id, bool extended) const { return m_library.nameFor(id, extended); }
    int count() const { return rowCount(); }

signals:
    // Any change to the stored messages (not to the running state).
    void libraryChanged();
    void countChanged();

private:
    MessageLibrary m_library;
    QList<quint64> m_handles;
    QSet<quint64> m_running;
    quint64 m_nextHandle = 1;
};

} // namespace usbcan
