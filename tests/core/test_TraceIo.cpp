#include "TraceIo.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace usbcan;
using namespace usbcan::traceio;

namespace {

CanFrame frame(quint32 id, bool extended, const char *hex, qint64 us, Direction dir = Direction::Rx)
{
    return makeFrame(id, extended, QByteArray::fromHex(hex), dir, us);
}

CanFrame remote(quint32 id, bool extended, int dlc, qint64 us)
{
    CanFrame f = makeFrame(id, extended, {}, Direction::Rx, us);
    f.rtr = true;
    f.dlc = quint8(dlc);
    return f;
}

const qint64 kT0 = 1760376017337000; // 2025-10-13 17:20:17.337 UTC

} // namespace

TEST(TraceIo, FormatForPath)
{
    EXPECT_EQ(formatForPath(QStringLiteral("a.CSV")), Format::Csv);
    EXPECT_EQ(formatForPath(QStringLiteral("a.log")), Format::Candump);
    EXPECT_EQ(formatForPath(QStringLiteral("a.candump")), Format::Candump);
    EXPECT_EQ(formatForPath(QStringLiteral("a.asc")), Format::Asc);
    EXPECT_FALSE(formatForPath(QStringLiteral("a.txt")).has_value());
}

TEST(TraceIo, CsvExport)
{
    const QList<CanFrame> frames{frame(0x18FF9341, true, "32ff", kT0),
                                frame(0x123, false, "", kT0 + 5, Direction::Tx)};
    const QString csv = exportText(frames, Format::Csv, [](const CanFrame &f) {
        return f.extended ? QStringLiteral("Engine, \"main\"") : QString();
    });
    EXPECT_EQ(csv, QStringLiteral("time_s,direction,id,extended,rtr,dlc,data,name\n"
                                  "1760376017.337000,RX,18FF9341,1,0,2,32 FF,\"Engine, \"\"main\"\"\"\n"
                                  "1760376017.337005,TX,123,0,0,0,,\n"));
    EXPECT_TRUE(exportText(frames, Format::Csv).endsWith(QStringLiteral(",0,0,0,,\n")));
}

TEST(TraceIo, CsvRoundTrip)
{
    CanFrame r = remote(0x55, false, 3, kT0 + 1);
    const QList<CanFrame> frames{frame(0x18FF9341, true, "32ffffffffff0fff", kT0),
                                frame(0x7FF, false, "01", kT0 + 999999, Direction::Tx), r};
    const QString csv = exportText(frames, Format::Csv, [](const CanFrame &) { return QStringLiteral("a,b"); });
    const ImportResult result = importText(csv, Format::Csv);
    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.skippedLines, 0);
    EXPECT_EQ(result.frames, frames);
}

TEST(TraceIo, CsvImportHandlesQuotedFields)
{
    const QString text = QStringLiteral("1.0,RX,123,0,0,1,AA,\"Engine, \"\"main\"\"\"\n"
                                        "2.0,TX,124,0,0,0,\"\",\"\"\n");
    const ImportResult result = importText(text, Format::Csv);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.frames.size(), 2);
    EXPECT_EQ(result.frames[0].id, 0x123u);
    EXPECT_EQ(result.frames[1].dir, Direction::Tx);
    EXPECT_EQ(result.frames[1].dlc, 0);
}

TEST(TraceIo, CsvImportSkipsInvalidLines)
{
    const QString text = QStringLiteral("# comment\n"
                                        "time_s,direction,id,extended,rtr,dlc,data,name\n"
                                        "1.5,RX,123,0,0,1,AA\n"      // valid, short fraction
                                        "1,RX,123\n"                 // too few columns
                                        "x,RX,123,0,0,1,AA,\n"       // bad time
                                        "1,RX,ZZZ,0,0,1,AA,\n"       // bad id
                                        "1,RX,123,0,0,1,AAA,\n"      // bad payload
                                        "1,RX,123,0,0,9,AA,\n"       // dlc out of range
                                        "1,RX,123,0,0,x,AA,\n"       // dlc not a number
                                        "\n");
    const ImportResult result = importText(text, Format::Csv);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.frames.size(), 1);
    EXPECT_EQ(result.frames[0].timestampUs, 1500000);
    EXPECT_EQ(result.skippedLines, 6);
}

TEST(TraceIo, CandumpExport)
{
    const QList<CanFrame> frames{frame(0x18FF9341, true, "32ff", kT0), frame(0x123, false, "", kT0 + 1),
                                remote(0x7, false, 0, kT0 + 2)};
    EXPECT_EQ(exportText(frames, Format::Candump, {}, QStringLiteral("wscan0")),
              QStringLiteral("(1760376017.337000) wscan0 18FF9341#32FF\n"
                             "(1760376017.337001) wscan0 123#\n"
                             "(1760376017.337002) wscan0 007#R\n"));
}

TEST(TraceIo, CandumpImport)
{
    const QString text = QStringLiteral("(1760376017.337000) can0 18FF9341#32FF\n"
                                        "(1.000001) vcan0 123#\n"
                                        "(2.5) can0 7FF#R\n"
                                        "(3.0) can0 7FF#R4\n"
                                        "(1.2.3) can0 123#00\n"     // bad time
                                        "(1.0) can0 123456789#00\n" // id too long
                                        "(1.0) can0 123#ABC\n"      // odd payload
                                        "garbage\n");
    const ImportResult result = importText(text, Format::Candump);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.frames.size(), 4);
    EXPECT_EQ(result.frames[0], frame(0x18FF9341, true, "32ff", kT0));
    EXPECT_EQ(result.frames[1], frame(0x123, false, "", 1000001));
    EXPECT_EQ(result.frames[2], remote(0x7FF, false, 0, 2500000));
    EXPECT_EQ(result.frames[3], remote(0x7FF, false, 4, 3000000));
    EXPECT_EQ(result.skippedLines, 4);
}

TEST(TraceIo, ImportFailures)
{
    const ImportResult asc = importText(QStringLiteral("anything"), Format::Asc);
    EXPECT_FALSE(asc.ok);
    EXPECT_EQ(asc.error, QStringLiteral("Importing Vector ASC files is not supported"));

    const ImportResult none = importText(QStringLiteral("garbage\n"), Format::Candump);
    EXPECT_FALSE(none.ok);
    EXPECT_EQ(none.error, QStringLiteral("No valid frames found"));

    const ImportResult empty = importText(QString(), Format::Csv);
    EXPECT_TRUE(empty.ok);
    EXPECT_TRUE(empty.frames.isEmpty());
}

TEST(TraceIo, AscExport)
{
    const QList<CanFrame> frames{frame(0x18FF9341, true, "32ff", kT0),
                                frame(0x123, false, "01", kT0 + 100250, Direction::Tx),
                                remote(0x7, false, 2, kT0 + 200000),
                                frame(0x1, false, "", kT0 - 1500)}; // out of order
    const QString asc = exportText(frames, Format::Asc);
    EXPECT_EQ(asc, QStringLiteral("date Mon Oct 13 17:20:17.337 2025\n"
                                  "base hex  timestamps absolute\n"
                                  "internal events logged\n"
                                  "// version 9.0.0\n"
                                  "Begin Triggerblock Mon Oct 13 17:20:17.337 2025\n"
                                  "   0.000000 1  18FF9341x       Rx   d 2 32 FF\n"
                                  "   0.100250 1  123             Tx   d 1 01\n"
                                  "   0.200000 1  7               Rx   r 2\n"
                                  "  -0.001500 1  1               Rx   d 0\n"
                                  "End TriggerBlock\n"));
}

TEST(TraceIo, AscExportEmpty)
{
    EXPECT_TRUE(exportText({}, Format::Asc).contains(QStringLiteral("date Thu Jan 01 00:00:00.000 1970")));
}

TEST(TraceIo, Files)
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("t.log"));
    ASSERT_TRUE(writeFile(path, QStringLiteral("hello ż")));
    EXPECT_EQ(readFile(path), QStringLiteral("hello ż"));

    QString error;
    EXPECT_FALSE(writeFile(dir.path(), QStringLiteral("x"), &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(writeFile(dir.path(), QStringLiteral("x")));

    error.clear();
    EXPECT_FALSE(readFile(dir.filePath(QStringLiteral("missing")), &error).has_value());
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(readFile(dir.filePath(QStringLiteral("missing"))).has_value());
}
