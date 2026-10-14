#pragma once

#include "ICanTransport.h"
#include "WaveshareProtocol.h"

#include <QIODevice>
#include <QTimer>

#include <functional>
#include <memory>

namespace usbcan {

// Waveshare USB-CAN-A over its CH340 serial port.
class WaveshareTransport : public ICanTransport
{
    Q_OBJECT

public:
    // Creates the (unopened) device for a port path; tests inject a fake QIODevice.
    using DeviceFactory = std::function<std::unique_ptr<QIODevice>(const QString &port)>;

    // 2 Mbaud 8N1, no flow control.
    static std::unique_ptr<QIODevice> createSerialPort(const QString &port);

    WaveshareTransport(QString port, waveshare::Config config,
                       DeviceFactory factory = &WaveshareTransport::createSerialPort,
                       QObject *parent = nullptr);
    ~WaveshareTransport() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool send(const CanFrame &frame) override;
    QString description() const override;

    // Asks the adapter for its error counters (answer arrives as busStatus).
    bool requestStatus();

    // How often the port path is checked for disappearance (unplug, dock reconnect).
    void setPresenceCheckInterval(int ms) { m_presenceTimer.setInterval(ms); }

private:
    void onReadyRead();
    void checkPresence();
    void fail(const QString &message);
    bool write(const QByteArray &bytes);

    QString m_port;
    waveshare::Config m_config;
    DeviceFactory m_factory;
    std::unique_ptr<QIODevice> m_device;
    waveshare::Parser m_parser;
    QTimer m_presenceTimer;
};

} // namespace usbcan
