#pragma once

#include <QIODevice>

#include <algorithm>
#include <cstring>

// In-memory stand-in for the adapter's serial port.
class FakeSerialDevice : public QIODevice
{
    Q_OBJECT

public:
    bool failOpen = false;
    bool failWrite = false;
    QByteArray written;

    bool open(OpenMode mode) override
    {
        if (failOpen) {
            setErrorString(QStringLiteral("Permission denied"));
            return false;
        }
        return QIODevice::open(mode);
    }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_inbox.size() + QIODevice::bytesAvailable(); }

    // Bytes "sent by the adapter".
    void inject(const QByteArray &bytes)
    {
        m_inbox.append(bytes);
        emit readyRead();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 n = std::min<qint64>(maxSize, m_inbox.size());
        std::memcpy(data, m_inbox.constData(), size_t(n));
        m_inbox.remove(0, n);
        return n;
    }

    qint64 writeData(const char *data, qint64 size) override
    {
        if (failWrite) {
            setErrorString(QStringLiteral("I/O error"));
            return -1;
        }
        written.append(data, size);
        return size;
    }

private:
    QByteArray m_inbox;
};
