#pragma once

#include <QColor>
#include <QMainWindow>
#include <QMap>

#include <array>
#include <functional>

#include "scopecontroller.h"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class QWidget;
class WaveformWidget;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void wireUi();
    void scanDevices();
    void connectScope();
    void disconnectScope();
    void setConnected(bool connected);
    void setRunning(bool running, bool connected = true);
    void syncControls();

    void acquire(const QString &action);
    void pollSingle();
    void finishAutoset();
    void setChannelVisible(int channel, bool visible);
    void setChannelScale(int channel, const QString &label);
    void setTimeScale(const QString &label);
    void scheduleRefresh(int delayMs = 350);
    bool updateGraph();
    void drawWaveforms();
    void sendScpi();

    void savePlot();
    void saveScreenshot();
    void saveCsv();

    bool guard(const QString &action, const std::function<void()> &work, bool popup = true);
    QString closestLabel(double value, const QMap<QString, double> &choices) const;
    QString savePath(const QString &caption, const QString &prefix,
                     const QString &suffix, const QString &filter);
    void error(const QString &action, const QString &details, bool popup = true);
    void log(const QString &message);

    ScopeController scope_;
    QMap<int, WaveformData> waveforms_;
    QMap<QString, double> volts_;
    QMap<QString, double> times_;
    QMap<int, QColor> colors_;
    bool syncing_ = false;
    bool running_ = false;
    int singlePolls_ = 0;

    QComboBox *devices_ = nullptr;
    QPushButton *scan_ = nullptr;
    QPushButton *connect_ = nullptr;
    QPushButton *disconnect_ = nullptr;
    QLabel *connectionState_ = nullptr;
    QPushButton *run_ = nullptr;
    QPushButton *stop_ = nullptr;
    QPushButton *single_ = nullptr;
    QPushButton *autoset_ = nullptr;
    std::array<QCheckBox *, 4> channelOn_{};
    std::array<QComboBox *, 4> channelScale_{};
    QComboBox *timeScale_ = nullptr;
    QPushButton *savePlot_ = nullptr;
    QPushButton *saveScreen_ = nullptr;
    QPushButton *saveCsv_ = nullptr;
    WaveformWidget *plot_ = nullptr;
    QPushButton *refresh_ = nullptr;
    QLineEdit *command_ = nullptr;
    QPushButton *send_ = nullptr;
    QPlainTextEdit *response_ = nullptr;
    QPushButton *clearResponse_ = nullptr;
    QPlainTextEdit *activity_ = nullptr;
    QLabel *instrument_ = nullptr;
    QLabel *ready_ = nullptr;
    QTimer *refreshTimer_ = nullptr;
};
