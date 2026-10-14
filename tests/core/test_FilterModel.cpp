#include "FilterModel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>

#include <gtest/gtest.h>

using namespace usbcan;
using R = FilterModel::Role;

namespace {

CanFrame f(quint32 id, bool ext, qint64 ms = 0)
{
    return makeFrame(id, ext, {}, Direction::Rx, ms * 1000);
}

QVariant role(const FilterModel &m, int row, int r)
{
    return m.data(m.index(row), r);
}

struct Fixture
{
    TraceStore store;
    FilterModel model;
    QAbstractItemModelTester tester{&model, QAbstractItemModelTester::FailureReportingMode::Fatal};

    void add(const CanFrame &frame)
    {
        store.append(frame);
        model.sync(store, {frame});
    }
};

} // namespace

TEST(FilterModel, NewIdentifiersAppearChecked)
{
    Fixture fx;
    QSignalSpy countChanged(&fx.model, &FilterModel::countChanged);
    QSignalSpy filterChanged(&fx.model, &FilterModel::filterChanged);

    fx.add(f(0x18FF9341, true, 0));
    fx.add(f(0x18FF9341, true, 100));
    fx.add(f(0x123, false));

    ASSERT_EQ(fx.model.count(), 2);
    EXPECT_EQ(countChanged.count(), 2);
    EXPECT_EQ(filterChanged.count(), 0);
    EXPECT_TRUE(fx.model.accepts(frameKey(0x18FF9341, true)));
    EXPECT_EQ(fx.model.hiddenCount(), 0);

    EXPECT_EQ(role(fx.model, 0, Qt::DisplayRole), QStringLiteral("18FF9341"));
    EXPECT_EQ(role(fx.model, 0, R::KeyRole).toULongLong(), frameKey(0x18FF9341, true));
    EXPECT_EQ(role(fx.model, 0, R::CheckedRole), true);
    EXPECT_EQ(role(fx.model, 0, Qt::CheckStateRole).toInt(), Qt::Checked);
    EXPECT_EQ(role(fx.model, 0, R::ExtendedRole), true);
    EXPECT_EQ(role(fx.model, 0, R::PgnRole), QStringLiteral("FF93"));
    EXPECT_EQ(role(fx.model, 0, R::PgnNameRole), QStringLiteral("PROPB - Proprietary B"));
    EXPECT_EQ(role(fx.model, 0, R::SourceRole), QStringLiteral("41"));
    EXPECT_EQ(role(fx.model, 0, R::CountRole).toULongLong(), 2u);
    EXPECT_DOUBLE_EQ(role(fx.model, 0, R::RateRole).toDouble(), 10.0);
    EXPECT_EQ(role(fx.model, 0, R::NameRole), QString());
    EXPECT_EQ(role(fx.model, 0, R::SearchRole), QStringLiteral("18FF9341  FF93 PROPB - Proprietary B"));

    EXPECT_EQ(role(fx.model, 1, R::IdRole), QStringLiteral("123"));
    EXPECT_EQ(role(fx.model, 1, R::PgnRole), QString());
    EXPECT_EQ(role(fx.model, 1, R::PgnNameRole), QString());
    EXPECT_EQ(role(fx.model, 1, R::SourceRole), QString());

    EXPECT_FALSE(role(fx.model, 0, Qt::UserRole + 999).isValid());
    EXPECT_FALSE(fx.model.data(fx.model.index(9), R::IdRole).isValid());
    EXPECT_EQ(fx.model.rowCount(fx.model.index(0)), 0);
    EXPECT_EQ(fx.model.roleNames().value(R::CheckedRole), "checked");
}

TEST(FilterModel, OnlyReceivedIdentifiersAreAdded)
{
    TraceStore store;
    FilterModel model;
    const CanFrame a = f(1, true), b = f(2, true);
    store.append(a);
    store.append(b);
    store.append(a);
    model.sync(store, {a, b, a}); // duplicates in one batch: one row each
    EXPECT_EQ(model.count(), 2);

    model.clear();
    model.sync(store, {}); // history alone does not bring rows back
    EXPECT_EQ(model.count(), 0);
    model.sync(store, {b});
    ASSERT_EQ(model.count(), 1);
    EXPECT_EQ(model.data(model.index(0), FilterModel::IdRole), QStringLiteral("00000002"));
    EXPECT_EQ(model.data(model.index(0), FilterModel::CountRole).toULongLong(), 1u);
}

TEST(FilterModel, NewIdentifiersHiddenWhenConfigured)
{
    Fixture fx;
    QSignalSpy visibleChanged(&fx.model, &FilterModel::newIdsVisibleChanged);
    fx.model.setNewIdsVisible(false);
    fx.model.setNewIdsVisible(false);
    EXPECT_EQ(visibleChanged.count(), 1);
    EXPECT_FALSE(fx.model.newIdsVisible());
    EXPECT_FALSE(fx.model.accepts(frameKey(0x5, true))); // unseen

    QSignalSpy filterChanged(&fx.model, &FilterModel::filterChanged);
    fx.add(f(0x5, true));
    EXPECT_EQ(filterChanged.count(), 1);
    EXPECT_FALSE(fx.model.accepts(frameKey(0x5, true)));
    EXPECT_EQ(role(fx.model, 0, Qt::CheckStateRole).toInt(), Qt::Unchecked);
    EXPECT_EQ(fx.model.hiddenCount(), 1);
}

TEST(FilterModel, CheckboxEditing)
{
    Fixture fx;
    fx.add(f(1, true));
    fx.add(f(2, true));
    QSignalSpy filterChanged(&fx.model, &FilterModel::filterChanged);

    EXPECT_TRUE(fx.model.setData(fx.model.index(0), false, R::CheckedRole));
    EXPECT_FALSE(fx.model.accepts(frameKey(1, true)));
    EXPECT_TRUE(fx.model.setData(fx.model.index(0), Qt::Checked, Qt::CheckStateRole));
    EXPECT_TRUE(fx.model.accepts(frameKey(1, true)));
    EXPECT_EQ(filterChanged.count(), 2);

    // Same state again: no change signal.
    EXPECT_TRUE(fx.model.setData(fx.model.index(0), true, R::CheckedRole));
    EXPECT_EQ(filterChanged.count(), 2);

    EXPECT_FALSE(fx.model.setData(fx.model.index(0), false, R::NameRole));
    EXPECT_FALSE(fx.model.setData(fx.model.index(7), false, R::CheckedRole));

    EXPECT_TRUE(fx.model.flags(fx.model.index(0)) & Qt::ItemIsUserCheckable);
    EXPECT_EQ(fx.model.flags(QModelIndex()), Qt::NoItemFlags);
}

TEST(FilterModel, BulkOperations)
{
    Fixture fx;
    for (quint32 id : {1u, 2u, 3u})
        fx.add(f(id, true));

    fx.model.setAll(false);
    EXPECT_EQ(fx.model.hiddenCount(), 3);
    fx.model.setAll(true);
    EXPECT_EQ(fx.model.hiddenCount(), 0);

    fx.model.setVisible(frameKey(1, true), false);
    fx.model.invert();
    EXPECT_TRUE(fx.model.accepts(frameKey(1, true)));
    EXPECT_FALSE(fx.model.accepts(frameKey(2, true)));

    fx.model.showOnly(2);
    EXPECT_FALSE(fx.model.accepts(frameKey(1, true)));
    EXPECT_TRUE(fx.model.accepts(frameKey(3, true)));
    fx.model.showOnly(-1);
    fx.model.showOnly(3);
    EXPECT_TRUE(fx.model.accepts(frameKey(3, true)));

    fx.model.showOnlyKey(frameKey(1, true));
    EXPECT_TRUE(fx.model.accepts(frameKey(1, true)));
    EXPECT_FALSE(fx.model.accepts(frameKey(3, true)));
    fx.model.showOnlyKey(frameKey(42, true)); // unknown: no change
    EXPECT_TRUE(fx.model.accepts(frameKey(1, true)));
}

TEST(FilterModel, HiddenKeysAreRememberedForUnseenIds)
{
    Fixture fx;
    fx.model.setHiddenKeys({frameKey(0x99, true)});
    EXPECT_FALSE(fx.model.accepts(frameKey(0x99, true)));
    fx.model.setVisible(frameKey(0x77, true), false); // not in the list yet
    EXPECT_EQ(fx.model.hiddenKeys().size(), 2);

    fx.add(f(0x99, true));
    EXPECT_EQ(role(fx.model, 0, R::CheckedRole), false);
}

TEST(FilterModel, ClearAndResync)
{
    Fixture fx;
    fx.add(f(1, true));
    fx.model.setVisible(frameKey(1, true), false);
    fx.store.clear();
    fx.model.sync(fx.store, {}); // rows whose identifier left the store are kept
    EXPECT_EQ(fx.model.count(), 1);

    fx.model.clear();
    EXPECT_EQ(fx.model.count(), 0);
    EXPECT_FALSE(fx.model.accepts(frameKey(1, true)));
}

TEST(FilterModel, Names)
{
    Fixture fx;
    QSignalSpy changed(&fx.model, &QAbstractItemModel::dataChanged);
    fx.model.setNameLookup([](quint32, bool) { return QStringLiteral("Engine"); });
    EXPECT_EQ(changed.count(), 0);
    fx.add(f(1, true));
    changed.clear();
    fx.model.refreshNames();
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(role(fx.model, 0, R::NameRole), QStringLiteral("Engine"));
}
