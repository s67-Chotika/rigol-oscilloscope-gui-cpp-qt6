#include "mainwindow.h"
#include "waveformwidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>

namespace {
QString what(const std::exception &e) { return QString::fromLocal8Bit(e.what()); }

// Restores the mouse cursor automatically, including when an exception occurs.
class BusyCursor {
public:
    BusyCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
    ~BusyCursor() { QApplication::restoreOverrideCursor(); }
};

QGroupBox *box(const QString &title, QLayout *layout)
{
    auto *group = new QGroupBox(title);
    group->setLayout(layout);
    return group;
}

QPushButton *button(const QString &text) { return new QPushButton(text); }
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    volts_ = {
        {"10 mV/div", .01}, {"20 mV/div", .02}, {"50 mV/div", .05},
        {"100 mV/div", .1}, {"200 mV/div", .2}, {"500 mV/div", .5},
        {"1 V/div", 1}, {"2 V/div", 2}, {"5 V/div", 5}, {"10 V/div", 10}
    };
    times_ = {
        {"1 us/div", 1e-6}, {"2 us/div", 2e-6}, {"5 us/div", 5e-6},
        {"10 us/div", 1e-5}, {"20 us/div", 2e-5}, {"50 us/div", 5e-5},
        {"100 us/div", 1e-4}, {"200 us/div", 2e-4}, {"500 us/div", 5e-4},
        {"1 ms/div", 1e-3}, {"2 ms/div", 2e-3}, {"5 ms/div", 5e-3},
        {"10 ms/div", 1e-2}, {"20 ms/div", 2e-2}, {"50 ms/div", 5e-2},
        {"100 ms/div", .1}, {"200 ms/div", .2}, {"500 ms/div", .5}, {"1 s/div", 1}
    };
    colors_ = {{1, QColor("#F5C542")}, {2, QColor("#00CFE8")},
               {3, QColor("#F238A6")}, {4, QColor("#1E88E5")}};

    setWindowTitle("RIGOL Oscilloscope Control Panel - Qt6 C++");
    resize(1360, 820);
    setMinimumSize(1000, 680);
    setStyleSheet(R"(
QMainWindow,QWidget{background:#F5F6F7;color:#202124;font-family:"Segoe UI";font-size:11px}
QGroupBox{background:#FAFAFA;border:1px solid #D7DADF;border-radius:7px;font-weight:600;margin-top:11px;padding:11px 9px 9px}
QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;left:10px;padding:0 4px;background:#F5F6F7}
QPushButton{min-height:34px;padding:0 12px;background:#FFF;border:1px solid #C9CDD3;border-radius:5px}
QPushButton:hover{background:#EEF3F8;border-color:#8CA7C2} QPushButton:pressed{background:#E1EAF3}
QPushButton:disabled{color:#A5A8AC;background:#F0F1F2;border-color:#D8DBDE}
QComboBox,QLineEdit,QPlainTextEdit{background:#FFF;border:1px solid #C9CDD3;border-radius:4px;padding:5px 7px}
QComboBox,QLineEdit{min-height:25px} QComboBox:focus,QLineEdit:focus,QPlainTextEdit:focus{border-color:#4A90E2}
QComboBox:disabled,QLineEdit:disabled{color:#9AA0A6;background:#ECEFF1;border-color:#D8DBDE}
QStatusBar{background:#F4F5F6;border-top:1px solid #D7DADF}
)");

    buildUi();
    wireUi();
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setSingleShot(true);
    connect(refreshTimer_, &QTimer::timeout, this, [this] { updateGraph(); });
    setConnected(false);
    log("Finding connected oscilloscope automatically...");
    QTimer::singleShot(0, this, [this] { scanDevices(); });
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(12, 8, 12, 6);
    root->setSpacing(8);

    // Instrument connection row.
    auto *connectionLayout = new QHBoxLayout;
    connectionLayout->setSpacing(8);
    devices_ = new QComboBox;
    devices_->addItem("Detecting USBTMC devices...");
    devices_->setMinimumWidth(300);
    scan_ = button("Scan Devices");
    connect_ = button("Connect");
    disconnect_ = button("Disconnect");
    connectionState_ = new QLabel("DISCONNECTED");
    connectionState_->setAlignment(Qt::AlignCenter);
    connectionState_->setMinimumWidth(120);
    connect_->setStyleSheet(
        "QPushButton{color:white;background:#2F80ED;border-color:#1F6FD1;font-weight:600}"
        "QPushButton:hover{background:#246FCE}"
        "QPushButton:disabled{color:#A5A8AC;background:#F0F1F2;border-color:#D8DBDE}");
    connectionLayout->addWidget(new QLabel("Device:"));
    connectionLayout->addWidget(devices_, 1);
    connectionLayout->addWidget(scan_);
    connectionLayout->addWidget(connect_);
    connectionLayout->addWidget(disconnect_);
    connectionLayout->addStretch();
    connectionLayout->addWidget(new QLabel("Status:"));
    connectionLayout->addWidget(connectionState_);
    root->addWidget(box("Instrument Connection", connectionLayout));

    auto *content = new QHBoxLayout;
    content->setSpacing(10);
    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(7);
    left->setFixedWidth(290);

    // Acquisition buttons.
    auto *acquisition = new QGridLayout;
    run_ = button("Run"); stop_ = button("Stop");
    single_ = button("Single"); autoset_ = button("Autoset");
    run_->setStyleSheet(
        "QPushButton{color:#176B35;background:#EFFAF2;border-color:#36B866;font-weight:600}"
        "QPushButton:hover{background:#DFF5E5}"
        "QPushButton:disabled{color:#A5A8AC;background:#F0F1F2;border-color:#D8DBDE}");
    stop_->setStyleSheet(
        "QPushButton{color:#C62828;background:#FFF4F4;border-color:#F05A5A;font-weight:600}"
        "QPushButton:hover{background:#FFE5E5}"
        "QPushButton:disabled{color:#A5A8AC;background:#F0F1F2;border-color:#D8DBDE}");
    acquisition->addWidget(run_, 0, 0); acquisition->addWidget(stop_, 0, 1);
    acquisition->addWidget(single_, 1, 0); acquisition->addWidget(autoset_, 1, 1);
    leftLayout->addWidget(box("Acquisition Controls", acquisition));

    // Four channels share one compact creation loop.
    auto *channels = new QVBoxLayout;
    channels->setSpacing(4);
    channels->setContentsMargins(0, 0, 0, 0);
    const QStringList scales = {
        "10 mV/div", "20 mV/div", "50 mV/div", "100 mV/div", "200 mV/div",
        "500 mV/div", "1 V/div", "2 V/div", "5 V/div", "10 V/div"
    };
    for (int index = 0; index < 4; ++index) {
        const int channel = index + 1;
        auto *frame = new QFrame;
        frame->setObjectName("channelFrame");
        frame->setFixedHeight(34);
        frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        frame->setStyleSheet(QString(
            "QFrame#channelFrame{background:#FFF;border:1px solid %1;border-radius:5px}"
            "QFrame#channelFrame QCheckBox,QFrame#channelFrame QLabel{"
            "background:transparent;border:none}"
            "QFrame#channelFrame QComboBox{background:#F8F9FA;border:1px solid #C9CDD3;"
            "border-radius:4px;padding:2px 6px;min-height:0px}"
            "QFrame#channelFrame QComboBox:disabled{color:#9AA0A6;background:#ECEFF1}"
        ).arg(colors_[channel].name()));
        auto *row = new QHBoxLayout(frame);
        row->setContentsMargins(6, 3, 6, 3);
        row->setSpacing(5);
        channelOn_[index] = new QCheckBox;
        channelOn_[index]->setFixedWidth(18);
        auto *label = new QLabel(QString("CH%1").arg(channel));
        label->setFixedWidth(38);
        label->setStyleSheet(QString("color:%1;font-weight:700").arg(colors_[channel].name()));
        channelScale_[index] = new QComboBox;
        channelScale_[index]->addItems(scales);
        channelScale_[index]->setCurrentText("1 V/div");
        channelScale_[index]->setFixedHeight(26);
        channelScale_[index]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(channelOn_[index]); row->addWidget(label);
        row->addWidget(channelScale_[index], 1);
        channels->addWidget(frame);
    }
    leftLayout->addWidget(box("Channels", channels));

    auto *horizontal = new QHBoxLayout;
    timeScale_ = new QComboBox;
    timeScale_->addItems({
        "1 us/div", "2 us/div", "5 us/div", "10 us/div", "20 us/div", "50 us/div",
        "100 us/div", "200 us/div", "500 us/div", "1 ms/div", "2 ms/div", "5 ms/div",
        "10 ms/div", "20 ms/div", "50 ms/div", "100 ms/div", "200 ms/div",
        "500 ms/div", "1 s/div"
    });
    timeScale_->setCurrentText("1 ms/div");
    horizontal->addWidget(new QLabel("Time/Div:"));
    horizontal->addWidget(timeScale_, 1);
    leftLayout->addWidget(box("Horizontal Settings", horizontal));

    auto *exports = new QVBoxLayout;
    savePlot_ = button("Save Plot Image");
    saveScreen_ = button("Save Scope Screenshot");
    saveCsv_ = button("Save Waveform CSV");
    exports->addWidget(savePlot_); exports->addWidget(saveScreen_); exports->addWidget(saveCsv_);
    leftLayout->addWidget(box("Export", exports));
    leftLayout->addStretch();
    content->addWidget(left);

    // Waveform above a resizable SCPI/log area.
    auto *right = new QSplitter(Qt::Vertical);
    right->setChildrenCollapsible(false);
    right->setHandleWidth(5);
    auto *waveLayout = new QVBoxLayout;
    waveLayout->setContentsMargins(9, 2, 9, 8);
    waveLayout->setSpacing(4);
    auto *refreshRow = new QHBoxLayout;
    refreshRow->addStretch();
    refresh_ = button("Update Graph");
    refresh_->setFixedSize(112, 30);
    refreshRow->addWidget(refresh_);
    plot_ = new WaveformWidget;
    waveLayout->addLayout(refreshRow);
    waveLayout->addWidget(plot_, 1);
    auto *waveBox = box("Waveform", waveLayout);
    waveBox->setMinimumHeight(390);
    right->addWidget(waveBox);

    auto *bottom = new QWidget;
    auto *bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(10);
    auto *scpi = new QGridLayout;
    scpi->setContentsMargins(0, 0, 0, 0);
    scpi->setHorizontalSpacing(6);
    scpi->setVerticalSpacing(3);
    scpi->setColumnStretch(1, 1);
    scpi->setRowStretch(0, 0);
    scpi->setRowStretch(1, 1);
    command_ = new QLineEdit("*IDN?");
    command_->setFixedHeight(28);
    command_->setStyleSheet("min-height:0;padding:2px 6px");
    send_ = button("Send");
    send_->setFixedHeight(28);
    send_->setStyleSheet("min-height:0;max-height:28px");
    response_ = new QPlainTextEdit;
    response_->setReadOnly(true);
    response_->setPlaceholderText("The oscilloscope response will be displayed here.");
    response_->setMinimumHeight(54);
    response_->setMaximumHeight(72);
    response_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    response_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    clearResponse_ = button("Clear Response");
    clearResponse_->setFixedHeight(28);
    clearResponse_->setStyleSheet("min-height:0;max-height:28px");
    auto *commandLabel = new QLabel("SCPI Command:");
    auto *responseLabel = new QLabel("Instrument Response:");
    scpi->addWidget(commandLabel, 0, 0, Qt::AlignVCenter);
    scpi->addWidget(command_, 0, 1);
    scpi->addWidget(send_, 0, 2, Qt::AlignVCenter);
    scpi->addWidget(responseLabel, 1, 0, Qt::AlignTop);
    scpi->addWidget(response_, 1, 1);
    scpi->addWidget(clearResponse_, 1, 2, Qt::AlignTop);

    auto *logLayout = new QVBoxLayout;
    activity_ = new QPlainTextEdit;
    activity_->setReadOnly(true);
    activity_->setPlaceholderText("Program activity and error messages will appear here.");
    activity_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    logLayout->addWidget(activity_);
    bottomLayout->addWidget(box("SCPI Console", scpi), 3);
    bottomLayout->addWidget(box("Activity / Error Log", logLayout), 2);
    bottom->setMinimumHeight(130);
    bottom->setMaximumHeight(185);
    right->addWidget(bottom);
    right->setStretchFactor(0, 4);
    right->setStretchFactor(1, 1);
    right->setSizes({560, 150});
    content->addWidget(right, 1);
    root->addLayout(content, 1);

    setCentralWidget(central);
    instrument_ = new QLabel("Instrument: Not connected");
    ready_ = new QLabel("Ready");
    statusBar()->addWidget(instrument_, 1);
    statusBar()->addPermanentWidget(ready_);
}

void MainWindow::wireUi()
{
    connect(scan_, &QPushButton::clicked, this, [this] { scanDevices(); });
    connect(connect_, &QPushButton::clicked, this, [this] { connectScope(); });
    connect(disconnect_, &QPushButton::clicked, this, [this] { disconnectScope(); });
    connect(run_, &QPushButton::clicked, this, [this] { acquire("Run"); });
    connect(stop_, &QPushButton::clicked, this, [this] { acquire("Stop"); });
    connect(single_, &QPushButton::clicked, this, [this] { acquire("Single"); });
    connect(autoset_, &QPushButton::clicked, this, [this] { acquire("Autoset"); });
    connect(refresh_, &QPushButton::clicked, this, [this] { updateGraph(); });
    connect(send_, &QPushButton::clicked, this, [this] { sendScpi(); });
    connect(command_, &QLineEdit::returnPressed, this, [this] { sendScpi(); });
    connect(clearResponse_, &QPushButton::clicked, response_, &QPlainTextEdit::clear);
    connect(savePlot_, &QPushButton::clicked, this, [this] { savePlot(); });
    connect(saveScreen_, &QPushButton::clicked, this, [this] { saveScreenshot(); });
    connect(saveCsv_, &QPushButton::clicked, this, [this] { saveCsv(); });
    connect(timeScale_, &QComboBox::currentTextChanged, this,
            [this](const QString &label) { setTimeScale(label); });
    for (int index = 0; index < 4; ++index) {
        connect(channelOn_[index], &QCheckBox::toggled, this,
                [this, channel = index + 1](bool on) { setChannelVisible(channel, on); });
        connect(channelScale_[index], &QComboBox::currentTextChanged, this,
                [this, channel = index + 1](const QString &label) { setChannelScale(channel, label); });
    }
}

bool MainWindow::guard(const QString &action, const std::function<void()> &work, bool popup)
{
    try { work(); return true; }
    catch (const std::exception &e) { error(action, what(e), popup); return false; }
}

void MainWindow::scanDevices()
{
    QStringList found;
    { BusyCursor busy; found = scope_.scanDevices(); }
    devices_->clear();
    if (found.isEmpty()) {
        devices_->addItem("No USBTMC device detected");
        connect_->setEnabled(false);
        ready_->setText("No device");
        log("No USBTMC device found. Use the scope USB DEVICE port, attach it "
            "to Ubuntu/VM, load the usbtmc driver, and check /dev/usbtmc*.");
        return;
    }
    devices_->addItems(found);
    connect_->setEnabled(!scope_.isConnected());
    ready_->setText("Device found");
    log(QString("Found %1 USBTMC device(s): %2").arg(found.size()).arg(found.join(", ")));
}

void MainWindow::connectScope()
{
    QString path = devices_->currentText();
    if (!path.startsWith("/dev/usbtmc")) {
        scanDevices();
        path = devices_->currentText();
        if (!path.startsWith("/dev/usbtmc")) return;
    }
    QString id;
    const bool connected = guard("Connect", [&, this] {
        { BusyCursor busy; id = scope_.connectDevice(path); }
        setConnected(true);
        response_->appendPlainText("< " + id);
        log("Connected: " + id);
        ready_->setText("Connected");
        QMessageBox::information(this, "Connection Successful",
                                 QString("Connected to:\n%1\n\n%2").arg(path, id));
    });
    if (!connected) {
        scope_.disconnect();
        setConnected(false);
        return;
    }

    // Identification is enough to establish a valid connection. A model may
    // reject one optional channel/status query, so synchronization failures are
    // reported without closing an otherwise healthy USBTMC session.
    if (!guard("Synchronize Controls", [this] { syncControls(); }, false))
        log("Connected, but some front-panel settings could not be synchronized.");
    if (!guard("Trigger Status", [this] {
            setRunning(scope_.triggerStatus() != "STOP");
        }, false))
        setRunning(false, true);
}

void MainWindow::disconnectScope()
{
    const bool wasConnected = scope_.isConnected();
    scope_.disconnect();
    waveforms_.clear();
    plot_->clearWaveforms();
    setConnected(false);
    log("Oscilloscope disconnected.");
    ready_->setText("Disconnected");
    if (wasConnected)
        QMessageBox::information(this, "Disconnected", "The oscilloscope has been disconnected.");
}

void MainWindow::setConnected(bool connected)
{
    scan_->setEnabled(!connected);
    devices_->setEnabled(!connected);
    connect_->setEnabled(!connected && devices_->currentText().startsWith("/dev/usbtmc"));
    disconnect_->setEnabled(connected);
    const std::array<QWidget *, 7> connectedWidgets = {
        single_, autoset_, refresh_, send_, saveScreen_, command_, timeScale_
    };
    for (QWidget *widget : connectedWidgets)
        widget->setEnabled(connected);
    for (int index = 0; index < 4; ++index) {
        channelOn_[index]->setEnabled(connected);
        channelScale_[index]->setEnabled(connected);
    }
    savePlot_->setEnabled(connected && !waveforms_.isEmpty());
    saveCsv_->setEnabled(connected && !waveforms_.isEmpty());
    setRunning(false, connected);
    connectionState_->setText(connected ? "CONNECTED" : "DISCONNECTED");
    connectionState_->setStyleSheet(connected
        ? "color:#14833B;background:#F3FFF6;border:1px solid #39B969;border-radius:5px;font-weight:700;padding:8px"
        : "color:#E02020;background:#FFF8F8;border:1px solid #F04B4B;border-radius:5px;font-weight:700;padding:8px");
    instrument_->setText(connected ? "Instrument: " + scope_.identification()
                                   : "Instrument: Not connected");
}

void MainWindow::setRunning(bool running, bool connected)
{
    running_ = running;
    run_->setEnabled(connected && !running);
    stop_->setEnabled(connected && running);
}

QString MainWindow::closestLabel(double value, const QMap<QString, double> &choices) const
{
    QString best;
    double distance = std::numeric_limits<double>::max();
    for (auto item = choices.cbegin(); item != choices.cend(); ++item) {
        const double next = std::abs(item.value() - value);
        if (next < distance) { best = item.key(); distance = next; }
    }
    return best;
}

void MainWindow::syncControls()
{
    syncing_ = true;
    try {
        for (int index = 0; index < 4; ++index) {
            const int channel = index + 1;
            channelOn_[index]->setChecked(scope_.channelDisplay(channel));
            channelScale_[index]->setCurrentText(closestLabel(scope_.channelScale(channel), volts_));
        }
        timeScale_->setCurrentText(closestLabel(scope_.timeScale(), times_));
    } catch (...) { syncing_ = false; throw; }
    syncing_ = false;
}

void MainWindow::acquire(const QString &action)
{
    guard(action, [&, this] {
        if (action == "Run") { scope_.run(); setRunning(true); log("Scope running."); }
        else if (action == "Stop") { scope_.stop(); setRunning(false); log("Scope stopped."); }
        else if (action == "Single") {
            scope_.single(); setRunning(true); singlePolls_ = 0;
            log("Single acquisition armed; waiting for a trigger...");
            QTimer::singleShot(250, this, [this] { pollSingle(); });
        } else {
            scope_.autoSet(); setRunning(true); log("Autoset started...");
            QTimer::singleShot(4000, this, [this] { finishAutoset(); });
        }
        ready_->setText(action);
    });
}

void MainWindow::pollSingle()
{
    if (!scope_.isConnected()) return;
    guard("Single Status", [this] {
        const QString status = scope_.triggerStatus();
        ++singlePolls_;
        if (status == "STOP") {
            setRunning(false);
            log("Single acquisition completed; scope stopped.");
            QTimer::singleShot(300, this, [this] { updateGraph(); });
        } else if (singlePolls_ < 40)
            QTimer::singleShot(250, this, [this] { pollSingle(); });
        else log("Still waiting for a trigger; existing graph was kept.");
    }, false);
}

void MainWindow::finishAutoset()
{
    if (!scope_.isConnected()) return;
    guard("Autoset Sync", [this] {
        syncControls();
        setRunning(scope_.triggerStatus() != "STOP");
        log("Autoset completed; settings synchronized.");
        updateGraph();
    });
}

void MainWindow::setChannelVisible(int channel, bool visible)
{
    if (syncing_ || !scope_.isConnected()) return;
    guard(QString("CH%1 Display").arg(channel), [this, channel, visible] {
        scope_.setChannelDisplay(channel, visible);
        log(QString("CH%1 display %2.").arg(channel).arg(visible ? "ON" : "OFF"));
        scheduleRefresh();
    });
}

void MainWindow::setChannelScale(int channel, const QString &label)
{
    if (syncing_ || !scope_.isConnected()) return;
    guard(QString("CH%1 Scale").arg(channel), [this, channel, label] {
        scope_.setChannelScale(channel, volts_.value(label));
        log(QString("CH%1 scale set to %2.").arg(channel).arg(label));
        scheduleRefresh();
    });
}

void MainWindow::setTimeScale(const QString &label)
{
    if (syncing_ || !scope_.isConnected()) return;
    guard("Time Scale", [this, label] {
        const double scale = times_.value(label);
        scope_.setTimeScale(scale);
        log("Time scale set to " + label + ".");
        scheduleRefresh(std::clamp(static_cast<int>(scale * 12000) + 300, 500, 5000));
    });
}

void MainWindow::scheduleRefresh(int delayMs)
{
    if (scope_.isConnected()) refreshTimer_->start(delayMs);
}

bool MainWindow::updateGraph()
{
    if (!scope_.isConnected()) { log("Update Graph failed: connect the oscilloscope first."); return false; }
    bool success = false;
    guard("Update Graph", [&, this] {
        BusyCursor busy;
        ready_->setText("Reading waveform...");
        syncControls();
        QList<int> channels;
        for (int index = 0; index < 4; ++index)
            if (channelOn_[index]->isChecked()) channels << index + 1;
        if (channels.isEmpty()) throw std::runtime_error("Enable at least one channel.");

        QMap<int, WaveformData> acquired;
        QStringList failures;
        log("Reading waveform data...");
        for (int channel : channels) {
            try {
                acquired[channel] = scope_.acquireWaveform(channel);
                log(QString("CH%1: %2 points received.").arg(channel).arg(acquired[channel].pointCount()));
            } catch (const std::exception &e) {
                failures << QString("CH%1: %2").arg(channel).arg(what(e));
                log("Waveform error - " + failures.constLast());
            }
        }
        if (acquired.isEmpty())
            throw std::runtime_error(("No channel returned waveform data. " + failures.join("; ")).toStdString());
        waveforms_ = acquired;
        drawWaveforms();
        savePlot_->setEnabled(true);
        saveCsv_->setEnabled(true);
        log("Waveform updated.");
        if (!failures.isEmpty()) log("Available channels were kept; failures: " + failures.join("; "));
        ready_->setText("Waveform updated");
        success = true;
    });
    return success;
}

void MainWindow::drawWaveforms()
{
    QMap<int, double> scales;
    for (int channel : waveforms_.keys()) scales[channel] = scope_.channelScale(channel);
    plot_->setWaveforms(waveforms_, scope_.timeScale(), scales);
}

void MainWindow::sendScpi()
{
    const QString command = command_->text().trimmed();
    guard("SCPI Command", [&, this] {
        response_->appendPlainText("> " + command);
        QString reply;
        { BusyCursor busy; reply = scope_.sendScpi(command); }
        response_->appendPlainText((command.contains('?') ? "< " : "") + reply);
        QString action = command.toUpper();
        action.remove(':'); action = action.section(' ', 0, 0);
        if (!command.contains('?')) {
            if (action == "RUN") setRunning(true);
            else if (action == "STOP") setRunning(false);
            else if (action.startsWith("SING")) setRunning(true);
        }
        log("SCPI: " + command);
        ready_->setText("Command completed");
    });
}

QString MainWindow::savePath(const QString &caption, const QString &prefix,
                             const QString &suffix, const QString &filter)
{
    const QString suggested = QString("%1_%2%3").arg(
        prefix, QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"), suffix);
    QString path = QFileDialog::getSaveFileName(this, caption, suggested, filter);
    if (!path.isEmpty() && QFileInfo(path).suffix().isEmpty()) path += suffix;
    return path;
}

void MainWindow::savePlot()
{
    if (waveforms_.isEmpty()) { log("Save Plot Image failed: update the graph first."); return; }
    const QString path = savePath("Save Plot Image", "rigol_graph", ".png", "PNG (*.png)");
    if (path.isEmpty()) return;
    QImage image(plot_->size() * 2, QImage::Format_ARGB32);
    image.setDevicePixelRatio(2); image.fill(Qt::transparent);
    QPainter painter(&image); plot_->render(&painter); painter.end();
    if (!image.save(path, "PNG")) { error("Save Plot Image", "Qt could not write the PNG file."); return; }
    log("Plot image saved: " + path);
}

void MainWindow::saveScreenshot()
{
    const QString path = savePath("Save Scope Screenshot", "rigol_scope", ".png", "PNG (*.png)");
    if (path.isEmpty()) return;
    guard("Save Scope Screenshot", [&, this] {
        QByteArray data;
        { BusyCursor busy; data = scope_.captureScreenshot(); }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size())
            throw std::runtime_error("Could not write the oscilloscope PNG file.");
        log("Oscilloscope screenshot saved: " + path);
    });
}

void MainWindow::saveCsv()
{
    if (waveforms_.isEmpty() && !updateGraph()) return;
    const QString path = savePath("Save Waveform CSV", "rigol_waveform", ".csv", "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error("Save Waveform CSV", "Could not create the CSV file."); return;
    }
    QTextStream out(&file);
    out << "Sample";
    for (int channel : waveforms_.keys()) out << ",Time_CH" << channel << "_s,Voltage_CH" << channel << "_V";
    out << '\n';
    qsizetype count = std::numeric_limits<qsizetype>::max();
    for (const WaveformData &waveform : waveforms_) count = std::min(count, waveform.pointCount());
    for (qsizetype sample = 0; sample < count; ++sample) {
        out << sample;
        for (int channel : waveforms_.keys()) {
            const auto &waveform = waveforms_[channel];
            out << ',' << QString::number(waveform.time[sample], 'g', 16)
                << ',' << QString::number(waveform.voltage[sample], 'g', 16);
        }
        out << '\n';
    }
    log("Waveform CSV saved: " + path);
}

void MainWindow::error(const QString &action, const QString &details, bool popup)
{
    const QString message = action + " failed: " + details;
    log(message);
    ready_->setText("Error");
    if (popup) QMessageBox::warning(this, action, message);
}

void MainWindow::log(const QString &message)
{
    activity_->appendPlainText(QString("[%1] %2").arg(
        QDateTime::currentDateTime().toString("HH:mm:ss"), message));
    activity_->verticalScrollBar()->setValue(activity_->verticalScrollBar()->maximum());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    scope_.disconnect();
    event->accept();
}
