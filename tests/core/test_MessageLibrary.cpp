#include "MessageLibrary.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace usbcan;

namespace {

SavedMessage sample(const QString &name, quint32 id = 0x18FF9341, bool extended = true)
{
    SavedMessage m;
    m.name = name;
    m.id = id;
    m.extended = extended;
    m.payload = QByteArray::fromHex("32FFFFFFFFFF0FFF");
    m.periodMs = 100;
    m.comment = QStringLiteral("seen on the bench, \"quoted\"");
    return m;
}

void writeText(const QString &path, const QByteArray &text)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(text);
}

} // namespace

TEST(MessageLibrary, EditOperations)
{
    MessageLibrary lib;
    EXPECT_EQ(lib.add(sample("A")), 0);
    EXPECT_EQ(lib.add(sample("B", 0x123, false)), 1);
    EXPECT_EQ(lib.size(), 2);
    EXPECT_EQ(lib.at(1).name, QStringLiteral("B"));

    lib.replace(0, sample("A2"));
    EXPECT_EQ(lib.messages()[0].name, QStringLiteral("A2"));
    EXPECT_EQ(lib.indexOf(0x123, false), 1);
    EXPECT_EQ(lib.indexOf(0x123, true), -1);
    EXPECT_EQ(lib.nameFor(0x18FF9341, true), QStringLiteral("A2"));
    EXPECT_TRUE(lib.nameFor(0x1, true).isEmpty());

    lib.remove(0);
    EXPECT_EQ(lib.size(), 1);
    lib.clear();
    EXPECT_EQ(lib.size(), 0);
}

TEST(MessageLibrary, ToFrameIsTransmit)
{
    const CanFrame f = sample("A").toFrame(5);
    EXPECT_EQ(f.id, 0x18FF9341u);
    EXPECT_TRUE(f.extended);
    EXPECT_EQ(f.payload(), QByteArray::fromHex("32FFFFFFFFFF0FFF"));
    EXPECT_EQ(f.dir, Direction::Tx);
    EXPECT_EQ(f.timestampUs, 5);
}

TEST(MessageLibrary, JsonRoundTrip)
{
    MessageLibrary lib;
    lib.add(sample("Engine"));
    lib.add(sample("Std", 0x7FF, false));
    SavedMessage empty = sample("Empty");
    empty.payload.clear();
    empty.periodMs = 0;
    lib.add(empty);

    const QByteArray json = lib.toJson();
    EXPECT_TRUE(json.contains("\"id\": \"18FF9341\""));
    EXPECT_TRUE(json.contains("\"payload\": \"32 FF FF FF FF FF 0F FF\""));

    const auto back = MessageLibrary::fromJson(json);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->messages(), lib.messages());
}

TEST(MessageLibrary, FromJsonDefaultsAndClamping)
{
    const auto lib = MessageLibrary::fromJson(
        R"({"format":"usbcan-library","version":1,"messages":[{"id":"18FF9341","payload":"01","periodMs":-5}]})");
    ASSERT_TRUE(lib.has_value());
    ASSERT_EQ(lib->size(), 1);
    EXPECT_TRUE(lib->at(0).extended);
    EXPECT_EQ(lib->at(0).periodMs, 0);
    EXPECT_TRUE(lib->at(0).name.isEmpty());
}

TEST(MessageLibrary, FromJsonErrors)
{
    struct Case
    {
        QByteArray json;
        QString error;
    };
    const QList<Case> cases{
        {"{oops", QStringLiteral("Not a valid library file")},
        {"[1,2]", QStringLiteral("Not a valid library file")},
        {R"({"format":"other","version":1})", QStringLiteral("Not a usbcan library file")},
        {R"({"format":"usbcan-library","version":0})", QStringLiteral("Unsupported library version 0")},
        {R"({"format":"usbcan-library","version":2})", QStringLiteral("Unsupported library version 2")},
        {R"({"format":"usbcan-library","version":1,"messages":[{"name":"X","id":"zz","payload":""}]})",
         QStringLiteral("Message 1 (X): ID must be hexadecimal")},
        {R"({"format":"usbcan-library","version":1,"messages":[{"name":"Y","id":"1","payload":"0"}]})",
         QStringLiteral("Message 1 (Y): Payload has an odd number of hex digits")},
    };
    for (const Case &c : cases) {
        QString error;
        EXPECT_FALSE(MessageLibrary::fromJson(c.json, &error).has_value()) << c.json.constData();
        EXPECT_TRUE(error.startsWith(c.error)) << error.toStdString();
    }
    EXPECT_FALSE(MessageLibrary::fromJson("{oops").has_value()); // no error pointer
}

TEST(MessageLibrary, SaveAndLoad)
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("nested/lib.canlib.json"));
    MessageLibrary lib;
    lib.add(sample("A"));
    ASSERT_TRUE(lib.save(path));

    MessageLibrary loaded;
    ASSERT_TRUE(loaded.load(path));
    EXPECT_EQ(loaded.messages(), lib.messages());
}

TEST(MessageLibrary, SaveFailure)
{
    QTemporaryDir dir;
    MessageLibrary lib;
    QString error;
    EXPECT_FALSE(lib.save(dir.path(), &error)); // a directory, not a file
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(lib.save(dir.path()));
}

TEST(MessageLibrary, LoadFailuresKeepContent)
{
    QTemporaryDir dir;
    MessageLibrary lib;
    lib.add(sample("Keep"));

    QString error;
    EXPECT_FALSE(lib.load(dir.filePath(QStringLiteral("missing.json")), &error));
    EXPECT_FALSE(error.isEmpty());

    const QString bad = dir.filePath(QStringLiteral("bad.json"));
    writeText(bad, "not json");
    EXPECT_FALSE(lib.load(bad, &error));
    EXPECT_FALSE(lib.load(bad));
    ASSERT_EQ(lib.size(), 1);
    EXPECT_EQ(lib.at(0).name, QStringLiteral("Keep"));
}
