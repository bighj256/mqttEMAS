#ifndef DATAPARSER_H
#define DATAPARSER_H

#include <QObject>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QDateTime>
#include <QMetaType>

/*数据解析器，用于将原始字符串数据(JSON/CSV/键值对)解析为传感器数据
 * 支持自动识别输入格式，并进行有效性检验*/


// 传感器数据结构，用于储存解析后的数据
struct SensorData {
    double temperature;    // 温度
    double humidity;       // 湿度
    QString timestamp;     // 时间戳
    QString deviceId;      // 设备ID
    QString rawData;       // 原始数据
    bool isValid;          // 数据是否有效

    SensorData() : temperature(0.0), humidity(0.0), timestamp(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")), isValid(false) {}
};

class DataParser : public QObject
{
    Q_OBJECT

public:

    //parent 父对象指针，用于QT的内存管理
    explicit DataParser(QObject *parent = nullptr);

    /*解析原始传感器数据，自动监测格式，调用相关函数并验证数据合理性
     * return 解析后的SensorData对象*/
    SensorData parse(const QString &rawData);

    // 数据格式枚举
    enum Format {
        JSON,      // JSON格式，如 {"temp":25}
        CSV,       // CSV格式，如 "25,60" 或 "25,60,device01"
        KV,        // 键值对格式，如"temp=25,hum=60"
        UNKNOWN    // 未知格式
    };

private:
    // 检测数据格式,return 检测到的Format枚举值
    Format detectFormat(const QString &data);

    // 解析JSON格式
    SensorData parseJson(const QString &data);

    // 解析CSV格式
    SensorData parseCsv(const QString &data);

    // 解析键值对格式
    SensorData parseKeyValue(const QString &data);

    // 验证数据合理性，温度范围(-50~100摄氏度），湿度范围(0%~100%)
    bool validateData(const SensorData &data);

};

Q_DECLARE_METATYPE(SensorData);

#endif // DATAPARSER_H
