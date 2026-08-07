#pragma once

#include <QColor>
#include <QMap>
#include <QWidget>

#include "scopecontroller.h"

// Draws an oscilloscope-style 12 x 8 division grid without Qt Charts.
class WaveformWidget : public QWidget
{
public:
    explicit WaveformWidget(QWidget *parent = nullptr);

    void clearWaveforms();
    void setWaveforms(const QMap<int, WaveformData> &waveforms,
                      double secondsPerDivision,
                      const QMap<int, double> &voltsPerDivision);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor channelColor(int channel) const;

    QMap<int, WaveformData> waveforms_;
    QMap<int, double> voltsPerDivision_;
    double secondsPerDivision_ = 0.001;
};