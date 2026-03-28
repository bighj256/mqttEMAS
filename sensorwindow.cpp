#include "sensorwindow.h"
#include "ui_sensorwindow.h"
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>

SensorWindow::SensorWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SensorWindow)
{
    ui->setupUi(this);

    // 设置窗口属性
    setWindowTitle("环境数据监测");
    setMinimumSize(900, 600);

    // 设置窗口标志
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);

    // 初始化UI
    setupUI();

    // 初始化折线图并加入布局
    setupChart();
    QChartView *chartView = new QChartView(m_chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(250);
    
    // 把 chartView 插入到主布局中 (这里可以插入到靠上或靠下的位置)
    ui->verticalLayout->insertWidget(2, chartView);


    // 初始化数据
    clearData();
}

SensorWindow::~SensorWindow()
{
    delete ui;
}

void SensorWindow::setupUI()
{

    updateProgressBars();

    // 设置字体
    QFont titleFont = ui->titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    ui->titleLabel->setFont(titleFont);

    QFont dataFont;
    dataFont.setPointSize(28);
    dataFont.setBold(true);
    ui->tempValueLabel->setFont(dataFont);
    ui->humidityValueLabel->setFont(dataFont);

    // 设置状态标签
    ui->statusLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #f5f7fa;"
        "   border: 2px solid #d1d9e6;"
        "   border-radius: 10px;"
        "   padding: 5px 15px;"
        "   font-weight: bold;"
        "   color: #7f8c8d;"
        "}"
        );
}

void SensorWindow::setupChart()
{
    m_chart = new QChart();
    m_chart->setTitle("实时温湿度曲线 (最近 60 笔数据)");
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    
    m_tempSeries = new QLineSeries();
    m_tempSeries->setName("温度 (°C)");
    m_humSeries = new QLineSeries();
    m_humSeries->setName("湿度 (%)");
    
    QPen tempPen(QColor("#FF5722"));
    tempPen.setWidth(3);
    m_tempSeries->setPen(tempPen);
    
    QPen humPen(QColor("#2196F3"));
    humPen.setWidth(3);
    m_humSeries->setPen(humPen);

    m_chart->addSeries(m_tempSeries);
    m_chart->addSeries(m_humSeries);

    m_axisX = new QDateTimeAxis();
    m_axisX->setTickCount(5);
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setTitleText("时间");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_tempSeries->attachAxis(m_axisX);
    m_humSeries->attachAxis(m_axisX);

    m_axisYTemp = new QValueAxis();
    m_axisYTemp->setRange(-10, 50);
    m_axisYTemp->setTitleText("温度 (°C)");
    m_axisYTemp->setLabelFormat("%i");
    m_chart->addAxis(m_axisYTemp, Qt::AlignLeft);
    m_tempSeries->attachAxis(m_axisYTemp);

    m_axisYHum = new QValueAxis();
    m_axisYHum->setRange(0, 100);
    m_axisYHum->setTitleText("湿度 (%)");
    m_axisYHum->setLabelFormat("%i");
    m_chart->addAxis(m_axisYHum, Qt::AlignRight);
    m_humSeries->attachAxis(m_axisYHum);
}

//更新数据
void SensorWindow::updateData(const SensorData &data)
{
    currentData = data;

    if (data.isValid) {
        // 更新温度显示
        QString tempColor;
        if (data.temperature < 10) {
            tempColor = "#2196F3";      // 蓝色
        } else if (data.temperature < 25) {
            tempColor = "#4CAF50";      // 绿色
        } else if (data.temperature < 35) {
            tempColor = "#FF9800";      // 橙色
        } else {
            tempColor = "#F44336";      // 红色
        }

        ui->tempValueLabel->setText(
            QString("<span style='color: %1;'>%2°C</span>")
                .arg(tempColor)
                .arg(data.temperature, 0, 'f', 1)
            );

        // 更新湿度显示
        QString humColor;
        if (data.humidity < 30) {
            humColor = "#FF9800";       // 橙色
        } else if (data.humidity < 60) {
            humColor = "#4CAF50";       // 绿色
        } else if (data.humidity < 80) {
            humColor = "#2196F3";       // 蓝色
        } else {
            humColor = "#9C27B0";       // 紫色
        }

        ui->humidityValueLabel->setText(
            QString("<span style='color: %1;'>%2%</span>")
                .arg(humColor)
                .arg(data.humidity, 0, 'f', 1)
            );

        // 更新设备信息
        ui->deviceIdLabel->setText(data.deviceId.isEmpty() ? "未知设备" : data.deviceId);

        // 更新时间
        ui->updateTimeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

        // 更新进度条
        updateProgressBars();

        // 刷新图表
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        m_tempSeries->append(currentTime, data.temperature);
        m_humSeries->append(currentTime, data.humidity);
        
        // 保持只显示最近 60 个点
        if (m_tempSeries->count() > 60) {
            m_tempSeries->remove(0);
            m_humSeries->remove(0);
        }
        
        // 动态调整 X 轴的时间范围
        if (m_tempSeries->count() > 0) {
            qint64 firstTime = (qint64)m_tempSeries->at(0).x();
            qint64 lastTime = (qint64)m_tempSeries->at(m_tempSeries->count() - 1).x();
            m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(firstTime), 
                              QDateTime::fromMSecsSinceEpoch(lastTime).addSecs(2));
        }

        // 更新状态指示器
        updateStatusIndicator();

        // 设置状态文本
        ui->statusLabel->setText("✓ 数据正常");
        ui->statusLabel->setStyleSheet(
            "QLabel {"
            "   background-color: #e8f5e9;"
            "   border: 2px solid #4CAF50;"
            "   border-radius: 10px;"
            "   padding: 5px 15px;"
            "   font-weight: bold;"
            "   color: #2e7d32;"
            "}"
            );

    } else {
        // 无效数据
        ui->tempValueLabel->setText("<span style='color: #bdc3c7;'>-- °C</span>");
        ui->humidityValueLabel->setText("<span style='color: #bdc3c7;'>-- %</span>");
        ui->deviceIdLabel->setText("--");
        ui->updateTimeLabel->setText("--");

        // 重置进度条
        ui->tempProgressBar->setValue(0);
        ui->humidityProgressBar->setValue(0);

        // 设置状态文本
        ui->statusLabel->setText("✗ 等待数据...");
        ui->statusLabel->setStyleSheet(
            "QLabel {"
            "   background-color: #fff3e0;"
            "   border: 2px solid #ff9800;"
            "   border-radius: 10px;"
            "   padding: 5px 15px;"
            "   font-weight: bold;"
            "   color: #ef6c00;"
            "}"
            );
    }

    // 发射信号
    emit dataUpdated(data);
}

//删除数据
void SensorWindow::clearData()
{
    clearChart();
    SensorData emptyData;
    emptyData.isValid = false;
    updateData(emptyData);
}

void SensorWindow::clearChart()
{
    if (m_tempSeries) m_tempSeries->clear();
    if (m_humSeries) m_humSeries->clear();
}

//更新状态条
void SensorWindow::updateProgressBars()
{
    if (currentData.isValid) {
        // 温度进度条 (0°C 到 50°C 映射到 0-100)
        int tempProgress = static_cast<int>(currentData.temperature * 100 / 50);
        if (tempProgress < 0) tempProgress = 0;
        if (tempProgress > 100) tempProgress = 100;
        ui->tempProgressBar->setValue(tempProgress);

        // 根据当前温度动态设置样式
        updateTemperatureStyle(currentData.temperature);

        // 湿度进度条
        int humProgress = static_cast<int>(currentData.humidity);
        if (humProgress < 0) humProgress = 0;
        if (humProgress > 100) humProgress = 100;
        ui->humidityProgressBar->setValue(humProgress);

        // 根据当前湿度动态设置样式
        updateHumidityStyle(currentData.humidity);
    }
}

// 根据温度值动态更新样式
void SensorWindow::updateTemperatureStyle(float temperature)
{
    // 根据温度值计算渐变色的结束位置
    QString gradientStops;

    if (temperature <= 10) {
        // 0-10°C: 深蓝到蓝色
        gradientStops = "stop:0 #1565C0, stop:1 #2196F3";
    } else if (temperature <= 20) {
        // 10-20°C: 蓝色到绿色
        gradientStops = "stop:0 #1565C0, stop:0.5 #2196F3, stop:1 #4CAF50";
    } else if (temperature <= 30) {
        // 20-30°C: 绿色到黄色
        gradientStops = "stop:0 #1565C0, stop:0.3 #2196F3, stop:0.6 #4CAF50, stop:1 #FFC107";
    } else if (temperature <= 40) {
        // 30-40°C: 黄色到橙色
        gradientStops = "stop:0 #1565C0, stop:0.25 #2196F3, stop:0.5 #4CAF50, stop:0.75 #FFC107, stop:1 #FF9800";
    } else {
        // 40-50°C: 橙色到红色
        gradientStops = "stop:0 #1565C0, stop:0.2 #2196F3, stop:0.4 #4CAF50, stop:0.6 #FFC107, stop:0.8 #FF9800, stop:1 #F44336";
    }

    QString style = QString(R"(
        QProgressBar {
            border: 2px solid #d1d9e6;
            border-radius: 5px;
            text-align: center;
            background-color: #f5f7fa;
        }
        QProgressBar::chunk {
            border-radius: 3px;
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                %1);
        }
    )").arg(gradientStops);

    ui->tempProgressBar->setStyleSheet(style);
}

// 根据湿度值动态更新样式
void SensorWindow::updateHumidityStyle(float humidity)
{
    QString gradientStops;

    if (humidity <= 30) {
        // 干燥
        gradientStops = "stop:0 #FF6F00, stop:1 #FF9800";
    } else if (humidity <= 60) {
        // 舒适
        gradientStops = "stop:0 #FF6F00, stop:0.3 #FF9800, stop:0.6 #4CAF50, stop:1 #8BC34A";
    } else if (humidity <= 80) {
        // 湿润
        gradientStops = "stop:0 #FF6F00, stop:0.2 #FF9800, stop:0.4 #4CAF50, stop:0.6 #8BC34A, stop:0.8 #2196F3, stop:1 #03A9F4";
    } else {
        // 过湿
        gradientStops = "stop:0 #FF6F00, stop:0.15 #FF9800, stop:0.3 #4CAF50, stop:0.45 #8BC34A, stop:0.7 #2196F3, stop:0.85 #9C27B0, stop:1 #673AB7";
    }

    QString style = QString(R"(
        QProgressBar {
            border: 2px solid #d1d9e6;
            border-radius: 5px;
            text-align: center;
            background-color: #f5f7fa;
        }
        QProgressBar::chunk {
            border-radius: 3px;
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                %1);
        }
    )").arg(gradientStops);

    ui->humidityProgressBar->setStyleSheet(style);
}

//环境评估状态加emoj
void SensorWindow::updateStatusIndicator()
{
    if (!currentData.isValid) return;

    // 根据温度和湿度评估环境状态
    QString status;
    QString statusColor;

    // 环境舒适度判断
    if (currentData.temperature >= 22 && currentData.temperature <= 28 &&
        currentData.humidity >= 40 && currentData.humidity <= 70) {
        // 舒适范围：22-28°C，40-70% 湿度
        if (currentData.temperature >= 24 && currentData.temperature <= 26 &&
            currentData.humidity >= 45 && currentData.humidity <= 55) {
            status = "环境极佳 ✨";
            statusColor = "#4CAF50";  // 绿色
        } else {
            status = "环境舒适 😊";
            statusColor = "#8BC34A";  // 浅绿色
        }
    } else if (currentData.temperature < 0) {
        status = "严寒 ❄️";
        statusColor = "#2196F3";  // 蓝色
    } else if (currentData.temperature < 10) {
        status = "寒冷 🥶";
        statusColor = "#2196F3";  // 蓝色
    } else if (currentData.temperature > 35) {
        status = "酷热 🔥";
        statusColor = "#FF5722";  // 深橙色
    } else if (currentData.temperature > 30) {
        status = "炎热 ☀️";
        statusColor = "#FF9800";  // 橙色
    } else if (currentData.humidity < 20) {
        status = "极度干燥 🏜️";
        statusColor = "#FF9800";  // 橙色
    } else if (currentData.humidity < 30) {
        if (currentData.temperature > 25) {
            status = "干热 🍃";
            statusColor = "#FF9800";  // 橙色
        } else {
            status = "干燥 🌵";
            statusColor = "#FFC107";  // 黄色
        }
    } else if (currentData.humidity > 85) {
        status = "非常潮湿 🌧️";
        statusColor = "#2196F3";  // 蓝色
    } else if (currentData.humidity > 75) {
        if (currentData.temperature > 25) {
            status = "闷热 🥵";
            statusColor = "#F44336";  // 红色
        } else {
            status = "潮湿 💦";
            statusColor = "#03A9F4";  // 浅蓝色
        }
    } else if (currentData.temperature < 15) {
        if (currentData.humidity > 60) {
            status = "湿冷 🌧️";
            statusColor = "#2196F3";  // 蓝色
        } else {
            status = "凉爽 🍂";
            statusColor = "#03A9F4";  // 浅蓝色
        }
    } else if (currentData.temperature > 25) {
        if (currentData.humidity > 60) {
            status = "湿热 🌡️";
            statusColor = "#FF5722";  // 深橙色
        } else {
            status = "温暖 ☀️";
            statusColor = "#FF9800";  // 橙色
        }
    } else if (currentData.temperature < 18) {
        status = "微凉 🍃";
        statusColor = "#03A9F4";  // 浅蓝色
    } else if (currentData.temperature > 22) {
        status = "温暖 🌤️";
        statusColor = "#FFC107";  // 黄色
    } else {
        status = "环境正常";
        statusColor = "#9E9E9E";  // 灰色
    }
    ui->stateLabel->setText(
        QString("<span style='color: %1;'>%2</span>")
            .arg(statusColor)
            .arg(status)
        );
}

void SensorWindow::closeEvent(QCloseEvent *event)
{
    emit windowClosed();
    QWidget::closeEvent(event);
}

void SensorWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}
