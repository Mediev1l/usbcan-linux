#pragma once

#include "ICanTransport.h"
#include "SocketApi.h"

#include <QSocketNotifier>

#include <memory>

namespace usbcan {

// Linux SocketCAN (can0, vcan0, ...), classic CAN frames.
class SocketCanTransport : public ICanTransport
{
    Q_OBJECT

public:
    explicit SocketCanTransport(QString interfaceName,
                                std::unique_ptr<ISocketApi> api = std::make_unique<PosixSocketApi>(),
                                QObject *parent = nullptr);
    ~SocketCanTransport() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool send(const CanFrame &frame) override;
    QString description() const override;

    // Drains the socket; called when it becomes readable.
    void readPending();

private:
    void fail(const QString &what);

    QString m_interface;
    std::unique_ptr<ISocketApi> m_api;
    int m_fd = -1;
    std::unique_ptr<QSocketNotifier> m_notifier;
};

} // namespace usbcan
