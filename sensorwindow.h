#ifndef SENSORWINDOW_H
#define SENSORWINDOW_H

#include <QWidget>
#include "dataparser.h"

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>

namespace Ui {
class SensorWindow;
}

class SensorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SensorWindow(QWidget *parent = nullptr);
    ~SensorWindow();
    void updateData(const SensorData &data);  // 更新传感器数据
    void clearData();                         // 清空数据
    void clearChart();                        // 清空图表
    SensorData getCurrentData() const { return currentData; }  // 获取当前数据

signals:
    void dataUpdated(const SensorData &data);  // 数据更新信号
    void windowClosed();                       // 窗口关闭信号

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Ui::SensorWindow *ui;
    SensorData currentData;

    void setupUI();                         // UI设置
    void setupChart();                      // 初始化折线图
    void updateProgressBars();              // 根据 currentData 更新进度条
    void updateTemperatureStyle(float temperature);
    void updateHumidityStyle(float humidity);
    void updateStatusIndicator();           // 更新状态指示灯

    QChart *m_chart;
    QLineSeries *m_tempSeries;
    QLineSeries *m_humSeries;
    QDateTimeAxis *m_axisX;
    QValueAxis *m_axisYTemp;
    QValueAxis *m_axisYHum;
};

#endif // SENSORWINDOW_H
