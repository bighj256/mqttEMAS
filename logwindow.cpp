#include "logwindow.h"
#include "ui_logwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QScrollBar>
#include <QTimer>
#include <QFile>
#include <QDir>

// 添加常量
const QString LOGS_FILE = "app_logs.dat";

LogWindow::LogWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LogWindow)
{
    ui->setupUi(this);

    // 设置窗口属性
    setWindowTitle("系统日志");
    setMinimumSize(600, 400);

    // 设置窗口标志
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);

    // 初始化UI和信号
    setupUI();
    connectSignals();

    // 加载持久化的日志
    loadLogsFromFile();

    // 更新显示
    updateLogDisplay();

    setShowTimestamp(true);
    setAutoFillBackground(true);
}

LogWindow::~LogWindow()
{
    // 保存日志到文件
    saveLogsToFile();
    delete ui;
}

//日记持久化：加载日志
void LogWindow::loadLogsFromFile()
{
    QFile file(LOGS_FILE);
    if (!file.exists()) {
        return;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine();
            LogEntry entry = LogEntry::deserialize(line);
            if (!entry.time.isEmpty()) {
                logEntries.append(entry);
            }
        }
        file.close();
        qDebug() << "从文件加载了" << logEntries.count() << "条日志";
    }
}

void LogWindow::saveLogsToFile()
{
    QFile file(LOGS_FILE);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        for (const LogEntry &entry : logEntries) {
            stream << entry.serialize() << "\n";
        }
        file.close();
        qDebug() << "保存了" << logEntries.count() << "条日志到文件";
    }
}

//外部批量设置日志
void LogWindow::setLogs(const QList<LogEntry> &logs)
{
    logEntries = logs;
    updateLogDisplay();
    ui->logCountLabel->setText(QString::number(logEntries.count()));
}

//删除日志
void LogWindow::clearLog()
{
    logEntries.clear();
    updateLogDisplay();
    ui->logCountLabel->setText("0");

    // 删除日志文件
    QFile::remove(LOGS_FILE);
}

//添加日志
void LogWindow::addLog(const QString &message, const QString &type)
{
    LogEntry entry;
    entry.time = QDateTime::currentDateTime().toString("HH:mm:ss");
    entry.message = message;
    entry.type = type;
    entry.color = getLogTypeColor(type);

    logEntries.append(entry);

    // 更新显示
    updateLogDisplay();

    // 更新日志计数
    ui->logCountLabel->setText(QString::number(logEntries.count()));

    // 自动滚动到底部
    if (autoScrollEnabled) {
        QTextCursor cursor = ui->logText->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->logText->setTextCursor(cursor);
        ui->logText->ensureCursorVisible();
    }
}

//保存日志
bool LogWindow::saveLog(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);

    // 写入日志头
    stream << "=== MQTT环境监测系统日志 ===\n";
    stream << "导出时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
    stream << "日志条数: " << logEntries.count() << "\n";
    stream << QString('=').repeated(40) << "\n\n";

    // 写入日志内容
    for (const LogEntry &entry : logEntries) {
        stream << QString("[%1] %2\n").arg(entry.time).arg(entry.message);
    }

    file.close();
    return true;
}

//清空日志
void LogWindow::onClearClicked()
{
    if (logEntries.isEmpty()) {
        QMessageBox::information(this, "提示", "日志列表已经是空的。");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              "确认清空",
                                                              QString("确定要清空所有日志吗？\n当前共有 %1 条日志记录。\n注意：这将删除所有日志信息！").arg(logEntries.count()),
                                                              QMessageBox::Yes | QMessageBox::No
                                                              );

    if (reply == QMessageBox::Yes) {

        clearLog();
        addLog("日志已清空", "info");
    }
}

void LogWindow::setupUI()
{
    // 设置日志文本框样式
    ui->logText->setStyleSheet(R"(
        QTextEdit {
            border: 2px solid #d1d9e6;
            border-radius: 8px;
            background-color: #1e1e1e;
            color: #d4d4d4;
            font-family: "Consolas", "Monospace";
            font-size: 10pt;
            padding: 10px;
            selection-background-color: #264f78;
        }
    )");

    // 设置字体
    QFont titleFont = ui->titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    ui->titleLabel->setFont(titleFont);

    // 设置日志级别下拉框
    ui->logLevelCombo->setStyleSheet(R"(
        QComboBox {
            border: 2px solid #d1d9e6;
            border-radius: 6px;
            padding: 6px;
            background-color: white;
            min-width: 100px;
        }
        QComboBox:hover {
            border-color: #4a90e2;
        }
        QComboBox::drop-down {
            border: none;
        }
    )");

    // 初始化控件状态
    ui->autoScrollCheck->setChecked(autoScrollEnabled);
    ui->timestampCheck->setChecked(showTimestamp);
    ui->logCountLabel->setText("0");
}

void LogWindow::connectSignals()
{
    // 连接按钮信号
    connect(ui->clearButton, &QPushButton::clicked, this, &LogWindow::onClearClicked);
    connect(ui->saveButton, &QPushButton::clicked, this, &LogWindow::onSaveClicked);
    connect(ui->exportButton, &QPushButton::clicked, this, &LogWindow::onExportClicked);

    // 连接下拉框信号
    connect(ui->logLevelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogWindow::onLogLevelChanged);

    // 连接搜索框信号
    connect(ui->searchEdit, &QLineEdit::textChanged,
            this, &LogWindow::onSearchTextChanged);

    // 连接复选框信号
    connect(ui->autoScrollCheck, &QCheckBox::toggled,
            this, &LogWindow::onAutoScrollToggled);
    connect(ui->timestampCheck, &QCheckBox::toggled,
            this, &LogWindow::onTimestampToggled);
}

//设置日志信息颜色
QString LogWindow::getLogTypeColor(const QString &type)
{
    if (type == "error") {
        return "#e74c3c";      // 红色
    } else if (type == "warning") {
        return "#f39c12";      // 黄色
    } else if (type == "success") {
        return "#27ae60";      // 绿色
    } else if (type == "data") {
        return "#3498db";      // 蓝色
    } else if (type == "system") {
        return "#9b59b6";      // 紫色
    } else {
        return "#7f8c8d";      // 灰色 (info)
    }
}

void LogWindow::updateLogDisplay()
{
    QString html = "<pre style='margin: 0; padding: 0; font-family: Consolas, Monospace; font-size: 10pt;'>";

    for (const LogEntry &entry : logEntries) {
        // 日志级别过滤
        QString currentLevel = ui->logLevelCombo->currentText();
        if (currentLevel != "全部") {
            if (currentLevel == "信息" && entry.type != "info") continue;
            if (currentLevel == "警告" && entry.type != "warning") continue;
            if (currentLevel == "错误" && entry.type != "error") continue;
            if (currentLevel == "成功" && entry.type != "success") continue;
        }
        // 搜索过滤
        QString searchText = ui->searchEdit->text();
        if (!searchText.isEmpty() && !entry.message.contains(searchText, Qt::CaseInsensitive)) {
            continue;
        }

        // 构建日志行
        QString line;
        if (showTimestamp) {
            line = QString("[<span style='color: #95a5a6;'>%1</span>] ").arg(entry.time);
        }
        line += QString("<span style='color: %1;'>%2</span><br>")
                    .arg(entry.color)
                    .arg(entry.message.toHtmlEscaped());

        html += line;
    }

    html += "</pre>";

    // 设置 HTML（会重置滚动位置）
    ui->logText->setHtml(html);
}

int LogWindow::getLogCount() const
{
    return logEntries.count();
}

void LogWindow::applyFilter()
{
    updateLogDisplay();
}

//保存日志.txt形式
void LogWindow::onSaveClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "保存日志",
                                                    QString("系统日志_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
                                                    "文本文件 (*.txt);;所有文件 (*.*)"
                                                    );

    if (fileName.isEmpty()) return;

    if (saveLog(fileName)) {
        addLog(QString("日志已保存到: %1").arg(fileName), "success");
        QMessageBox::information(this, "保存成功", "日志文件已保存。");
    } else {
        addLog("日志保存失败", "error");
        QMessageBox::warning(this, "保存失败", "无法保存日志文件，请检查路径和权限。");
    }
}

//导出日志.html形式
void LogWindow::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "导出日志",
                                                    QString("日志导出_%1.html").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
                                                    "HTML文件 (*.html);;所有文件 (*.*)"
                                                    );

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法创建文件，请检查路径和权限。");
        return;
    }

    QTextStream stream(&file);

    // 构建HTML文档
    stream << "<!DOCTYPE html>\n";
    stream << "<html>\n";
    stream << "<head>\n";
    stream << "  <meta charset=\"UTF-8\">\n";
    stream << "  <title>系统日志导出</title>\n";
    stream << "  <style>\n";
    stream << "    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }\n";
    stream << "    .container { background-color: white; border-radius: 8px; padding: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n";
    stream << "    .header { border-bottom: 2px solid #4a90e2; padding-bottom: 10px; margin-bottom: 20px; }\n";
    stream << "    .log-entry { padding: 5px 0; border-bottom: 1px solid #eee; }\n";
    stream << "    .timestamp { color: #95a5a6; font-size: 0.9em; }\n";
    stream << "    .info { color: #7f8c8d; }\n";
    stream << "    .success { color: #27ae60; font-weight: bold; }\n";
    stream << "    .warning { color: #f39c12; font-weight: bold; }\n";
    stream << "    .error { color: #e74c3c; font-weight: bold; }\n";
    stream << "    .data { color: #3498db; }\n";
    stream << "  </style>\n";
    stream << "</head>\n";
    stream << "<body>\n";
    stream << "  <div class=\"container\">\n";
    stream << "    <div class=\"header\">\n";
    stream << "      <h1>MQTT环境监测系统日志</h1>\n";
    stream << "      <p>导出时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "</p>\n";
    stream << "      <p>日志条数: " << logEntries.count() << "</p>\n";
    stream << "    </div>\n";

    // 写入日志内容
    for (const LogEntry &entry : logEntries) {
        QString cssClass = entry.type;
        stream << "    <div class=\"log-entry\">\n";
        stream << "      <span class=\"timestamp\">[" << entry.time << "]</span>\n";
        stream << "      <span class=\"" << cssClass << "\">" << entry.message.toHtmlEscaped() << "</span>\n";
        stream << "    </div>\n";
    }

    stream << "  </div>\n";
    stream << "</body>\n";
    stream << "</html>\n";

    file.close();

    addLog(QString("日志已导出为HTML: %1").arg(fileName), "success");
    QMessageBox::information(this, "导出成功", "日志已成功导出为HTML文件。");
}

//响应日志类别下拉框的操作
void LogWindow::onLogLevelChanged(int index)
{
    Q_UNUSED(index);
    applyFilter();
}

//响应在搜索框输入和删除操作
void LogWindow::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    applyFilter();
}

void LogWindow::setAutoScroll(bool enabled)
{
    ui->autoScrollCheck->setChecked(enabled);
}

void LogWindow::setShowTimestamp(bool enabled)
{
    ui->timestampCheck->setChecked(enabled);
}

//自动滚动
void LogWindow::onAutoScrollToggled(bool checked)
{
    autoScrollEnabled = checked;

    if (checked) {
        QScrollBar *vbar = ui->logText->verticalScrollBar();
        int target = vbar->maximum();
        int current = vbar->value();

        if (current >= target) return; // 已在底部

        // 每次滚动的步长（可调）
        const int step = 5;
        // 滚动间隔（毫秒，越小越快）
        const int interval = 80;

        // 使用单次 QTimer 驱动滚动
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [=]() {
            int current = vbar->value();
            if (current < target) {
                vbar->setValue(qMin(current + step, target));
            } else {
                timer->stop();
                timer->deleteLater();
            }
        });
        timer->start(interval);
    }
}

//显示时间戳
void LogWindow::onTimestampToggled(bool checked)
{
    showTimestamp = checked;
    updateLogDisplay();
}
