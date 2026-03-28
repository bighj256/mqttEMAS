#ifndef HISTORYWINDOW_H
#define HISTORYWINDOW_H

#include <QWidget>
#include <QListWidgetItem>
#include "widget.h"

namespace Ui {
class HistoryWindow;
}
//前向声明自动生成的UI类，可减少编译依赖，加快编译速度

class HistoryWindow : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWindow(Widget *mainWidget, QWidget *parent = nullptr);  // explicit防止隐式类型转换
    ~HistoryWindow();

    void setDeviceFilter(const QString &device);

private slots:
    void onExportClicked();                                 // 导出按钮
    void onClearClicked();                                  // 清空按钮
    void onItemClicked(QListWidgetItem *item);              // 列表项点击
    void onHistoryItemAdded(const QString &item);           // 响应新历史项

private:
    void setupUI();                                         // UI设置
    void connectSignals();                                  // 信号连接
    void updateStatistics();                                // 计算统计信息，温度湿度平均值，最大值
    void loadGlobalHistory();                               // 加载全局数据
    void parseHistoryItem(const QString &item, double &temp, double &humidity);  // 解析数据
    void updateDisplay();                                   // 更新统计标签显示
    bool exportHistory(const QString &filename);            // 导出历史记录

    struct Statistics {
        int totalCount = 0;            // 总数据条数
        double avgTemperature = 0.0;   // 平均温度
        double maxTemperature = -999.0; // 最高温度
        double avgHumidity = 0.0;      // 平均湿度
        double maxHumidity = -999.0;   // 最高湿度
    };

    Ui::HistoryWindow *ui;             // 指向自定生成UI控件的组件
    Statistics currentStats;           // 缓存当前统计结果
    QString m_currentFilter = "所有设备"; // 当前要过滤显示的设备ID
};

#endif // HISTORYWINDOW_H
