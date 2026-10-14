#include "WaveshareTransport.h"

#include <QFileInfo>
#include <QSerialPort>

namespace usbcan {

std::unique_ptr<QIODevice> WaveshareTransport::createSerialPort(const QString &port)
{
    auto serial = std::make_unique<QSerialPort>();
    serial->setPortName(port);
    serial->setBaudRate(waveshare::kSerialBaudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    return serial;
}

WaveshareTransport::WaveshareTransport(QString port, waveshare::Config config,
                                       DeviceFactory factory, QObject *parent)
    : ICanTransport(parent)
    , m_port(std::move(port))
    , m_config(config)
    , m_factory(std::move(factory))
{
    m_presenceTimer.setInterval(500);
    connect(&m_presenceTimer, &QTimer::timeout, this, &WaveshareTransport::checkPresence);
}

WaveshareTransport::~WaveshareTransport()
{
    close();
}

bool WaveshareTransport::open()
{
    close();
    const QByteArray config = waveshare::encodeConfig(m_config);
    if (config.isEmpty()) {
        emit errorOccurred(QStringLiteral("Bitrate %1 is not supported by the adapter")
                               .arg(m_config.bitrate));
        return false;
    }

    m_device = m_factory(m_port);
    if (!m_device->open(QIODevice::ReadWrite)) {
        const QString reason = m_device->errorString();
        m_device.reset();
        emit errorOccurred(QStringLiteral("Cannot open %1: %2").arg(m_port, reason));
        return false;
    }
    m_parser.reset();
    connect(m_device.get(), &QIODevice::readyRead, this, &WaveshareTransport::onReadyRead);
    if (!write(config))
        return false;
    m_presenceTimer.start();
    return true;
}

void WaveshareTransport::close()
{
    m_presenceTimer.stop();
    if (m_device) {
        m_device->disconnect(this);
        m_device->close();
        m_device.reset();
    }
}

bool WaveshareTransport::isOpen() const
{
    return m_device != nullptr;
}

bool WaveshareTransport::send(const CanFrame &frame)
{
    if (!m_device)
        return false;
    return write(waveshare::encodeFrame(frame));
}

QString WaveshareTransport::description() const
{
    return m_port;
}

bool WaveshareTransport::requestStatus()
{
    if (!m_device)
        return false;
    return write(waveshare::encodeStatusRequest());
}

void WaveshareTransport::onReadyRead()
{
    const QByteArray bytes = m_device->readAll();
    QList<CanFrame> frames;
    for (const auto &event : m_parser.feed(bytes, nowUs())) {
        if (event.kind == waveshare::Parser::Event::Kind::Frame)
            frames.append(event.frame);
        else
            emit busStatus(event.status.rxErrors, event.status.txErrors);
    }
    if (!frames.isEmpty())
        emit framesReceived(frames);
}

void WaveshareTransport::checkPresence()
{
    if (!QFileInfo::exists(m_port))
        fail(QStringLiteral("Adapter %1 disappeared (unplugged?)").arg(m_port));
}

void WaveshareTransport::fail(const QString &message)
{
    close();
    emit errorOccurred(message);
}

bool WaveshareTransport::write(const QByteArray &bytes)
{
    if (m_device->write(bytes) != bytes.size()) {
        fail(QStringLiteral("Write to %1 failed: %2").arg(m_port, m_device->errorString()));
        return false;
    }
    return true;
}

} // namespace usbcan
