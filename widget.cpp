#include "widget.h"
#include "dataparser.h"
#include "dbmanager.h"
#include "historywindow.h"
#include "logwindow.h"
#include "sensorwindow.h"
#include "ui_widget.h"


#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QTime>
#include <stdlib.h>


// 最大历史记录条数
static const int Max_size = 500;

// 用于即使未打开历史数据或者关闭历史数据窗口也可以保存历史
QStringList Widget::globalHistory;

Widget::Widget(QWidget *parent)
    : QWidget(parent), ui(new Ui::Widget), mqttClient(new QMqttClient(this)),
      dataParser(new DataParser(this)) {
  ui->setupUi(this);

  // 初始化设备列表
  m_currentSelectedDevice = "所有设备";
  ui->deviceListWidget->addItem("所有设备");
  ui->deviceListWidget->setCurrentRow(0);
  connect(ui->deviceListWidget, &QListWidget::currentTextChanged, this, &Widget::onDeviceSelectionChanged);

  // 设置窗口
  setWindowTitle("MQTT环境监测系统 v1.3.0");
  setMinimumSize(900, 700);

  // 连接MQTT信号
  connect(mqttClient, &QMqttClient::stateChanged, this,
          &Widget::updateConnectionState);
  connect(mqttClient, &QMqttClient::messageReceived, this,
          &Widget::onMessageReceived);
  connect(mqttClient, &QMqttClient::errorChanged, this, &Widget::onMqttError);

  // 加载配置
  QSettings settings("MyCompany", "mqttEMAS2");
  ui->serverEdit->setText(
      settings.value("MQTT/server", "82.157.129.239").toString());
  ui->portEdit->setText(settings.value("MQTT/port", "1883").toString());
  ui->topicEdit->setText(
      settings.value("MQTT/topic", "sensor/data").toString());

  // 初始化数据
  ui->lastUpdateTimeLabel->setText("--");
  ui->currentTempLabel->setText("-- °C");
  ui->currentHumidityLabel->setText("-- %");
  ui->dataCountLabel->setText("0");

  // 初始化定时器与自动重连状态
  m_manualDisconnect = true;
  reconnectTimer = new QTimer(this);
  connect(reconnectTimer, &QTimer::timeout, this, &Widget::tryReconnect);

  // 创建日志窗口（但不显示）
  logWindow = new LogWindow(0);
  logWindow->hide();
  addLogMessage("系统启动完成", "info");
  addLogMessage("MQTT环境监测系统已就绪", "success");

  // 连接远程 SQL Server 82.157.129.239:1433
  if (DbManager::instance().connectSqlServer("82.157.129.239", 1433, "mqttEMAS",
                                             "sa", "QWEasdZXC123!")) {
    addLogMessage("已连接到远程 SQL Server 数据持久化库 (82.157.129.239)",
                  "success");

    // 导入最新的 500 条记录
    QList<SensorData> pastData = DbManager::instance().getLatestSensorData(500);
    if (!pastData.isEmpty()) {
      Widget::globalHistory.clear();
      for (const SensorData &data : pastData) {
        QString historyItem = QString("%1 | 温度: %2°C | 湿度: %3%")
                                  .arg(data.timestamp)
                                  .arg(data.temperature, 0, 'f', 1)
                                  .arg(data.humidity, 0, 'f', 1);
        
        QString devId = data.deviceId.isEmpty() ? "未知" : data.deviceId;
        if (!data.deviceId.isEmpty()) {
          historyItem += QString(" | 设备: %1").arg(data.deviceId);
        }
        Widget::globalHistory.append(historyItem);
        
        if (!m_knownDevices.contains(devId)) {
            m_knownDevices.insert(devId);
            ui->deviceListWidget->addItem(devId);
            m_deviceLatestData[devId] = data; 
        }
      }
      addLogMessage(QString("已成功从数据库导入最近 %1 条历史记录").arg(pastData.size()), "success");
    }
  } else {
    addLogMessage("连接远程 SQL Server 失败，将仅使用内存记录历史", "warning");
  }
}

Widget::~Widget() {
  if (mqttClient->state() == QMqttClient::Connected) {
    mqttClient->disconnectFromHost(); // 安全断开
  }
  delete ui;
}

// 添加统一的日志记录函数
void Widget::addLogMessage(const QString &message, const QString &type) {
  // 确保日志窗口存在
  if (!logWindow) {
    logWindow = new LogWindow(0);
  }
  logWindow->addLog(message, type);
}

// 连接/断开按钮点击
void Widget::on_connectButton_clicked() {
  if (mqttClient->state() == QMqttClient::Disconnected && !reconnectTimer->isActive()) {
    QString hostname = ui->serverEdit->text().trimmed();
    QString port = ui->portEdit->text().trimmed();
    QString topic = ui->topicEdit->text().trimmed();

    if (hostname.isEmpty()) {
      QMessageBox::warning(this, "警告", "请输入服务器地址");
      return;
    }

    if (port.isEmpty()) {
      QMessageBox::warning(this, "警告", "请输入端口号");
      return;
    }

    if (topic.isEmpty()) {
      QMessageBox::warning(this, "警告", "请输入订阅主题");
      return;
    }

    mqttClient->setHostname(hostname);
    mqttClient->setPort(port.toInt());
    mqttClient->setClientId(QString("QtClient_%1").arg(rand() % 1000));
    mqttClient->setCleanSession(true);

    m_manualDisconnect = false;
    mqttClient->connectToHost();

    // 记录连接日志
    addLogMessage(QString("正在连接到 %1:%2...").arg(hostname).arg(port),
                  "info");

  } else {
    m_manualDisconnect = true;
    if (reconnectTimer->isActive()) {
      reconnectTimer->stop();
      addLogMessage("已取消自动重连", "info");
      ui->statusLabel->setText("未连接");
      ui->statusLabel->setStyleSheet(
          "QLabel { color: #e74c3c; font-weight: bold; }");
    }

    if (mqttClient->state() != QMqttClient::Disconnected) {
      addLogMessage("正在断开连接...", "info");
      mqttClient->disconnectFromHost();
    }
    
    ui->connectButton->setText("连接");
  }
}

/*MQTT信息处理
 *接收原始数据->转为字符串并记录日志
 *解析为结构化数据->构造新历史数据项
 *发射historyItemAdded信号
 */
void Widget::onMessageReceived(const QByteArray &message,
                               const QMqttTopicName &topic) {
  QString msg = QString::fromUtf8(message);
  addLogMessage(QString("收到消息 [%1]: %2").arg(topic.name()).arg(msg),
                "data");

  SensorData data = dataParser->parse(msg);

  if (data.isValid) {
    QString currentTime =
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // 构造与 HistoryWindow 兼容的历史项字符串
    QString historyItem = QString("%1 | 温度: %2°C | 湿度: %3%")
                              .arg(currentTime)
                              .arg(data.temperature, 0, 'f', 1)
                              .arg(data.humidity, 0, 'f', 1);
    if (!data.deviceId.isEmpty()) {
      historyItem += QString(" | 设备: %1").arg(data.deviceId);
    }

    // 保存到全局历史（无论窗口是否打开）
    Widget::globalHistory.prepend(historyItem); // 最新在前

    // 限制最多 Max_size 条
    if (Widget::globalHistory.size() > Max_size) {
      Widget::globalHistory.removeLast();
    }

    // 发射信号，通知监听者有新数据
    emit historyItemAdded(historyItem);

    // 异步存入远端 SQL Server 数据库
    DbManager::instance().insertSensorData(data);

    dataCount++;
    ui->dataCountLabel->setText(QString::number(dataCount));

    // 更新设备列表
    QString devId = data.deviceId.isEmpty() ? "未知" : data.deviceId;
    if (!m_knownDevices.contains(devId)) {
        m_knownDevices.insert(devId);
        ui->deviceListWidget->addItem(devId);
    }
    m_deviceLatestData[devId] = data;

    // 若当前为"所有设备" 或正是收到了选中设备的数据，更新面板
    if (m_currentSelectedDevice == "所有设备" || m_currentSelectedDevice == devId) {
        updateMainWindowData(QString::number(data.temperature, 'f', 1),
                             QString::number(data.humidity, 'f', 1), currentTime,
                             data.deviceId);
        lastUpdateTime = currentTime;
        
        // 同步子窗口
        if (sensorWindow) {
          sensorWindow->updateData(data);
        }
    }

    addLogMessage(QString("数据解析成功 - 温度: %1°C, 湿度: %2%")
                      .arg(data.temperature, 0, 'f', 1)
                      .arg(data.humidity, 0, 'f', 1),
                  "success");
  } else {
    addLogMessage("数据解析失败或数据无效", "error");
  }
}

// MQTT连接状态更新
void Widget::updateConnectionState() {
  QString hostname = ui->serverEdit->text().trimmed();
  qDebug() << "当前MQTT状态改变为:" << mqttClient->state();
  switch (mqttClient->state()) {
  case QMqttClient::Disconnected:
    qDebug() << "MQTT状态: 未连接";
    
    if (logWindow) {
      if (connectState) {
        addLogMessage(QString("已与服务器断开连接: %1").arg(hostname),
                      "system");
      }
    }

    if (!m_manualDisconnect) {
      if (!reconnectTimer->isActive()) {
        reconnectTimer->start(5000);
        addLogMessage("检测到意外断开，将在5秒后尝试自动重连...", "warning");
        ui->statusLabel->setText("等待重连...");
        ui->statusLabel->setStyleSheet("QLabel { color: #e67e22; font-weight: bold; }");
        ui->connectButton->setText("停止重连");
      }
    } else {
      ui->statusLabel->setText("未连接");
      ui->statusLabel->setStyleSheet(
          "QLabel { color: #e74c3c; font-weight: bold; }");
      ui->connectButton->setText("连接");
    }
    ui->connectButton->setEnabled(true);
    break;

  case QMqttClient::Connecting:
    connectState = false;
    qDebug() << "MQTT状态: 连接中...";
    ui->statusLabel->setText("连接中...");
    ui->statusLabel->setStyleSheet(
        "QLabel { color: #f39c12; font-weight: bold; }");
    ui->connectButton->setText("取消连接");
    ui->connectButton->setEnabled(true);
    break;

  case QMqttClient::Connected:
    connectState = true;
    m_manualDisconnect = false;
    if (reconnectTimer->isActive()) {
      reconnectTimer->stop();
    }
    qDebug() << "MQTT状态: 已连接";
    ui->statusLabel->setText("已连接");
    ui->statusLabel->setStyleSheet(
        "QLabel { color: #27ae60; font-weight: bold; }");
    ui->connectButton->setText("断开连接");
    ui->connectButton->setEnabled(true);
    QString topic = ui->topicEdit->text().trimmed();
    addLogMessage(QString("已连接到服务器: %1").arg(hostname), "success");

    // 保存成功的配置
    {
      QSettings settings("MyCompany", "mqttEMAS2");
      settings.setValue("MQTT/server", hostname);
      settings.setValue("MQTT/port", ui->portEdit->text().trimmed());
      settings.setValue("MQTT/topic", topic);
    }

    if (!topic.isEmpty()) {
      mqttClient->subscribe(topic);
      addLogMessage(QString("已订阅主题: %1").arg(topic), "success");
    }
    break;
  }
}

// 实时数据窗口
void Widget::on_dataWindowButton_clicked() {
  if (!sensorWindow) {
    sensorWindow = new SensorWindow(0);
    sensorWindow->setAttribute(Qt::WA_DeleteOnClose);
    connect(sensorWindow, &QObject::destroyed,
            [this]() { sensorWindow.clear(); });
  }
  sensorWindow->show();
  sensorWindow->raise();
  sensorWindow->activateWindow();
}

// 历史数据窗口
void Widget::on_historyWindowButton_clicked() {
  if (!historyWindow) {
    historyWindow = new HistoryWindow(this, 0);
    historyWindow->setAttribute(Qt::WA_DeleteOnClose);
    connect(historyWindow, &QObject::destroyed,
            [this]() { historyWindow.clear(); });
  }
  // 在显示之前，强制同步一下当前的主界面设备选择器的过滤条件
  historyWindow->setDeviceFilter(m_currentSelectedDevice);
  
  historyWindow->show();
  historyWindow->raise();
  historyWindow->activateWindow();
}

// 日志窗口
void Widget::on_logWindowButton_clicked() {
  // 确保日志窗口存在
  if (!logWindow) {
    logWindow = new LogWindow(this);
  }

  logWindow->show();
  logWindow->raise();
  logWindow->activateWindow();
}

// 设置窗口
void Widget::on_settingsButton_clicked() {

  QMessageBox::information(this, "系统设置", "系统设置功能正在开发中...");
}

// 更新主界面数据显示
void Widget::updateMainWindowData(const QString &temp, const QString &humidity,
                                  const QString &time,
                                  const QString &deviceId) {
  Q_UNUSED(deviceId);
  ui->lastUpdateTimeLabel->setText(time);
  ui->currentTempLabel->setText(temp + " °C");
  ui->currentHumidityLabel->setText(humidity + " %");
}

// MQTT错误处理
void Widget::onMqttError(QMqttClient::ClientError error) {
  if (error == QMqttClient::UnknownError) {
    qDebug() << "MQTT未知错误，但可能只是临时状态";
    if (logWindow) {
      logWindow->addLog("MQTT连接过程中出现未知错误", "warning");
    }
    return;
  }

  // 只对致命错误弹出警告框
  QString errorMsg;
  bool shouldShowDialog = true;

  switch (error) {
  case QMqttClient::InvalidProtocolVersion:
    errorMsg = "无效的协议版本";
    break;
  case QMqttClient::IdRejected:
    errorMsg = "客户端ID被拒绝";
    break;
  case QMqttClient::ServerUnavailable:
    errorMsg = "服务器不可用";
    break;
  case QMqttClient::BadUsernameOrPassword:
    errorMsg = "错误的用户名或密码";
    break;
  case QMqttClient::NotAuthorized:
    errorMsg = "未授权访问";
    break;
  case QMqttClient::TransportInvalid:
    errorMsg = "传输无效 - 请检查网络连接";
    break;
  case QMqttClient::ProtocolViolation:
    errorMsg = "协议违规";
    break;
  case QMqttClient::UnknownError:
    // 已经处理过，不会走到这里
    shouldShowDialog = false;
    break;
  default:
    errorMsg = "未知错误";
  }

  qDebug() << "MQTT错误:" << errorMsg;

  if (logWindow) {
    logWindow->addLog("连接错误: " + errorMsg, "error");
  }

  // 只在需要时才弹出警告框
  if (shouldShowDialog) {
    QMessageBox::warning(this, "连接错误", errorMsg);
  }

  // 只在确实断开时才重置按钮状态
  if (mqttClient->state() == QMqttClient::Disconnected && m_manualDisconnect) {
    ui->connectButton->setEnabled(true);
    ui->connectButton->setText("连接");
  }
}

void Widget::tryReconnect() {
  if (mqttClient->state() == QMqttClient::Disconnected) {
    addLogMessage("正在尝试自动重连...", "warning");
    QString hostname = ui->serverEdit->text().trimmed();
    int port = ui->portEdit->text().toInt();
    mqttClient->setHostname(hostname);
    mqttClient->setPort(port);
    // 使用新的 ClientId 避免冲突
    mqttClient->setClientId(QString("QtClient_%1_RC").arg(rand() % 1000));
    mqttClient->connectToHost();
  }
}

void Widget::onDeviceSelectionChanged(const QString &device) {
  m_currentSelectedDevice = device;
  if (device == "所有设备") {
      ui->currentTempLabel->setText("-- °C");
      ui->currentHumidityLabel->setText("-- %");
      ui->lastUpdateTimeLabel->setText("--");
      if (sensorWindow) sensorWindow->clearData();
  } else {
      if (m_deviceLatestData.contains(device)) {
          SensorData data = m_deviceLatestData[device];
          updateMainWindowData(QString::number(data.temperature, 'f', 1),
                               QString::number(data.humidity, 'f', 1),
                               data.timestamp,
                               data.deviceId);
          if (sensorWindow) {
              sensorWindow->clearData();
              sensorWindow->updateData(data);
          }
      } else {
          updateMainWindowData("--", "--", "--", device);
          if (sensorWindow) sensorWindow->clearData();
      }
  }
  
  if (historyWindow) {
      historyWindow->setDeviceFilter(device);
  }
}
