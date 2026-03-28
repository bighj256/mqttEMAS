#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QDebug>
#include <QList>
#include "dataparser.h"

class DbManager
{
public:
    static DbManager& instance();

    // Setup SQL Server connection
    bool connectSqlServer(const QString& host, int port, const QString& dbName, const QString& user, const QString& pwd);
    
    // Insert new sensor data
    bool insertSensorData(const SensorData& data);

    // Get latest data
    QList<SensorData> getLatestSensorData(int limit = 500);

private:
    DbManager();
    ~DbManager();
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    QSqlDatabase m_db;
};

#endif // DBMANAGER_H
