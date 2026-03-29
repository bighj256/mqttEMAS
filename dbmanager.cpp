#include "dbmanager.h"
#include <QMetaObject>

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
    QString sql = QString("SELECT TOP %1 DeviceId, Temperature, Humidity, Timestamp "
                          "FROM SensorDataTable ORDER BY Timestamp DESC").arg(limit);
    
    if (query.exec(sql)) {
        while (query.next()) {
            SensorData data;
            data.deviceId = query.value("DeviceId").toString();
            data.temperature = query.value("Temperature").toDouble();
            data.humidity = query.value("Humidity").toDouble();
            data.timestamp = query.value("Timestamp").toDateTime().toString("yyyy-MM-dd HH:mm:ss");
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
