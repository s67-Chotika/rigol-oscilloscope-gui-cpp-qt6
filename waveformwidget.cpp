#include "waveformwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);
}

void WaveformWidget::clearWaveforms()
{
    waveforms_.clear();
    voltsPerDivision_.clear();
    update();
}

void WaveformWidget::setWaveforms(const QMap<int, WaveformData> &waveforms,
                                  double secondsPerDivision,
                                  const QMap<int, double> &voltsPerDivision)
{
    waveforms_ = waveforms;
    voltsPerDivision_ = voltsPerDivision;
    secondsPerDivision_ = secondsPerDivision;
    update();
}

QColor WaveformWidget::channelColor(int channel) const
{
    switch (channel) {
    case 1: return QColor("#F5C542");
    case 2: return QColor("#00CFE8");
    case 3: return QColor("#F238A6");
    case 4: return QColor("#1E88E5");
    default: return QColor("#FFFFFF");
    }
}

void WaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#151D25"));

    const int leftMargin = 54;
    const int rightMargin = 18;
    const int topMargin = 18;
    const int bottomMargin = 43;
    const QRectF graph(leftMargin,
                       topMargin,
                       std::max(1, width() - leftMargin - rightMargin),
                       std::max(1, height() - topMargin - bottomMargin));

    // Draw the same 12 horizontal and 8 vertical screen divisions as the scope.
    painter.setPen(QPen(QColor(91, 102, 112, 170), 1.0));
    for (int division = 0; division <= 12; ++division) {
        const qreal x = graph.left() + graph.width() * division / 12.0;
        painter.drawLine(QPointF(x, graph.top()), QPointF(x, graph.bottom()));
    }
    for (int division = 0; division <= 8; ++division) {
        const qreal y = graph.top() + graph.height() * division / 8.0;
        painter.drawLine(QPointF(graph.left(), y), QPointF(graph.right(), y));
    }

    // Highlight the trigger/time-zero axis in orange.
    QPen centerPen(QColor("#F0A000"), 1.2, Qt::DashLine);
    painter.setPen(centerPen);
    painter.drawLine(QPointF(graph.center().x(), graph.top()),
                     QPointF(graph.center().x(), graph.bottom()));

    painter.setPen(QPen(QColor("#87919A"), 1.0));
    painter.drawRect(graph);

    painter.setPen(QColor("#CDD3D8"));
    painter.drawText(QRectF(graph.left(), graph.bottom() + 10, graph.width(), 24),
                     Qt::AlignCenter,
                     "Horizontal divisions");

    painter.save();
    painter.translate(17, graph.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-graph.height() / 2.0, -12, graph.height(), 24),
                     Qt::AlignCenter,
                     "Vertical divisions");
    painter.restore();

    // Clip traces so extreme samples cannot paint over labels or controls.
    painter.save();
    painter.setClipRect(graph.adjusted(1, 1, -1, -1));

    for (auto iterator = waveforms_.cbegin(); iterator != waveforms_.cend(); ++iterator) {
        const int channel = iterator.key();
        const WaveformData &waveform = iterator.value();
        const double voltsPerDivision = voltsPerDivision_.value(channel, 1.0);
        if (secondsPerDivision_ <= 0.0 || voltsPerDivision <= 0.0)
            continue;

        const qsizetype points = std::min(waveform.time.size(),
                                         waveform.displayVoltage.size());
        if (points < 2)
            continue;

        QPainterPath path;
        bool started = false;
        for (qsizetype index = 0; index < points; ++index) {
            const double xDivision = waveform.time.at(index) / secondsPerDivision_;
            const double yDivision = waveform.displayVoltage.at(index) / voltsPerDivision;
            const QPointF point(graph.center().x() + xDivision * graph.width() / 12.0,
                                graph.center().y() - yDivision * graph.height() / 8.0);
            if (!started) {
                path.moveTo(point);
                started = true;
            } else {
                path.lineTo(point);
            }
        }

        painter.setPen(QPen(channelColor(channel), 1.35));
        painter.drawPath(path);
    }
    painter.restore();

    // Display a compact channel legend only when waveform data exists.
    if (!waveforms_.isEmpty()) {
        int legendX = static_cast<int>(graph.left()) + 8;
        const int legendY = static_cast<int>(graph.top()) + 8;
        for (auto iterator = waveforms_.cbegin(); iterator != waveforms_.cend(); ++iterator) {
            const int channel = iterator.key();
            const QString text = QString("CH%1  %2 V/div")
                                     .arg(channel)
                                     .arg(voltsPerDivision_.value(channel, 1.0), 0, 'g', 4);
            const int itemWidth = painter.fontMetrics().horizontalAdvance(text) + 22;
            painter.fillRect(QRect(legendX, legendY, itemWidth, 23), QColor(17, 24, 32, 220));
            painter.setPen(channelColor(channel));
            painter.drawText(QRect(legendX + 8, legendY, itemWidth - 12, 23),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             text);
            legendX += itemWidth + 6;
        }
    }
}

