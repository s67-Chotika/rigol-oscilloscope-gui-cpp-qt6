#include "scopecontroller.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/usb/tmc.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{
std::runtime_error error(const QString &message)
{
    return std::runtime_error(message.toStdString());
}

QString systemError(const QString &prefix)
{
    return QString("%1: %2").arg(prefix, QString::fromLocal8Bit(std::strerror(errno)));
}
}

ScopeController::~ScopeController()
{
    disconnect();
}

QStringList ScopeController::scanDevices() const
{
    QStringList devices;

    // Linux exposes every bound USB Test and Measurement device as
    // /dev/usbtmcN. QDir::System is required because these are character
    // devices rather than regular files.
    const QDir deviceDirectory("/dev");
    const QStringList names = deviceDirectory.entryList(
        {"usbtmc*"}, QDir::System | QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const QString &name : names) {
        const QString path = deviceDirectory.absoluteFilePath(name);
        if (QFileInfo::exists(path))
            devices << path;
    }

    return devices;
}

QString ScopeController::connectDevice(const QString &devicePath)
{
    if (devicePath.trimmed().isEmpty())
        throw error("Select a USBTMC device before connecting.");

    disconnect();

    const QByteArray nativePath = QFile::encodeName(devicePath);
    fileDescriptor_ = ::open(nativePath.constData(), O_RDWR | O_CLOEXEC);
    if (fileDescriptor_ < 0) {
        throw error(systemError(
            QString("Cannot open %1. Check the udev permission rule").arg(devicePath)));
    }

    devicePath_ = devicePath;
    try {
        identification_ = query("*IDN?", 5000).trimmed();
        if (!identification_.contains("RIGOL", Qt::CaseInsensitive))
            throw error("The selected USBTMC device is not a RIGOL oscilloscope: "
                        + identification_);
    } catch (...) {
        disconnect();
        throw;
    }

    return identification_;
}

void ScopeController::disconnect()
{
    if (fileDescriptor_ >= 0)
        ::close(fileDescriptor_);

    fileDescriptor_ = -1;
    devicePath_.clear();
    identification_.clear();
}

void ScopeController::requireConnection() const
{
    if (!isConnected())
        throw error("The oscilloscope is not connected.");
}

void ScopeController::setDriverTimeout(int timeoutMs)
{
    requireConnection();

    // The Linux usbtmc driver owns the real USB timeout. poll() must not be
    // used before a normal read(), because read() itself starts the USBTMC
    // DEV_DEP_MSG_IN request. The kernel accepts timeouts of at least 100 ms.
    __u32 value = static_cast<__u32>(std::max(100, timeoutMs));
    if (::ioctl(fileDescriptor_, USBTMC_IOCTL_SET_TIMEOUT, &value) < 0)
        throw error(systemError("Cannot configure the USBTMC timeout"));
}

void ScopeController::writeBytes(const QByteArray &bytes)
{
    requireConnection();
    setDriverTimeout(5000);

    qsizetype written = 0;
    while (written < bytes.size()) {
        const ssize_t count = ::write(fileDescriptor_,
                                      bytes.constData() + written,
                                      static_cast<size_t>(bytes.size() - written));
        if (count < 0) {
            if (errno == EINTR)
                continue;
            throw error(systemError("USBTMC write failed"));
        }
        if (count == 0)
            throw error("USBTMC write returned zero bytes.");
        written += count;
    }
}

void ScopeController::writeCommand(const QString &command)
{
    QString cleaned = command.trimmed();
    if (cleaned.isEmpty())
        throw error("Enter an SCPI command first.");

    QByteArray bytes = cleaned.toUtf8();
    bytes.append('\n');
    writeBytes(bytes);
}

QByteArray ScopeController::readChunk(qsizetype maximumBytes, int timeoutMs)
{
    requireConnection();
    if (maximumBytes <= 0)
        throw error("USBTMC read size must be greater than zero.");

    setDriverTimeout(timeoutMs);

    QByteArray buffer;
    buffer.resize(maximumBytes);
    ssize_t count;
    do {
        count = ::read(fileDescriptor_, buffer.data(), static_cast<size_t>(maximumBytes));
    } while (count < 0 && errno == EINTR);

    if (count < 0) {
        if (errno == ETIMEDOUT)
            throw error(QString("The oscilloscope response timed out after %1 ms.")
                            .arg(std::max(100, timeoutMs)));
        throw error(systemError("USBTMC read failed"));
    }
    if (count == 0)
        throw error("The oscilloscope returned an empty response.");

    buffer.resize(count);
    return buffer;
}

QString ScopeController::query(const QString &command, int timeoutMs)
{
    writeCommand(command);
    // One normal usbtmc read requests and receives the complete short SCPI
    // response. The driver removes the USBTMC transport header for us.
    return QString::fromLatin1(readChunk(65536, timeoutMs)).trimmed();
}

QString ScopeController::sendScpi(const QString &command)
{
    const QString cleaned = command.trimmed();
    if (cleaned.isEmpty())
        throw error("Enter an SCPI command first.");

    if (cleaned.contains('?'))
        return query(cleaned);

    writeCommand(cleaned);
    return "Command sent - no response expected.";
}

void ScopeController::run()
{
    writeCommand(":RUN");
}

void ScopeController::stop()
{
    writeCommand(":STOP");
}

void ScopeController::single()
{
    writeCommand(":SINGle");
}

void ScopeController::autoSet()
{
    if (identification_.contains("DHO", Qt::CaseInsensitive)) {
        writeCommand(":AUToset:ENAble ON");
        writeCommand(":AUToset");
    } else {
        writeCommand(":SYSTem:AUToscale ON");
        writeCommand(":AUToscale");
    }
}

QString ScopeController::triggerStatus()
{
    return query(":TRIGger:STATus?").toUpper();
}

void ScopeController::setChannelDisplay(int channel, bool enabled)
{
    writeCommand(QString(":CHANnel%1:DISPlay %2")
                     .arg(channel)
                     .arg(enabled ? "ON" : "OFF"));
}

bool ScopeController::channelDisplay(int channel)
{
    return query(QString(":CHANnel%1:DISPlay?").arg(channel)).toDouble() != 0.0;
}

void ScopeController::setChannelScale(int channel, double scale)
{
    writeCommand(QString(":CHANnel%1:SCALe %2")
                     .arg(channel)
                     .arg(scale, 0, 'g', 12));
}

double ScopeController::channelScale(int channel)
{
    return query(QString(":CHANnel%1:SCALe?").arg(channel)).toDouble();
}

void ScopeController::setTimeScale(double scale)
{
    writeCommand(QString(":TIMebase:MAIN:SCALe %1").arg(scale, 0, 'g', 12));
}

double ScopeController::timeScale()
{
    return query(":TIMebase:MAIN:SCALe?").toDouble();
}

QByteArray ScopeController::readBinaryBlock(const QString &command,
                                            int timeoutMs,
                                            qsizetype minimumSize)
{
    writeCommand(command);

    QByteArray received;
    QElapsedTimer timer;
    timer.start();

    auto remainingTime = [&]() {
        return std::max(1, timeoutMs - static_cast<int>(timer.elapsed()));
    };

    // Obtain enough bytes to decode the #Nxxxxxxxx binary-block header.
    while (received.size() < 2)
        received += readChunk(4096, remainingTime());

    if (received.at(0) != '#')
        throw error("Invalid binary block header returned by the oscilloscope.");

    const int digitCount = received.mid(1, 1).toInt();
    if (digitCount <= 0 || digitCount > 9)
        throw error("Invalid binary block length digit count.");

    const qsizetype headerSize = 2 + digitCount;
    while (received.size() < headerSize)
        received += readChunk(4096, remainingTime());

    bool lengthOk = false;
    const qlonglong payloadLength = received.mid(2, digitCount).toLongLong(&lengthOk);
    if (!lengthOk || payloadLength < 0)
        throw error("Invalid binary block payload length.");

    const qlonglong totalSize = headerSize + payloadLength;
    while (received.size() < totalSize) {
        if (timer.elapsed() >= timeoutMs)
            throw error("Binary transfer timed out.");
        const qlonglong missing = totalSize - received.size();
        // On the final read request up to two extra bytes so an optional CR/LF
        // terminator is consumed with the binary block instead of contaminating
        // the next text query.
        const qlonglong payloadRequest = std::min<qlonglong>(65534, missing);
        const qlonglong trailerAllowance = missing <= 65534 ? 2 : 0;
        received += readChunk(static_cast<qsizetype>(payloadRequest + trailerAllowance),
                              remainingTime());
    }

    QByteArray payload = received.mid(headerSize, payloadLength);
    const QByteArray trailer = received.mid(totalSize);
    for (char byte : trailer) {
        if (byte != '\r' && byte != '\n')
            throw error(QString("Unexpected byte 0x%1 after the binary block.")
                            .arg(static_cast<unsigned char>(byte), 2, 16, QLatin1Char('0')));
    }
    if (minimumSize > 0 && payload.size() < minimumSize)
        throw error(QString("Expected at least %1 bytes, received %2.")
                        .arg(minimumSize)
                        .arg(payload.size()));

    return payload;
}

WaveformData ScopeController::acquireWaveform(int channel)
{
    writeCommand(QString(":WAVeform:SOURce CHANnel%1").arg(channel));
    writeCommand(":WAVeform:MODE NORMal");
    writeCommand(":WAVeform:FORMat BYTE");

    const QStringList fields = query(":WAVeform:PREamble?").split(',');
    if (fields.size() < 10)
        throw error(QString("Invalid waveform preamble for CH%1.").arg(channel));

    const int returnedPoints = fields.at(2).toDouble();
    const double xIncrement = fields.at(4).toDouble();
    const double xOrigin = fields.at(5).toDouble();
    const double xReference = fields.at(6).toDouble();
    const double yIncrement = fields.at(7).toDouble();
    const double yOrigin = fields.at(8).toDouble();
    const double yReference = fields.at(9).toDouble();

    const QByteArray payload = readBinaryBlock(":WAVeform:DATA?", 30000, 1);
    const int pointCount = std::min(returnedPoints, static_cast<int>(payload.size()));
    if (pointCount < 2)
        throw error(QString("Not enough waveform points returned from CH%1.").arg(channel));

    WaveformData waveform;
    waveform.channel = channel;
    waveform.time.reserve(pointCount);
    waveform.voltage.reserve(pointCount);
    waveform.displayVoltage.reserve(pointCount);

    for (int index = 0; index < pointCount; ++index) {
        const double raw = static_cast<unsigned char>(payload.at(index));
        waveform.time.append((index - xReference) * xIncrement + xOrigin);
        waveform.voltage.append((raw - yOrigin - yReference) * yIncrement);
        waveform.displayVoltage.append((raw - 127.0) * yIncrement);
    }

    return waveform;
}

QByteArray ScopeController::captureScreenshot()
{
    const QString command = identification_.contains("DHO", Qt::CaseInsensitive)
                                ? ":DISPlay:DATA? PNG"
                                : ":DISPlay:DATA? ON,OFF,PNG";
    const QByteArray data = readBinaryBlock(command, 30000);
    if (!data.startsWith("\x89PNG\r\n\x1a\n"))
        throw error("The oscilloscope did not return valid PNG data.");
    return data;
}
