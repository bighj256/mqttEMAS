#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QDebug>
#include <QList>
#include <QObject>
#include <QThread>
#include "dataparser.h"

// 负责在后台线程执行实际数据库操作的工作类
class DbWorker : public QObject
{
    Q_OBJECT
public:
    explicit DbWorker(QObject *parent = nullptr);
    ~DbWorker();

public slots:
    void connectSqlServer(const QString& host, int port, const QString& dbName, const QString& user, const QString& pwd);
    void insertSensorData(const SensorData& data);
    void getLatestSensorData(int limit);

signals:
    void connectFinished(bool success, const QString& errorMsg);
    void latestDataReady(const QList<SensorData>& data);

private:
    QSqlDatabase m_db;
};

// 面向主线程的单例管理器，负责分发任务到后台线程
class DbManager : public QObject
{
    Q_OBJECT
public:
    static DbManager& instance();

    // 异步连接 SQL Server
    void connectSqlServerAsync(const QString& host, int port, const QString& dbName, const QString& user, const QString& pwd);
    
    // 异步插入传感器数据
    void insertSensorDataAsync(const SensorData& data);

    // 异步获取最新数据，通过 latestDataReceived 信号返回
    void requestLatestSensorDataAsync(int limit = 500);

signals:
    void connectResult(bool success, const QString& errorMsg);
    void latestDataReceived(const QList<SensorData>& data);

private:
    DbManager();
    ~DbManager();
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    QThread m_dbThread;
    DbWorker *m_worker;
};

#endif // DBMANAGER_H
