#include "TraceFilterProxy.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

using namespace usbcan;

namespace {

struct Fixture
{
    TraceModel trace;
    FilterModel filters;
    TraceFilterProxy proxy;

    Fixture()
    {
        trace.setNameLookup([](quint32 id, bool) { return id == 2 ? QStringLiteral("Dashboard") : QString(); });
        proxy.setSourceModel(&trace);
        proxy.setFilterModel(&filters);
        trace.appendFrames({
            makeFrame(0x18FF9341, true, QByteArray::fromHex("32ff"), Direction::Rx),
            makeFrame(2, false, QByteArray::fromHex("abcd"), Direction::Tx),
            makeFrame(0x18F00400, true, QByteArray::fromHex("01"), Direction::Rx),
        });
        filters.sync(trace.store(), trace.frames());
    }

    QStringList ids() const
    {
        QStringList list;
        for (int r = 0; r < proxy.rowCount(); ++r)
            list.append(proxy.index(r, 0).data(TraceModel::IdRole).toString());
        return list;
    }
};

} // namespace

TEST(TraceFilterProxy, UncheckedIdentifiersAreHidden)
{
    Fixture fx;
    QSignalSpy countChanged(&fx.proxy, &TraceFilterProxy::countChanged);
    EXPECT_EQ(fx.proxy.count(), 3);
    fx.filters.setVisible(frameKey(2, false), false);
    EXPECT_EQ(fx.ids(), (QStringList{QStringLiteral("18FF9341"), QStringLiteral("18F00400")}));
    EXPECT_GE(countChanged.count(), 1);
}

TEST(TraceFilterProxy, DirectionSwitches)
{
    Fixture fx;
    QSignalSpy rxChanged(&fx.proxy, &TraceFilterProxy::showRxChanged);
    QSignalSpy txChanged(&fx.proxy, &TraceFilterProxy::showTxChanged);

    fx.proxy.setShowRx(false);
    fx.proxy.setShowRx(false);
    EXPECT_FALSE(fx.proxy.showRx());
    EXPECT_EQ(rxChanged.count(), 1);
    EXPECT_EQ(fx.ids(), QStringList{QStringLiteral("002")});

    fx.proxy.setShowRx(true);
    fx.proxy.setShowTx(false);
    fx.proxy.setShowTx(false);
    EXPECT_FALSE(fx.proxy.showTx());
    EXPECT_EQ(txChanged.count(), 1);
    EXPECT_EQ(fx.proxy.count(), 2);
}

TEST(TraceFilterProxy, Search)
{
    Fixture fx;
    QSignalSpy searchChanged(&fx.proxy, &TraceFilterProxy::searchTextChanged);
    const QList<std::pair<QString, QStringList>> cases{
        {QStringLiteral("ff93"), {QStringLiteral("18FF9341")}},       // id and PGN
        {QStringLiteral("dashboard"), {QStringLiteral("002")}},       // name
        {QStringLiteral("AB CD"), {QStringLiteral("002")}},           // payload
        {QStringLiteral("eec1"), {QStringLiteral("18F00400")}},       // PGN name
        {QStringLiteral("F004"), {QStringLiteral("18F00400")}},       // PGN
        {QStringLiteral("nothing"), {}},
    };
    for (const auto &[text, expected] : cases) {
        fx.proxy.setSearchText(text);
        EXPECT_EQ(fx.ids(), expected) << text.toStdString();
    }
    fx.proxy.setSearchText(QStringLiteral("  nothing  ")); // trimmed: unchanged
    EXPECT_EQ(fx.proxy.searchText(), QStringLiteral("nothing"));
    EXPECT_EQ(searchChanged.count(), int(cases.size()));
    fx.proxy.setSearchText(QString());
    EXPECT_EQ(fx.proxy.count(), 3);
}

TEST(TraceFilterProxy, SourceRow)
{
    Fixture fx;
    fx.filters.setVisible(frameKey(0x18FF9341, true), false);
    EXPECT_EQ(fx.proxy.sourceRow(0), 1);
    EXPECT_EQ(fx.proxy.sourceRow(5), -1);
    EXPECT_EQ(fx.proxy.sourceRow(-1), -1);
}

TEST(TraceFilterProxy, WorksWithoutAndWithReplacedFilterModel)
{
    Fixture fx;
    fx.filters.setAll(false);
    EXPECT_EQ(fx.proxy.count(), 0);

    fx.proxy.setFilterModel(nullptr);
    EXPECT_EQ(fx.proxy.count(), 3);

    FilterModel other;
    fx.proxy.setFilterModel(&other);
    fx.proxy.setFilterModel(&fx.filters); // disconnects `other`
    other.setAll(false);
    EXPECT_EQ(fx.proxy.count(), 0);
}
