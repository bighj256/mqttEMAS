#include "dbmanager.h"

DbManager& DbManager::instance()
{
    static DbManager instance;
    return instance;
}

DbManager::DbManager()
{
}

DbManager::~DbManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DbManager::connectSqlServer(const QString& host, int port, const QString& dbName, const QString& user, const QString& pwd)
{
    if (m_db.isOpen()) {
        return true;
    }

    if(QSqlDatabase::contains("qt_sql_default_connection")) {
        m_db = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        m_db = QSqlDatabase::addDatabase("QODBC");
    }

    // 配置 ODBC 连接字符串 (DSN-less connection) 用于 SQL Server
    // 采用默认的 "SQL Server" Windows 自带驱动
    QString dsn = QString("DRIVER={SQL Server};SERVER=%1,%2;DATABASE=%3;")
                      .arg(host).arg(port).arg(dbName);
    
    m_db.setDatabaseName(dsn);
    m_db.setUserName(user);
    m_db.setPassword(pwd);

    if (!m_db.open()) {
        qWarning() << "数据库连接失败:" << m_db.lastError().text();
        return false;
    }

    qDebug() << "成功连接到 SQL Server 数据库!";
    
    // 初始化表结构 (如果不存在则创建)
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

    return true;
}

bool DbManager::insertSensorData(const SensorData& data)
{
    if (!m_db.isOpen()) {
        // 如果未连接，静默返回或者记录日志
        return false;
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
        return false;
    }

    return true;
}

QList<SensorData> DbManager::getLatestSensorData(int limit)
{
    QList<SensorData> results;
    if (!m_db.isOpen()) {
        qWarning() << "数据库未打开，无法获取数据";
        return results;
    }

    QSqlQuery query(m_db);
    // SQL Server uses TOP <N> instead of LIMIT
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
    
    return results;
}
