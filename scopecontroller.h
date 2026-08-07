#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

// Calibrated samples returned by one oscilloscope channel.
struct WaveformData
{
    int channel = 0;
    QVector<double> time;
    QVector<double> voltage;
    QVector<double> displayVoltage;

    qsizetype pointCount() const { return time.size(); }
};

// Owns the Linux USBTMC file and all SCPI communication.
class ScopeController
{
public:
    ScopeController() = default;
    ~ScopeController();

    QStringList scanDevices() const;
    QString connectDevice(const QString &devicePath);
    void disconnect();

    bool isConnected() const { return fileDescriptor_ >= 0; }
    QString devicePath() const { return devicePath_; }
    QString identification() const { return identification_; }

    void writeCommand(const QString &command);
    QString query(const QString &command, int timeoutMs = 5000);
    QString sendScpi(const QString &command);

    void run();
    void stop();
    void single();
    void autoSet();
    QString triggerStatus();

    void setChannelDisplay(int channel, bool enabled);
    bool channelDisplay(int channel);
    void setChannelScale(int channel, double scale);
    double channelScale(int channel);
    void setTimeScale(double scale);
    double timeScale();

    WaveformData acquireWaveform(int channel);
    QByteArray captureScreenshot();

private:
    void requireConnection() const;
    void setDriverTimeout(int timeoutMs);
    void writeBytes(const QByteArray &bytes);
    QByteArray readChunk(qsizetype maximumBytes, int timeoutMs);
    QByteArray readBinaryBlock(const QString &command,
                               int timeoutMs = 30000,
                               qsizetype minimumSize = 0);

    int fileDescriptor_ = -1;
    QString devicePath_;
    QString identification_;
};
