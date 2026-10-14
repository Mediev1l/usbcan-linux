#pragma once

#include "ICanTransport.h"

#include <QList>

// Transport double for controller tests; records sent frames.
class FakeTransport : public usbcan::ICanTransport
{
    Q_OBJECT

public:
    struct Script
    {
        bool openResult = true;
        bool sendResult = true;
        QString openError = QStringLiteral("cannot open");
        QList<usbcan::CanFrame> sent;
        int opens = 0;
        int closes = 0;
    };

    explicit FakeTransport(Script *script, QString description = QStringLiteral("fake0"))
        : m_script(script)
        , m_description(std::move(description))
    {
    }

    bool open() override
    {
        ++m_script->opens;
        if (!m_script->openResult) {
            emit errorOccurred(m_script->openError);
            return false;
        }
        m_open = true;
        return true;
    }

    void close() override
    {
        if (m_open)
            ++m_script->closes;
        m_open = false;
    }

    bool isOpen() const override { return m_open; }

    bool send(const usbcan::CanFrame &frame) override
    {
        if (!m_script->sendResult) {
            m_open = false;
            emit errorOccurred(QStringLiteral("send failed"));
            return false;
        }
        m_script->sent.append(frame);
        return true;
    }

    QString description() const override { return m_description; }

    // Simulates the hardware.
    void receive(const QList<usbcan::CanFrame> &frames) { emit framesReceived(frames); }
    void reportStatus(int rx, int tx) { emit busStatus(rx, tx); }
    void dropConnection(const QString &message)
    {
        m_open = false;
        emit errorOccurred(message);
    }
    void warn(const QString &message) { emit errorOccurred(message); }

private:
    Script *m_script;
    QString m_description;
    bool m_open = false;
};
