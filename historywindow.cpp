#include "historywindow.h"
#include "ui_historywindow.h"
#include "widget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

HistoryWindow::HistoryWindow(Widget *mainWidget,QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HistoryWindow)
{
    ui->setupUi(this);

    // 设置窗口属性
    setWindowTitle("历史数据记录");
    setMinimumSize(600, 400);

    // 设置窗口标志
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);

    // 初始化UI和信号
    setupUI();
    connectSignals();

    if (mainWidget) {
        connect(mainWidget, &Widget::historyItemAdded,
                this, &HistoryWindow::onHistoryItemAdded);
    }

    // 加载全局历史数据
    loadGlobalHistory();
}

HistoryWindow::~HistoryWindow()
{
    delete ui;
}

void HistoryWindow::setupUI()
{
    // 设置列表样式
    ui->historyList->setStyleSheet(R"(
        QListWidget {
            border: 2px solid #d1d9e6;
            border-radius: 8px;
            background-color: white;
            font-size: 11pt;
            padding: 5px;
        }
        QListWidget::item {
            padding: 12px;
            border-bottom: 1px solid #f0f0f0;
            background-color: white;
        }
        QListWidget::item:hover {
            background-color: #f8fafd;
            border-left: 4px solid #4a90e2;
        }
        QListWidget::item:selected {
            background-color: #e3f2fd;
            color: #1565c0;
            border-left: 4px solid #1565c0;
        }
    )");


    // 设置字体
    QFont titleFont = ui->titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    ui->titleLabel->setFont(titleFont);

    // 初始化统计标签
    ui->avgTempLabel->setText("-- °C");
    ui->maxTempLabel->setText("-- °C");
    ui->avgHumidityLabel->setText("-- %");
    ui->maxHumidityLabel->setText("-- %");
    ui->countLabel->setText("0");
}

void HistoryWindow::connectSignals()
{
    // 连接按钮信号
    connect(ui->exportButton, &QPushButton::clicked, this, &HistoryWindow::onExportClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &HistoryWindow::onClearClicked);

    // 连接列表信号
    connect(ui->historyList, &QListWidget::itemClicked, this, &HistoryWindow::onItemClicked);

}

//从全局加载历史
void HistoryWindow::loadGlobalHistory()
{
    ui->historyList->clear();

    for (const QString& itemText : Widget::globalHistory) {
        QListWidgetItem* listItem = new QListWidgetItem(itemText);
        if (itemText.contains("温度:", Qt::CaseInsensitive)) {
            QRegularExpression tempRegex("温度:\\s*(\\d+\\.?\\d*)");
            QRegularExpressionMatch match = tempRegex.match(itemText);
            if (match.hasMatch()) {
                double temp = match.captured(1).toDouble();
                if (temp > 30) {
                    listItem->setForeground(QColor("#e74c3c"));
                } else if (temp < 10) {
                    listItem->setForeground(QColor("#3498db"));
                } else {
                    listItem->setForeground(QColor("#27ae60"));
                }
            }
        }

        ui->historyList->addItem(listItem);
    }

    updateStatistics(); // 重新计算统计
}

//更新数据统计
void HistoryWindow::updateStatistics()
{
    currentStats = Statistics();
    currentStats.totalCount = Widget::globalHistory.size();

    if (currentStats.totalCount == 0) {
        updateDisplay();
        return;
    }

    double tempSum = 0;
    double humiditySum = 0;

    for (const QString& itemText : Widget::globalHistory) {
        double temp = 0, humidity = 0;
        parseHistoryItem(itemText, temp, humidity);

        tempSum += temp;
        humiditySum += humidity;

        if (temp > currentStats.maxTemperature) {
            currentStats.maxTemperature = temp;
        }
        if (humidity > currentStats.maxHumidity) {
            currentStats.maxHumidity = humidity;
        }
    }

    currentStats.avgTemperature = tempSum / currentStats.totalCount;
    currentStats.avgHumidity = humiditySum / currentStats.totalCount;

    updateDisplay();
}

//解析数据
void HistoryWindow::parseHistoryItem(const QString &item, double &temp, double &humidity)
{
    QRegularExpression tempRegex(R"(温度:\s*([-+]?\d*\.?\d+))");
    QRegularExpression humidityRegex(R"(湿度:\s*(\d+\.?\d*))");

    QRegularExpressionMatch tempMatch = tempRegex.match(item);
    QRegularExpressionMatch humidityMatch = humidityRegex.match(item);

    if (tempMatch.hasMatch()) {
        temp = tempMatch.captured(1).toDouble();
    }
    if (humidityMatch.hasMatch()) {
        humidity = humidityMatch.captured(1).toDouble();
    }
}

//更新数据显示
void HistoryWindow::updateDisplay()
{
    ui->countLabel->setText(QString::number(currentStats.totalCount));

    if (currentStats.totalCount > 0) {
        ui->avgTempLabel->setText(QString("%1 °C").arg(currentStats.avgTemperature, 0, 'f', 1));
        ui->maxTempLabel->setText(QString("%1 °C").arg(currentStats.maxTemperature, 0, 'f', 1));
        ui->avgHumidityLabel->setText(QString("%1 %").arg(currentStats.avgHumidity, 0, 'f', 1));
        ui->maxHumidityLabel->setText(QString("%1 %").arg(currentStats.maxHumidity, 0, 'f', 1));
    } else {
        ui->avgTempLabel->setText("-- °C");
        ui->maxTempLabel->setText("-- °C");
        ui->avgHumidityLabel->setText("-- %");
        ui->maxHumidityLabel->setText("-- %");
    }
}

//导出历史数据
void HistoryWindow::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "导出历史数据",
                                                    QString("历史数据_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
                                                    "CSV文件 (*.csv);;文本文件 (*.txt)"
                                                    );

    if (fileName.isEmpty()) return;

    if (exportHistory(fileName)) {
        QMessageBox::information(this, "导出成功",
                                 QString("已成功导出 %1 条历史记录到文件。").arg(currentStats.totalCount));
    } else {
        QMessageBox::warning(this, "导出失败", "无法导出历史记录，请检查文件路径和权限。");
    }
}

bool HistoryWindow::exportHistory(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << "时间,温度(°C),湿度(%),设备ID\n";

    //  遍历全局历史（注意：globalHistory 是从新到旧）
    for (int i = Widget::globalHistory.size() - 1; i >= 0; --i) {
        QString itemText = Widget::globalHistory[i];

        QString time, temp, humidity, device;
        QStringList parts = itemText.split(" | ");
        for (const QString &part : parts) {
            if (part.contains("温度:")) {
                temp = part.mid(3).trimmed().replace("°C", "");
            } else if (part.contains("湿度:")) {
                humidity = part.mid(3).trimmed().replace("%", "");
            } else if (part.contains("设备:")) {
                device = part.mid(3).trimmed();
            } else if (part.contains("-")) {
                time = part.trimmed();
            }
        }

        stream << QString("%1,%2,%3,%4\n")
                      .arg(time.isEmpty() ? QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") : time)
                      .arg(temp.isEmpty() ? "0" : temp)
                      .arg(humidity.isEmpty() ? "0" : humidity)
                      .arg(device.isEmpty() ? "未知" : device);
    }

    file.close();
    return true;
}

//清空历史数据
void HistoryWindow::onClearClicked()
{
    if (Widget::globalHistory.isEmpty()) {
        QMessageBox::information(this, "提示", "历史记录列表已经是空的。");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              "确认清空",
                                                              QString("确定要清空所有历史记录吗？\n当前共有 %1 条记录。").arg(Widget::globalHistory.size()),
                                                              QMessageBox::Yes | QMessageBox::No
                                                              );

    if (reply == QMessageBox::Yes) {
        Widget::globalHistory.clear();
        loadGlobalHistory(); // 刷新 UI
        QMessageBox::information(this, "清空完成", "历史记录已清空。");
    }
}

//响应点击历史项
void HistoryWindow::onItemClicked(QListWidgetItem *item)
{
    QMessageBox::information(this, "记录详情", item->text());
}

//实时更新历史数据
void HistoryWindow::onHistoryItemAdded(const QString &item)
{
    // 插入到顶部（最新在前）
    QListWidgetItem* listItem = new QListWidgetItem(item);

    if (item.contains("温度:", Qt::CaseInsensitive)) {
        QRegularExpression tempRegex("温度:\\s*(\\d+\\.?\\d*)");
        QRegularExpressionMatch match = tempRegex.match(item);
        if (match.hasMatch()) {
            double temp = match.captured(1).toDouble();
            if (temp > 30) {
                listItem->setForeground(QColor("#e74c3c"));
            } else if (temp < 10) {
                listItem->setForeground(QColor("#3498db"));
            } else {
                listItem->setForeground(QColor("#27ae60"));
            }
        }
    }

    ui->historyList->insertItem(0, listItem); // 插入顶部

    //自动滚动到顶部
    ui->historyList->scrollToItem(ui->historyList->item(0));

    // 更新统计
    updateStatistics();
}

