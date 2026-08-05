#include "mainwindow.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>


namespace {

// Creates a group box and places the supplied layout inside it.
QGroupBox *createGroup(const QString &title, QLayout *layout)
{
    auto *group = new QGroupBox(title);
    group->setLayout(layout);
    return group;
}


// Creates a standard button used throughout the interface.
QPushButton *createButton(const QString &text)
{
    return new QPushButton(text);
}


// Temporary waveform area for Commit 2.
// Real waveform acquisition and drawing will be added in a later commit.
class WaveformPanel final : public QWidget
{
public:
    explicit WaveformPanel(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(320);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#111820"));

        // Draw the oscilloscope-style grid.
        painter.setPen(QPen(QColor("#3D4853"), 1));

        constexpr int horizontalDivisions = 12;
        constexpr int verticalDivisions = 8;

        for (int division = 0; division <= horizontalDivisions; ++division) {
            const int x = division * width() / horizontalDivisions;
            painter.drawLine(x, 0, x, height());
        }

        for (int division = 0; division <= verticalDivisions; ++division) {
            const int y = division * height() / verticalDivisions;
            painter.drawLine(0, y, width(), y);
        }

        // Draw brighter centre axes.
        painter.setPen(QPen(QColor("#7A8794"), 1));
        painter.drawLine(width() / 2, 0, width() / 2, height());
        painter.drawLine(0, height() / 2, width(), height() / 2);
    }
};

} // namespace


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("RIGOL Oscilloscope Control Panel - Qt6 C++");
    resize(1200, 760);
    setMinimumSize(960, 640);

    // Apply one consistent appearance to the whole application.
    setStyleSheet(R"(
        QMainWindow,
        QWidget {
            background: #F5F6F7;
            color: #202124;
            font-family: "Segoe UI";
            font-size: 11px;
        }

        QGroupBox {
            background: #FAFAFA;
            border: 1px solid #D7DADF;
            border-radius: 7px;
            font-weight: 600;
            margin-top: 11px;
            padding: 11px 9px 9px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
            background: #F5F6F7;
        }

        QPushButton {
            min-height: 34px;
            padding: 0 12px;
            background: white;
            border: 1px solid #C9CDD3;
            border-radius: 5px;
        }

        QPushButton:hover {
            background: #EEF3F8;
            border-color: #8CA7C2;
        }

        QPushButton:disabled {
            color: #A5A8AC;
            background: #F0F1F2;
            border-color: #D8DBDE;
        }

        QComboBox,
        QLineEdit,
        QPlainTextEdit {
            background: white;
            border: 1px solid #C9CDD3;
            border-radius: 4px;
            padding: 5px 7px;
        }

        QComboBox,
        QLineEdit {
            min-height: 25px;
        }

        QComboBox:disabled,
        QLineEdit:disabled {
            color: #9AA0A6;
            background: #ECEFF1;
            border-color: #D8DBDE;
        }

        QStatusBar {
            background: #F4F5F6;
            border-top: 1px solid #D7DADF;
        }
    )");

    buildUi();
    setHardwareControlsEnabled(false);

    activityLog_->appendPlainText(
        "UI layout ready. Oscilloscope communication "
        "will be added in the next commit."
    );
}


void MainWindow::buildUi()
{
    auto *centralWidget = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(centralWidget);

    rootLayout->setContentsMargins(12, 8, 12, 6);
    rootLayout->setSpacing(8);

    /*
     * Instrument connection section
     */
    auto *connectionLayout = new QHBoxLayout;
    connectionLayout->setSpacing(8);

    deviceCombo_ = new QComboBox;
    deviceCombo_->addItem("No device detected");
    deviceCombo_->setMinimumWidth(300);

    scanButton_ = createButton("Scan Devices");
    connectButton_ = createButton("Connect");
    disconnectButton_ = createButton("Disconnect");

    connectionStatus_ = new QLabel("DISCONNECTED");
    connectionStatus_->setAlignment(Qt::AlignCenter);
    connectionStatus_->setMinimumWidth(120);
    connectionStatus_->setStyleSheet(
        "color: #D32F2F;"
        "border: 1px solid #EF5350;"
        "border-radius: 5px;"
        "padding: 8px;"
        "font-weight: 700;"
        "background: #FFF5F5;"
    );

    connectionLayout->addWidget(new QLabel("Device:"));
    connectionLayout->addWidget(deviceCombo_, 1);
    connectionLayout->addWidget(scanButton_);
    connectionLayout->addWidget(connectButton_);
    connectionLayout->addWidget(disconnectButton_);
    connectionLayout->addStretch();
    connectionLayout->addWidget(new QLabel("Status:"));
    connectionLayout->addWidget(connectionStatus_);

    rootLayout->addWidget(
        createGroup("Instrument Connection", connectionLayout)
    );

    /*
     * Main content area
     */
    auto *contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(10);

    auto *leftPanel = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPanel);

    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(7);
    leftPanel->setFixedWidth(285);

    /*
     * Acquisition controls
     */
    auto *acquisitionLayout = new QGridLayout;

    runButton_ = createButton("Run");
    stopButton_ = createButton("Stop");
    singleButton_ = createButton("Single");
    autosetButton_ = createButton("Autoset");

    acquisitionLayout->addWidget(runButton_, 0, 0);
    acquisitionLayout->addWidget(stopButton_, 0, 1);
    acquisitionLayout->addWidget(singleButton_, 1, 0);
    acquisitionLayout->addWidget(autosetButton_, 1, 1);

    leftLayout->addWidget(
        createGroup("Acquisition Controls", acquisitionLayout)
    );

    /*
     * Channel controls
     */
    auto *channelsLayout = new QVBoxLayout;

    const QStringList voltageScales = {
        "10 mV/div",
        "20 mV/div",
        "50 mV/div",
        "100 mV/div",
        "200 mV/div",
        "500 mV/div",
        "1 V/div",
        "2 V/div",
        "5 V/div",
        "10 V/div"
    };

    const std::array<QString, 4> channelColors = {
        "#F5C542",
        "#00B8D4",
        "#F238A6",
        "#1E88E5"
    };

    for (int index = 0; index < 4; ++index) {
        auto *channelFrame = new QFrame;

        channelFrame->setStyleSheet(
            QString(
                "QFrame {"
                "background: white;"
                "border: 1px solid %1;"
                "border-radius: 5px;"
                "}"
                "QCheckBox, QLabel, QComboBox {"
                "border: none;"
                "}"
            ).arg(channelColors[index])
        );

        auto *channelLayout = new QHBoxLayout(channelFrame);
        channelLayout->setContentsMargins(7, 5, 7, 5);

        channelChecks_[index] = new QCheckBox;

        auto *channelLabel =
            new QLabel(QString("CH%1").arg(index + 1));

        channelLabel->setMinimumWidth(35);
        channelLabel->setStyleSheet(
            QString("color: %1; font-weight: 700;")
                .arg(channelColors[index])
        );

        channelScales_[index] = new QComboBox;
        channelScales_[index]->addItems(voltageScales);
        channelScales_[index]->setCurrentText("1 V/div");

        channelLayout->addWidget(channelChecks_[index]);
        channelLayout->addWidget(channelLabel);
        channelLayout->addWidget(channelScales_[index], 1);

        channelsLayout->addWidget(channelFrame);
    }

    leftLayout->addWidget(createGroup("Channels", channelsLayout));

    /*
     * Horizontal settings
     */
    auto *horizontalLayout = new QHBoxLayout;

    timeScale_ = new QComboBox;
    timeScale_->addItems({
        "1 us/div",
        "2 us/div",
        "5 us/div",
        "10 us/div",
        "20 us/div",
        "50 us/div",
        "100 us/div",
        "200 us/div",
        "500 us/div",
        "1 ms/div",
        "2 ms/div",
        "5 ms/div",
        "10 ms/div",
        "20 ms/div",
        "50 ms/div",
        "100 ms/div",
        "200 ms/div",
        "500 ms/div",
        "1 s/div"
    });

    timeScale_->setCurrentText("1 ms/div");

    horizontalLayout->addWidget(new QLabel("Time/Div:"));
    horizontalLayout->addWidget(timeScale_, 1);

    leftLayout->addWidget(
        createGroup("Horizontal Settings", horizontalLayout)
    );

    /*
     * Export controls
     */
    auto *exportLayout = new QVBoxLayout;

    savePlotButton_ = createButton("Save Plot Image");
    saveScreenButton_ = createButton("Save Scope Screenshot");
    saveCsvButton_ = createButton("Save Waveform CSV");

    exportLayout->addWidget(savePlotButton_);
    exportLayout->addWidget(saveScreenButton_);
    exportLayout->addWidget(saveCsvButton_);

    leftLayout->addWidget(createGroup("Export", exportLayout));
    leftLayout->addStretch();

    contentLayout->addWidget(leftPanel);

    /*
     * Right side: waveform and console/log panels
     */
    auto *rightSplitter = new QSplitter(Qt::Vertical);
    rightSplitter->setChildrenCollapsible(false);
    rightSplitter->setHandleWidth(5);

    /*
     * Waveform section
     */
    auto *waveformLayout = new QVBoxLayout;
    waveformLayout->setContentsMargins(9, 2, 9, 8);
    waveformLayout->setSpacing(4);

    auto *updateRow = new QHBoxLayout;
    updateRow->addStretch();

    updateGraphButton_ = createButton("Update Graph");
    updateGraphButton_->setFixedSize(112, 30);

    updateRow->addWidget(updateGraphButton_);

    auto *waveformPanel = new WaveformPanel;

    waveformLayout->addLayout(updateRow);
    waveformLayout->addWidget(waveformPanel, 1);

    auto *waveformGroup = createGroup("Waveform", waveformLayout);
    waveformGroup->setMinimumHeight(360);

    rightSplitter->addWidget(waveformGroup);

    /*
     * SCPI console
     */
    auto *bottomPanel = new QWidget;
    auto *bottomLayout = new QHBoxLayout(bottomPanel);

    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(10);

    auto *scpiLayout = new QGridLayout;
    scpiLayout->setColumnStretch(1, 1);

    scpiCommand_ = new QLineEdit("*IDN?");
    sendButton_ = createButton("Send");

    instrumentResponse_ = new QPlainTextEdit;
    instrumentResponse_->setReadOnly(true);
    instrumentResponse_->setPlaceholderText(
        "The oscilloscope response will be displayed here."
    );
    instrumentResponse_->setMaximumHeight(58);
    instrumentResponse_->setVerticalScrollBarPolicy(
        Qt::ScrollBarAlwaysOn
    );

    clearResponseButton_ = createButton("Clear Response");

    scpiLayout->addWidget(new QLabel("SCPI Command:"), 0, 0);
    scpiLayout->addWidget(scpiCommand_, 0, 1);
    scpiLayout->addWidget(sendButton_, 0, 2);

    scpiLayout->addWidget(new QLabel("Instrument Response:"), 1, 0);
    scpiLayout->addWidget(instrumentResponse_, 1, 1);
    scpiLayout->addWidget(clearResponseButton_, 1, 2);

    /*
     * Activity and error log
     */
    auto *logLayout = new QVBoxLayout;

    activityLog_ = new QPlainTextEdit;
    activityLog_->setReadOnly(true);
    activityLog_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    activityLog_->setPlaceholderText(
        "Program activity and error messages will appear here."
    );

    logLayout->addWidget(activityLog_);

    bottomLayout->addWidget(
        createGroup("SCPI Console", scpiLayout),
        3
    );

    bottomLayout->addWidget(
        createGroup("Activity / Error Log", logLayout),
        2
    );

    bottomPanel->setMinimumHeight(130);
    bottomPanel->setMaximumHeight(185);

    rightSplitter->addWidget(bottomPanel);
    rightSplitter->setStretchFactor(0, 4);
    rightSplitter->setStretchFactor(1, 1);
    rightSplitter->setSizes({520, 150});

    contentLayout->addWidget(rightSplitter, 1);
    rootLayout->addLayout(contentLayout, 1);

    /*
     * Status bar
     */
    instrumentLabel_ = new QLabel("Instrument: Not connected");
    readyLabel_ = new QLabel("Ready");

    statusBar()->addWidget(instrumentLabel_, 1);
    statusBar()->addPermanentWidget(readyLabel_);

    setCentralWidget(centralWidget);

    // Clearing the response box does not require an oscilloscope.
    connect(
        clearResponseButton_,
        &QPushButton::clicked,
        instrumentResponse_,
        &QPlainTextEdit::clear
    );
}


void MainWindow::setHardwareControlsEnabled(bool enabled)
{
    // Device communication will be implemented in Commit 3.
    deviceCombo_->setEnabled(enabled);
    scanButton_->setEnabled(enabled);
    connectButton_->setEnabled(enabled);
    disconnectButton_->setEnabled(false);

    runButton_->setEnabled(false);
    stopButton_->setEnabled(false);
    singleButton_->setEnabled(false);
    autosetButton_->setEnabled(false);

    for (int index = 0; index < 4; ++index) {
        channelChecks_[index]->setEnabled(false);
        channelScales_[index]->setEnabled(false);
    }

    timeScale_->setEnabled(false);

    savePlotButton_->setEnabled(false);
    saveScreenButton_->setEnabled(false);
    saveCsvButton_->setEnabled(false);

    updateGraphButton_->setEnabled(false);
    scpiCommand_->setEnabled(false);
    sendButton_->setEnabled(false);
}