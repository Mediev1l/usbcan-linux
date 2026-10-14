#include "TraceModel.h"

#include <QAbstractItemModelTester>
#include <QDateTime>
#include <QSignalSpy>
#include <QVariantMap>

#include <gtest/gtest.h>

using namespace usbcan;
using R = TraceModel::Role;

namespace {

CanFrame f(quint32 id, bool ext, const char *hex, qint64 ms, Direction dir = Direction::Rx)
{
    return makeFrame(id, ext, QByteArray::fromHex(hex), dir, ms * 1000);
}

QVariant role(const TraceModel &m, int row, int r)
{
    return m.data(m.index(row), r);
}

} // namespace

TEST(TraceModel, ExtendedFrameRoles)
{
    TraceModel m;
    QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::Fatal);
    m.setNameLookup([](quint32 id, bool) { return id == 0x18FF9341 ? QStringLiteral("Engine") : QString(); });
    const qint64 ms = 1760376017337;
    m.appendFrames({f(0x18FF9341, true, "32ff", ms - 100), f(0x18FF9341, true, "33ff", ms)});

    ASSERT_EQ(m.rowCount(), 2);
    const int r = 1;
    EXPECT_EQ(role(m, r, Qt::DisplayRole), QStringLiteral("18FF9341"));
    EXPECT_EQ(role(m, r, R::IdRole), QStringLiteral("18FF9341"));
    EXPECT_EQ(role(m, r, R::KeyRole).toULongLong(), frameKey(0x18FF9341, true));
    EXPECT_EQ(role(m, r, R::SeqRole).toULongLong(), 2u);
    EXPECT_EQ(role(m, r, R::TimeRole), QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("HH:mm:ss.zzz")));
    EXPECT_EQ(role(m, r, R::DirectionRole), QStringLiteral("RX"));
    EXPECT_EQ(role(m, r, R::ExtendedRole), true);
    EXPECT_EQ(role(m, r, R::RtrRole), false);
    EXPECT_EQ(role(m, r, R::NameRole), QStringLiteral("Engine"));
    EXPECT_EQ(role(m, r, R::DlcRole).toInt(), 2);
    EXPECT_EQ(role(m, r, R::PayloadRole), QStringLiteral("33 FF"));
    EXPECT_EQ(role(m, r, R::ChangedMaskRole).toInt(), 0b1);
    EXPECT_EQ(role(m, r, R::CountRole).toULongLong(), 2u);
    EXPECT_DOUBLE_EQ(role(m, r, R::PeriodRole).toDouble(), 100.0);
    EXPECT_EQ(role(m, r, R::IsJ1939Role), true);
    EXPECT_EQ(role(m, r, R::PgnRole), QStringLiteral("FF93"));
    EXPECT_EQ(role(m, r, R::PgnDecimalRole).toInt(), 0xFF93);
    EXPECT_EQ(role(m, r, R::PgnNameRole), QStringLiteral("PROPB - Proprietary B"));
    EXPECT_EQ(role(m, r, R::PriorityRole).toInt(), 6);
    EXPECT_EQ(role(m, r, R::SourceRole), QStringLiteral("41"));
    EXPECT_EQ(role(m, r, R::DestinationRole), QStringLiteral("FF (global)"));
    EXPECT_EQ(role(m, r, R::PduFormatRole), QStringLiteral("PDU2 (broadcast)"));

    const QVariantList fields = role(m, r, R::IdFieldsRole).toList();
    ASSERT_EQ(fields.size(), 6);
    EXPECT_EQ(fields[0].toMap().value(QStringLiteral("label")), QStringLiteral("Priority"));
    EXPECT_EQ(fields[5].toMap().value(QStringLiteral("value")), QStringLiteral("41"));

    const QVariantList bytes = role(m, r, R::BytesRole).toList();
    ASSERT_EQ(bytes.size(), 2);
    const QVariantMap b0 = bytes[0].toMap();
    EXPECT_EQ(b0.value(QStringLiteral("index")).toInt(), 0);
    EXPECT_EQ(b0.value(QStringLiteral("hex")), QStringLiteral("33"));
    EXPECT_EQ(b0.value(QStringLiteral("dec")).toInt(), 0x33);
    EXPECT_EQ(b0.value(QStringLiteral("bin")), QStringLiteral("00110011"));
    EXPECT_EQ(b0.value(QStringLiteral("changed")), true);
    EXPECT_EQ(bytes[1].toMap().value(QStringLiteral("changed")), false);

    EXPECT_FALSE(role(m, r, Qt::UserRole + 999).isValid());
}

TEST(TraceModel, Pdu1DestinationAddress)
{
    TraceModel m;
    m.appendFrames({f(0x18EA00F9, true, "", 0)});
    EXPECT_EQ(role(m, 0, R::DestinationRole), QStringLiteral("00"));
}

TEST(TraceModel, StandardAndRemoteFrames)
{
    TraceModel m;
    CanFrame rtr = f(0x7FF, false, "", 0, Direction::Tx);
    rtr.rtr = true;
    m.appendFrames({rtr});

    EXPECT_EQ(role(m, 0, R::IdRole), QStringLiteral("7FF"));
    EXPECT_EQ(role(m, 0, R::DirectionRole), QStringLiteral("TX"));
    EXPECT_EQ(role(m, 0, R::PayloadRole), QStringLiteral("remote request"));
    EXPECT_EQ(role(m, 0, R::NameRole), QString());
    EXPECT_EQ(role(m, 0, R::IsJ1939Role), false);
    for (int r : {R::PgnRole, R::PgnNameRole, R::SourceRole, R::DestinationRole, R::PduFormatRole})
        EXPECT_EQ(role(m, 0, r), QString()) << r;
    EXPECT_EQ(role(m, 0, R::PgnDecimalRole).toInt(), 0);
    EXPECT_EQ(role(m, 0, R::PriorityRole).toInt(), 0);
    const QVariantList fields = role(m, 0, R::IdFieldsRole).toList();
    ASSERT_EQ(fields.size(), 1);
    EXPECT_EQ(fields[0].toMap().value(QStringLiteral("bits")), QStringLiteral("11111111111"));
}

TEST(TraceModel, InvalidAccess)
{
    TraceModel m;
    m.appendFrames({f(1, false, "", 0)});
    EXPECT_FALSE(m.data(m.index(5), R::IdRole).isValid());
    EXPECT_EQ(m.rowCount(m.index(0)), 0);
    EXPECT_EQ(m.frameAt(-1), CanFrame{});
    EXPECT_EQ(m.frameAt(1), CanFrame{});
    EXPECT_EQ(m.frameAt(0).id, 1u);
    EXPECT_TRUE(m.roleNames().value(R::IdRole) == "canId");
}

TEST(TraceModel, ChronologicalInsertAndEvict)
{
    TraceModel m(3);
    QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&m, &QAbstractItemModel::rowsRemoved);
    QSignalSpy reset(&m, &QAbstractItemModel::modelReset);

    m.appendFrames({});
    EXPECT_EQ(inserted.count(), 0);

    m.appendFrames({f(1, false, "", 0), f(2, false, "", 1)});
    EXPECT_EQ(inserted.count(), 1);
    m.appendFrames({f(3, false, "", 2), f(4, false, "", 3)});
    EXPECT_EQ(removed.count(), 1);
    EXPECT_EQ(removed[0][1].toInt(), 0);
    EXPECT_EQ(removed[0][2].toInt(), 0);
    ASSERT_EQ(m.rowCount(), 3);
    EXPECT_EQ(m.frameAt(0).id, 2u);

    // A batch bigger than the whole buffer resets the model.
    m.appendFrames({f(5, false, "", 4), f(6, false, "", 5), f(7, false, "", 6), f(8, false, "", 7)});
    EXPECT_EQ(reset.count(), 1);
    ASSERT_EQ(m.rowCount(), 3);
    EXPECT_EQ(m.frameAt(0).id, 6u);
    EXPECT_EQ(m.frames().size(), 3);
}

TEST(TraceModel, LatestPerIdMode)
{
    TraceModel m;
    QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy modeChanged(&m, &TraceModel::modeChanged);
    m.appendFrames({f(1, true, "01", 0), f(2, true, "02", 1), f(1, true, "03", 100)});

    m.setMode(TraceModel::LatestPerId);
    EXPECT_EQ(modeChanged.count(), 1);
    m.setMode(TraceModel::LatestPerId);
    EXPECT_EQ(modeChanged.count(), 1);
    ASSERT_EQ(m.rowCount(), 2);
    EXPECT_EQ(role(m, 0, R::PayloadRole), QStringLiteral("03"));
    EXPECT_EQ(role(m, 0, R::CountRole).toULongLong(), 2u);

    QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
    QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
    m.appendFrames({f(1, true, "04", 200), f(3, true, "05", 201), f(3, true, "06", 202)});
    EXPECT_EQ(inserted.count(), 1);
    EXPECT_EQ(changed.count(), 1);
    ASSERT_EQ(m.rowCount(), 3);
    EXPECT_EQ(role(m, 0, R::PayloadRole), QStringLiteral("04"));
    EXPECT_EQ(role(m, 2, R::PayloadRole), QStringLiteral("06"));
    EXPECT_EQ(m.frameAt(2).id, 3u);

    // Only updates: no insert. From empty: no dataChanged for old rows.
    m.appendFrames({f(2, true, "07", 300)});
    EXPECT_EQ(inserted.count(), 1);
    m.clear();
    changed.clear();
    m.appendFrames({f(9, true, "", 0)});
    EXPECT_EQ(changed.count(), 0);

    m.setMode(42); // anything unknown falls back to chronological
    EXPECT_EQ(m.mode(), TraceModel::Chronological);
}

TEST(TraceModel, NamesRefresh)
{
    TraceModel m;
    QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
    m.refreshNames(); // no rows: nothing to announce
    EXPECT_EQ(changed.count(), 0);

    m.appendFrames({f(1, true, "", 0)});
    QString name = QStringLiteral("old");
    m.setNameLookup([&name](quint32, bool) { return name; });
    EXPECT_EQ(changed.count(), 1);
    name = QStringLiteral("new");
    m.refreshNames();
    EXPECT_EQ(changed.count(), 2);
    EXPECT_EQ(role(m, 0, R::NameRole), QStringLiteral("new"));
}

TEST(TraceModel, Clear)
{
    TraceModel m;
    m.appendFrames({f(1, true, "", 0)});
    m.clear();
    EXPECT_EQ(m.rowCount(), 0);
    EXPECT_TRUE(m.store().keys().isEmpty());
}
