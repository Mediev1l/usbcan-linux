#include "MessageLibrary.h"

#include "HexInput.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace usbcan {

namespace {

const QString kFormat = QStringLiteral("usbcan-library");

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

} // namespace

CanFrame SavedMessage::toFrame(qint64 timestampUs) const
{
    return makeFrame(id, extended, payload, Direction::Tx, timestampUs);
}

qsizetype MessageLibrary::add(const SavedMessage &message)
{
    m_messages.append(message);
    return m_messages.size() - 1;
}

void MessageLibrary::replace(qsizetype i, const SavedMessage &message)
{
    m_messages[i] = message;
}

void MessageLibrary::remove(qsizetype i)
{
    m_messages.removeAt(i);
}

qsizetype MessageLibrary::indexOf(quint32 id, bool extended) const
{
    for (qsizetype i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].id == id && m_messages[i].extended == extended)
            return i;
    }
    return -1;
}

QString MessageLibrary::nameFor(quint32 id, bool extended) const
{
    const qsizetype i = indexOf(id, extended);
    return i < 0 ? QString() : m_messages[i].name;
}

QByteArray MessageLibrary::toJson() const
{
    QJsonArray list;
    for (const SavedMessage &m : m_messages) {
        list.append(QJsonObject{
            {QStringLiteral("name"), m.name},
            {QStringLiteral("id"), formatId(m.id, m.extended)},
            {QStringLiteral("extended"), m.extended},
            {QStringLiteral("payload"), formatPayload(m.payload)},
            {QStringLiteral("periodMs"), m.periodMs},
            {QStringLiteral("comment"), m.comment},
        });
    }
    const QJsonObject root{
        {QStringLiteral("format"), kFormat},
        {QStringLiteral("version"), kVersion},
        {QStringLiteral("messages"), list},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<MessageLibrary> MessageLibrary::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        setError(error, QStringLiteral("Not a valid library file: %1").arg(parseError.errorString()));
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != kFormat) {
        setError(error, QStringLiteral("Not a usbcan library file"));
        return std::nullopt;
    }
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version < 1 || version > kVersion) {
        setError(error, QStringLiteral("Unsupported library version %1").arg(version));
        return std::nullopt;
    }

    MessageLibrary lib;
    const QJsonArray list = root.value(QStringLiteral("messages")).toArray();
    for (qsizetype i = 0; i < list.size(); ++i) {
        const QJsonObject o = list[i].toObject();
        SavedMessage m;
        m.name = o.value(QStringLiteral("name")).toString();
        m.extended = o.value(QStringLiteral("extended")).toBool(true);
        m.periodMs = std::max(0, o.value(QStringLiteral("periodMs")).toInt());
        m.comment = o.value(QStringLiteral("comment")).toString();

        const auto id = hexinput::parseId(o.value(QStringLiteral("id")).toString(), m.extended);
        const auto payload = hexinput::parsePayload(o.value(QStringLiteral("payload")).toString());
        if (!id.ok || !payload.ok) {
            setError(error, QStringLiteral("Message %1 (%2): %3")
                                .arg(i + 1)
                                .arg(m.name, id.ok ? payload.error : id.error));
            return std::nullopt;
        }
        m.id = id.id;
        m.payload = payload.bytes;
        lib.add(m);
    }
    return lib;
}

bool MessageLibrary::save(const QString &path, QString *error) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(toJson()) < 0 || !file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

bool MessageLibrary::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return false;
    }
    auto lib = fromJson(file.readAll(), error);
    if (!lib)
        return false;
    *this = std::move(*lib);
    return true;
}

} // namespace usbcan
