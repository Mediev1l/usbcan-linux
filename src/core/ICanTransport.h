#pragma once

#include "CanFrame.h"

#include <QList>
#include <QObject>
#include <QString>

namespace usbcan {

// A connection to a CAN bus. Implementations deliver received frames in batches.
class ICanTransport : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    // Queues a frame for transmission; false (and errorOccurred) on failure.
    virtual bool send(const CanFrame &frame) = 0;
    // Human readable "where": port path, interface name.
    virtual QString description() const = 0;

signals:
    void framesReceived(const QList<usbcan::CanFrame> &frames);
    // Adapter error counters, when the hardware reports them.
    void busStatus(int rxErrors, int txErrors);
    // Fatal for the connection: the transport is closed when this is emitted.
    void errorOccurred(const QString &message);
};

} // namespace usbcan
