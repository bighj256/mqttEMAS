#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QWidget>

//前向声明自动生成的UI类，可减少编译依赖，加快编译速度
namespace Ui {
class LogWindow;
}

class LogWindow : public QWidget
{
    Q_OBJECT

public:
    struct LogEntry {  // 日志结构（必须放在public部分才能在public方法中使用）
        QString time;
        QString message;
        QString type;
        QString color;
        QString serialize() const { return QString("%1|%2|%3|%4").arg(time, message, type, color); }  // 添加序列化支持
        static LogEntry deserialize(const QString &str) {  // 反序列化-->字符串恢复成结构体
            QStringList parts = str.split("|", Qt::KeepEmptyParts);
            if (parts.size() >= 4) return {parts[0], parts[1], parts[2], parts[3]};
            return {};
        }
    };

    explicit LogWindow(QWidget *parent = nullptr);
    ~LogWindow();
    void addLog(const QString &message, const QString &type = "info");  // 添加日志
    void clearLog();                             // 清空日志
    bool saveLog(const QString &filename);       // 保存日志到文件
    void setAutoScroll(bool enabled);            // 设置自动滚动
    void setShowTimestamp(bool enabled);         // 设置显示时间戳
    int getLogCount() const;                     // 获取日志数量
    QList<LogEntry> getAllLogs() const { return logEntries; }  // 获取所有日志条目
    void setLogs(const QList<LogEntry> &logs);   // 从外部设置日志（用于恢复数据）

public slots:
    void onClearClicked();           // 清空日志
    void onSaveClicked();            // 保存为纯文本
    void onExportClicked();          // 导出为HTML
    void onLogLevelChanged(int index);               // 响应下拉框选项变化
    void onSearchTextChanged(const QString &text);   // 过滤关键词实现搜索
    void onAutoScrollToggled(bool checked);          // 自动滚动
    void onTimestampToggled(bool checked);           // 显示时间戳

private:
    Ui::LogWindow *ui;
    QList<LogEntry> logEntries;      // 核心数据存储：所有日志条目的列表
    bool autoScrollEnabled = true;
    bool showTimestamp = true;

    void setupUI();
    void connectSignals();
    QString getLogTypeColor(const QString &type);
    void updateLogDisplay();
    void applyFilter();              // 应用当前的过滤规则（日志类别 + 搜索关键词），然后刷新显示
    void saveLogsToFile();           // 持久化到本地文件
    void loadLogsFromFile();         //日记持久化：加载日志
};
#endif // LOGWINDOW_H
