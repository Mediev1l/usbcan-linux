#pragma once

#include "CanFrame.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

namespace usbcan {

// A named frame the user saved so it can be recognised in the trace and sent again.
struct SavedMessage
{
    QString name;
    quint32 id = 0;
    bool extended = true;
    QByteArray payload;
    int periodMs = 0; // 0 = send once only
    QString comment;

    CanFrame toFrame(qint64 timestampUs = 0) const;
    bool operator==(const SavedMessage &) const = default;
};

// Ordered collection of saved messages, persisted as JSON:
//   {"format":"usbcan-library","version":1,"messages":[{"name":..,"id":"18FF9341",
//    "extended":true,"payload":"32 FF ..","periodMs":100,"comment":..}]}
class MessageLibrary
{
public:
    static constexpr int kVersion = 1;

    const QList<SavedMessage> &messages() const { return m_messages; }
    qsizetype size() const { return m_messages.size(); }
    const SavedMessage &at(qsizetype i) const { return m_messages.at(i); }

    qsizetype add(const SavedMessage &message);
    void replace(qsizetype i, const SavedMessage &message);
    void remove(qsizetype i);
    void clear() { m_messages.clear(); }

    // First message with this identifier, or -1.
    qsizetype indexOf(quint32 id, bool extended) const;
    QString nameFor(quint32 id, bool extended) const;

    QByteArray toJson() const;
    static std::optional<MessageLibrary> fromJson(const QByteArray &json, QString *error = nullptr);

    bool save(const QString &path, QString *error = nullptr) const;
    // Replaces the content; leaves it untouched on failure. A missing file is an error.
    bool load(const QString &path, QString *error = nullptr);

private:
    QList<SavedMessage> m_messages;
};

} // namespace usbcan
