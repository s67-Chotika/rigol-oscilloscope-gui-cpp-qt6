#pragma once

#include <QMainWindow>

#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QWidget;


class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    // Creates every section of the oscilloscope control panel.
    void buildUi();

    // Enables or disables controls that require an oscilloscope connection.
    void setHardwareControlsEnabled(bool enabled);

    QComboBox *deviceCombo_ = nullptr;
    QPushButton *scanButton_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *disconnectButton_ = nullptr;
    QLabel *connectionStatus_ = nullptr;

    QPushButton *runButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    QPushButton *singleButton_ = nullptr;
    QPushButton *autosetButton_ = nullptr;

    std::array<QCheckBox *, 4> channelChecks_{};
    std::array<QComboBox *, 4> channelScales_{};
    QComboBox *timeScale_ = nullptr;

    QPushButton *savePlotButton_ = nullptr;
    QPushButton *saveScreenButton_ = nullptr;
    QPushButton *saveCsvButton_ = nullptr;

    QPushButton *updateGraphButton_ = nullptr;

    QLineEdit *scpiCommand_ = nullptr;
    QPushButton *sendButton_ = nullptr;
    QPlainTextEdit *instrumentResponse_ = nullptr;
    QPushButton *clearResponseButton_ = nullptr;

    QPlainTextEdit *activityLog_ = nullptr;
    QLabel *instrumentLabel_ = nullptr;
    QLabel *readyLabel_ = nullptr;
};