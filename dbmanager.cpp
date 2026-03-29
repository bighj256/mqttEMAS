#include "dbmanager.h"
#include <QMetaObject>
#include <QSqlRecord>

DbWorker::DbWorker(QObject *parent) : QObject(parent)
{
}

DbWorker::~DbWorker()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

void DbWorker::connectSqlServer(const QString& host, int port, const QString& dbName, const QString& user, const QString& pwd)
{
    if (m_db.isOpen()) {
        emit connectFinished(true, "");
        return;
    }

    if(QSqlDatabase::contains("db_worker_conn")) {
        m_db = QSqlDatabase::database("db_worker_conn");
    } else {
        m_db = QSqlDatabase::addDatabase("QODBC", "db_worker_conn");
    }

    QString dsn = QString("DRIVER={SQL Server};SERVER=%1,%2;DATABASE=%3;")
                      .arg(host).arg(port).arg(dbName);
    
    m_db.setDatabaseName(dsn);
    m_db.setUserName(user);
    m_db.setPassword(pwd);

    if (!m_db.open()) {
        qWarning() << "数据库连接失败:" << m_db.lastError().text();
        emit connectFinished(false, m_db.lastError().text());
        return;
    }

    qDebug() << "成功连接到 SQL Server 数据库!";
    
    QSqlQuery query(m_db);
    QString createTableQuery = 
        "IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='SensorDataTable' AND xtype='U') "
        "CREATE TABLE SensorDataTable ("
        "  Id INT IDENTITY(1,1) PRIMARY KEY,"
        "  Temperature FLOAT,"
        "  Humidity FLOAT,"
        "  DeviceId NVARCHAR(50),"
        "  Timestamp DATETIME"
        ")";
    
    if (!query.exec(createTableQuery)) {
        qWarning() << "创建数据表失败:" << query.lastError().text();
    }

    // 创建基于 Timestamp 的降序索引，极大加速启动时的获取历史数据
    QString createIndexQuery = 
        "IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name='idx_str_timestamp_desc' AND object_id = OBJECT_ID('SensorDataTable')) "
        "CREATE INDEX idx_str_timestamp_desc ON SensorDataTable (Timestamp DESC)";
    if (!query.exec(createIndexQuery)) {
        qWarning() << "创建时间戳索引失败:" << query.lastError().text();
    }

    emit connectFinished(true, "");
}

void DbWorker::insertSensorData(const SensorData& data)
{
    if (!m_db.isOpen()) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO SensorDataTable (Temperature, Humidity, DeviceId, Timestamp) "
                  "VALUES (:temp, :hum, :device, :time)");
    query.bindValue(":temp", data.temperature);
    query.bindValue(":hum", data.humidity);
    query.bindValue(":device", data.deviceId.isEmpty() ? "Unknown" : data.deviceId);
    query.bindValue(":time", data.timestamp); 

    if (!query.exec()) {
        qWarning() << "插入传感器数据失败:" << query.lastError().text();
    }
}

void DbWorker::getLatestSensorData(int limit)
{
    QList<SensorData> results;
    if (!m_db.isOpen()) {
        qWarning() << "数据库未打开，无法获取数据";
        emit latestDataReady(results);
        return;
    }

    QSqlQuery query(m_db);
    // 开启 ForwardOnly 可以显著降低 ODBC 网络封包缓存的内存和延迟
    query.setForwardOnly(true); 
    QString sql = QString("SELECT TOP %1 DeviceId, Temperature, Humidity, Timestamp "
                          "FROM SensorDataTable ORDER BY Timestamp DESC").arg(limit);
    
    if (query.exec(sql)) {
        // 按索引号进行取值比按列名字符串匹配查找更快
        int idxDevice = query.record().indexOf("DeviceId");
        int idxTemp = query.record().indexOf("Temperature");
        int idxHum = query.record().indexOf("Humidity");
        int idxTime = query.record().indexOf("Timestamp");

        while (query.next()) {
            SensorData data;
            data.deviceId = query.value(idxDevice).toString();
            data.temperature = query.value(idxTemp).toDouble();
            data.humidity = query.value(idxHum).toDouble();
            data.timestamp = query.value(idxTime).toDateTime().toString("yyyy-MM-dd HH:mm:ss");
            data.isValid = true;
            results.append(data);
        }
    } else {
        qWarning() << "查询历史数据失败:" << query.lastError().text();
    }
    
    emit latestDataReady(results);
}


DbManager& DbManager::instance()
{
    static DbManager instance;
    return instance;
}

DbManager::DbManager()
    : m_worker(new DbWorker())
{
    qRegisterMetaType<SensorData>("SensorData");
    qRegisterMetaType<QList<SensorData>>("QList<SensorData>");

    m_worker->moveToThread(&m_dbThread);
    connect(&m_dbThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &DbWorker::connectFinished, this, &DbManager::connectResult);
    connect(m_worker, &DbWorker::latestDataReady, this, &DbManager::latestDataReceived);

    m_dbThread.start();
}

DbManager::~DbManager()
{
    m_dbThread.quit();
    m_dbThread.wait();
}

void DbManager::connectSqlServerAsync(const QString& host, int port, const QString& dbName, const QString& user, const QString& pwd)
{
    QMetaObject::invokeMethod(m_worker, "connectSqlServer", Qt::QueuedConnection,
                              Q_ARG(QString, host), Q_ARG(int, port), Q_ARG(QString, dbName), Q_ARG(QString, user), Q_ARG(QString, pwd));
}

void DbManager::insertSensorDataAsync(const SensorData& data)
{
    QMetaObject::invokeMethod(m_worker, "insertSensorData", Qt::QueuedConnection, Q_ARG(SensorData, data));
}

void DbManager::requestLatestSensorDataAsync(int limit)
{
    QMetaObject::invokeMethod(m_worker, "getLatestSensorData", Qt::QueuedConnection, Q_ARG(int, limit));
}
