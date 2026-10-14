// Renders the application icon to resources/icons/usbcan-linux-<size>.png.
// Same geometry as resources/usbcan-linux.svg: keep both in sync when changing it.
// (QPainter instead of the SVG, because the Qt SVG plugin is optional.)
//
//   g++ -std=c++20 -fPIC scripts/render-icons.cpp $(pkg-config --cflags --libs Qt6Gui) -o /tmp/render-icons
//   QT_QPA_PLATFORM=offscreen /tmp/render-icons resources/icons
#include <QGuiApplication>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <cstdio>
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QString out = QString::fromLocal8Bit(argv[1]);
    const QList<QPointF> xs{{48,0},{84,0},{84,1},{124,1},{124,0},{148,0},{148,1},{172,1},{172,0},{208,0}};
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size / 256.0, size / 256.0);
        QLinearGradient g(0, 16, 0, 240);
        g.setColorAt(0, QColor("#3ddc84"));
        g.setColorAt(1, QColor("#1b8f4c"));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(QRectF(16, 16, 224, 224), 48, 48);
        auto wave = [&](qreal recessive, qreal dominant, const char *color, qreal width) {
            QPainterPath path;
            for (int i = 0; i < xs.size(); ++i) {
                const QPointF pt(xs[i].x(), xs[i].y() ? dominant : recessive);
                i == 0 ? path.moveTo(pt) : path.lineTo(pt);
            }
            // Thicker strokes at tiny sizes keep the waveform readable.
            const qreal w = size <= 24 ? width * 1.6 : width;
            p.setPen(QPen(QColor(color), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        };
        wave(148, 196, "#f1fa8c", 14); // CAN_L
        wave(124, 76, "#ffffff", 18);  // CAN_H
        p.end();
        const QString file = QStringLiteral("%1/usbcan-linux-%2.png").arg(out).arg(size);
        std::printf("%s %s\n", qPrintable(file), img.save(file) ? "ok" : "FAILED");
    }
}
