#include "LibraryModel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>

#include <gtest/gtest.h>

using namespace usbcan;
using R = LibraryModel::Role;

namespace {

SavedMessage msg(const QString &name, quint32 id = 0x18FF9341, bool extended = true)
{
    SavedMessage m;
    m.name = name;
    m.id = id;
    m.extended = extended;
    m.payload = QByteArray::fromHex("32ff");
    m.periodMs = 100;
    m.comment = QStringLiteral("note");
    return m;
}

QVariant role(const LibraryModel &m, int row, int r)
{
    return m.data(m.index(row), r);
}

struct Fixture
{
    LibraryModel model;
    QAbstractItemModelTester tester{&model, QAbstractItemModelTester::FailureReportingMode::Fatal};
    QSignalSpy changed{&model, &LibraryModel::libraryChanged};
    QSignalSpy count{&model, &LibraryModel::countChanged};
};

} // namespace

TEST(LibraryModel, RolesAndHandles)
{
    Fixture fx;
    EXPECT_EQ(fx.model.add(msg("Engine")), 0);
    EXPECT_EQ(fx.model.add(msg("Std", 0x7FF, false)), 1);
    EXPECT_EQ(fx.changed.count(), 2);
    EXPECT_EQ(fx.count.count(), 2);
    EXPECT_EQ(fx.model.count(), 2);

    EXPECT_EQ(role(fx.model, 0, Qt::DisplayRole), QStringLiteral("Engine"));
    EXPECT_EQ(role(fx.model, 0, R::NameRole), QStringLiteral("Engine"));
    EXPECT_EQ(role(fx.model, 0, R::IdRole), QStringLiteral("18FF9341"));
    EXPECT_EQ(role(fx.model, 1, R::IdRole), QStringLiteral("7FF"));
    EXPECT_EQ(role(fx.model, 0, R::ExtendedRole), true);
    EXPECT_EQ(role(fx.model, 0, R::PayloadRole), QStringLiteral("32 FF"));
    EXPECT_EQ(role(fx.model, 0, R::PeriodRole).toInt(), 100);
    EXPECT_EQ(role(fx.model, 0, R::CommentRole), QStringLiteral("note"));
    EXPECT_EQ(role(fx.model, 0, R::RunningRole), false);
    EXPECT_FALSE(role(fx.model, 0, Qt::UserRole + 99).isValid());
    EXPECT_FALSE(fx.model.data(fx.model.index(5), R::NameRole).isValid());
    EXPECT_EQ(fx.model.rowCount(fx.model.index(0)), 0);

    const quint64 h0 = fx.model.handle(0);
    const quint64 h1 = fx.model.handle(1);
    EXPECT_NE(h0, h1);
    EXPECT_EQ(role(fx.model, 0, R::HandleRole).toULongLong(), h0);
    EXPECT_EQ(fx.model.rowOfHandle(h1), 1);
    EXPECT_EQ(fx.model.rowOfHandle(999), -1);
    EXPECT_EQ(fx.model.nameFor(0x7FF, false), QStringLiteral("Std"));
    EXPECT_EQ(fx.model.message(1).id, 0x7FFu);
    EXPECT_EQ(fx.model.library().size(), 2);
    EXPECT_EQ(fx.model.roleNames().value(R::PayloadRole), "payload");
    EXPECT_TRUE(fx.model.flags(fx.model.index(0)) & Qt::ItemIsEditable);
    EXPECT_EQ(fx.model.flags(QModelIndex()), Qt::NoItemFlags);
}

TEST(LibraryModel, EditingValidatesInput)
{
    Fixture fx;
    fx.model.add(msg("Engine"));
    fx.changed.clear();
    const QModelIndex i = fx.model.index(0);

    EXPECT_TRUE(fx.model.setData(i, QStringLiteral("  Motor "), R::NameRole));
    EXPECT_EQ(role(fx.model, 0, R::NameRole), QStringLiteral("Motor"));
    EXPECT_TRUE(fx.model.setData(i, QStringLiteral("0x18FF9241"), R::IdRole));
    EXPECT_EQ(role(fx.model, 0, R::IdRole), QStringLiteral("18FF9241"));
    EXPECT_FALSE(fx.model.setData(i, QStringLiteral("xyz"), R::IdRole));
    EXPECT_TRUE(fx.model.setData(i, QStringLiteral("01 02 03"), R::PayloadRole));
    EXPECT_FALSE(fx.model.setData(i, QStringLiteral("123"), R::PayloadRole));
    EXPECT_TRUE(fx.model.setData(i, -50, R::PeriodRole));
    EXPECT_EQ(role(fx.model, 0, R::PeriodRole).toInt(), 0);
    EXPECT_TRUE(fx.model.setData(i, QStringLiteral("c"), R::CommentRole));

    // A 29-bit ID cannot become a standard frame.
    EXPECT_FALSE(fx.model.setData(i, false, R::ExtendedRole));
    EXPECT_TRUE(fx.model.setData(i, QStringLiteral("123"), R::IdRole));
    EXPECT_TRUE(fx.model.setData(i, false, R::ExtendedRole));
    EXPECT_EQ(role(fx.model, 0, R::IdRole), QStringLiteral("123"));

    EXPECT_FALSE(fx.model.setData(i, true, R::RunningRole));
    EXPECT_FALSE(fx.model.setData(fx.model.index(4), QStringLiteral("x"), R::NameRole));
    EXPECT_EQ(fx.changed.count(), 7);

    // Unchanged value: accepted, no change signal.
    EXPECT_TRUE(fx.model.setData(i, QStringLiteral("c"), R::CommentRole));
    EXPECT_EQ(fx.changed.count(), 7);
}

TEST(LibraryModel, RemoveMoveAndReset)
{
    Fixture fx;
    for (const char *n : {"A", "B", "C"})
        fx.model.add(msg(QString::fromLatin1(n)));
    const quint64 hA = fx.model.handle(0);

    fx.model.move(0, 2);
    EXPECT_EQ(role(fx.model, 2, R::NameRole), QStringLiteral("A"));
    EXPECT_EQ(fx.model.handle(2), hA);
    fx.model.move(2, 0);
    EXPECT_EQ(role(fx.model, 0, R::NameRole), QStringLiteral("A"));
    for (auto [from, to] : {std::pair{-1, 0}, {0, 3}, {1, 1}, {3, 0}, {0, -1}})
        fx.model.move(from, to); // ignored
    EXPECT_EQ(role(fx.model, 0, R::NameRole), QStringLiteral("A"));

    fx.model.remove(1);
    EXPECT_EQ(fx.model.count(), 2);
    fx.model.remove(7);
    fx.model.remove(-1);
    EXPECT_EQ(fx.model.count(), 2);

    MessageLibrary lib;
    lib.add(msg("X"));
    fx.model.setLibrary(lib);
    EXPECT_EQ(fx.model.count(), 1);
    EXPECT_EQ(role(fx.model, 0, R::NameRole), QStringLiteral("X"));
    EXPECT_NE(fx.model.handle(0), hA);
}

TEST(LibraryModel, RunningState)
{
    Fixture fx;
    fx.model.add(msg("A"));
    const quint64 h = fx.model.handle(0);
    QSignalSpy dataChanged(&fx.model, &QAbstractItemModel::dataChanged);
    fx.model.setRunning(h, true);
    EXPECT_EQ(role(fx.model, 0, R::RunningRole), true);
    fx.model.setRunning(h, false);
    EXPECT_EQ(role(fx.model, 0, R::RunningRole), false);
    fx.model.setRunning(12345, true); // unknown handle
    EXPECT_EQ(dataChanged.count(), 2);

    fx.model.setRunning(h, true);
    fx.model.remove(0); // running flag goes with the row
    fx.model.add(msg("B"));
    EXPECT_EQ(role(fx.model, 0, R::RunningRole), false);
}
