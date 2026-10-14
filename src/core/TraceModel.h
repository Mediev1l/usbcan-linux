#pragma once

#include "TraceStore.h"

#include <QAbstractListModel>

#include <functional>

namespace usbcan {

using IdNameLookup = std::function<QString(quint32 id, bool extended)>;

// Frames for the trace view. Every row also exposes the decoded details
// (J1939 fields, identifier bits, per-byte breakdown) shown when the row is expanded.
class TraceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    enum Mode { Chronological = 0, LatestPerId = 1 };
    Q_ENUM(Mode)

    enum Role {
        KeyRole = Qt::UserRole + 1,
        SeqRole,
        TimeRole,
        DirectionRole,
        IdRole,
        ExtendedRole,
        RtrRole,
        NameRole,
        DlcRole,
        PayloadRole,
        ChangedMaskRole,
        CountRole,
        PeriodRole,
        IsJ1939Role,
        PgnRole,
        PgnDecimalRole,
        PgnNameRole,
        PriorityRole,
        SourceRole,
        DestinationRole,
        PduFormatRole,
        IdFieldsRole,
        BytesRole,
    };
    Q_ENUM(Role)

    explicit TraceModel(qsizetype capacity = 200000, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendFrames(const QList<CanFrame> &frames);
    Q_INVOKABLE void clear();

    int mode() const { return m_mode; }
    void setMode(int mode);

    CanFrame frameAt(int row) const;
    // Chronological history, oldest first (independent of the mode).
    QList<CanFrame> frames() const;
    const TraceStore &store() const { return m_store; }

    void setNameLookup(IdNameLookup lookup);
    // Re-reads names after the lookup's data changed.
    void refreshNames();

signals:
    void modeChanged();

private:
    const TraceEntry &entryAt(int row) const;

    TraceStore m_store;
    Mode m_mode = Chronological;
    IdNameLookup m_nameLookup;
};

} // namespace usbcan
