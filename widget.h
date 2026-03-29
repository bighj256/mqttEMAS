#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QMqttClient>
#include <QPointer>
#include <QStringList>
#include <QSettings>
#include <QTimer>

namespace Ui {
class Widget;
}

class DataParser;
class SensorWindow;
class HistoryWindow;
class LogWindow;
//避免在头文件中包含这些类的完整定义，减少编译依赖、加快编译速度。

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    // 新增：全局历史记录（字符串格式，与 HistoryWindow 兼容）
    static QStringList globalHistory;
    ~Widget();

signals:
    // 信号：当有新历史项添加时触发
    void historyItemAdded(const QString &item);

private slots:
    //启动/断开 MQTT 连接
    void on_connectButton_clicked();

    //解析原始数据并更新界面
    //void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);

    //更新连接状态显示（如按钮文本、颜色）
    void updateConnectionState();

    //点击实时数据，显示sensorwindow窗口
    void on_dataWindowButton_clicked();

    //点击历史数据，显示historywindow窗口
    void on_historyWindowButton_clicked();

    //点击系统日志，显示logwindow窗口
    void on_logWindowButton_clicked();

    //设置窗口
    void on_settingsButton_clicked();

    //MQTT客户端报错，记录错误日志
    void onMqttError(QMqttClient::ClientError error);

    //解析原始数据并更新界面
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);

    //自动断线重连槽函数
    void tryReconnect();
private:
    Ui::Widget *ui;            //UI对象
    QMqttClient *mqttClient;  //MQTT客户端
    DataParser *dataParser;   //数据解析器

    QPointer<SensorWindow> sensorWindow;
    QPointer<HistoryWindow> historyWindow;
    QPointer<LogWindow> logWindow;

    QTimer *reconnectTimer;   // 定时器用于断线重连
    bool m_manualDisconnect;  // 标记是否为用户手动断开，手动断开不重连

    int dataCount = 0;
    QString lastUpdateTime;
    bool connectState = false;

    //在主窗口概览数据信息
    void updateMainWindowData(const QString &temp, const QString &humidity,
                              const QString &time, const QString &deviceId);

    // 添加日志记录
    void addLogMessage(const QString &message, const QString &type = "info");
};

#endif // WIDGET_H
