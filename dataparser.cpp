#include "dataparser.h"
#include <QDateTime>
#include <QDebug>

DataParser::DataParser(QObject *parent) : QObject(parent) {}


//主入口函数：自动格式识别-对应函数解析->验证->返回
SensorData DataParser::parse(const QString &rawData)
{
    SensorData result;
    result.rawData = rawData;
    result.isValid = false;
    result.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    if (rawData.trimmed().isEmpty()) {
        qDebug() << "接收到的数据为空";
        return result;
    }

    Format format = detectFormat(rawData);

    switch (format) {
    case JSON:
        result = parseJson(rawData);
        result.isValid = validateData(result);
        break;
    case CSV:
        result = parseCsv(rawData);
        result.isValid = validateData(result);
        break;
    case KV:
        result = parseKeyValue(rawData);
        result.isValid = validateData(result);
        break;
    default:
        qWarning() << "无法识别的数据格式:" << rawData;
        result.isValid=false;
        break;
    }

    qDebug()<<"result time"<<result.timestamp;

    return result;
}

//格式识别
DataParser::Format DataParser::detectFormat(const QString &data)
{
    QString trimmed = data.trimmed();

    // 检测JSON格式
    if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
        return JSON;
    }

    // 检测CSV格式（纯数字逗号分隔）
    QRegularExpression csvPattern("^\\s*(-?\\d+(?:\\.\\d+)?)\\s*,"
                                  "\\s*(-?\\d+(?:\\.\\d+)?)(?:\\s*,"
                                  "\\s*([^,]+))?\\s*$");
    if (csvPattern.match(trimmed).hasMatch()) {
        return CSV;
    }

    // 检测键值对格式
    if (trimmed.contains('=') && (trimmed.contains("temp", Qt::CaseInsensitive) ||
                                  trimmed.contains("hum", Qt::CaseInsensitive))) {
        return KV;
    }

    return UNKNOWN;
}

//检查是否为JSON对象(JavaScript对象表示方法）用于传输结构化数据
SensorData DataParser::parseJson(const QString &data)
{
    SensorData result;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON解析错误:" << error.errorString();
        return result;
    }

    if (!doc.isObject()) {
        qWarning() << "JSON数据不是对象";
        return result;
    }

    QJsonObject obj = doc.object();

    // 尝试不同的键名（兼容多种JSON格式）
    QStringList tempKeys = {"temperature", "temp", "t", "温度"};
    QStringList humKeys = {"humidity", "hum", "h", "湿度"};
    QStringList deviceKeys = {"deviceId", "device", "id", "device_id"};

    // 查找温度
    for (const QString &key : tempKeys) {
        if (obj.contains(key)) {
            result.temperature = obj[key].toDouble();
            break;
        }
    }

    // 查找湿度
    for (const QString &key : humKeys) {
        if (obj.contains(key)) {
            result.humidity = obj[key].toDouble();
            break;
        }
    }

    // 查找设备ID
    for (const QString &key : deviceKeys) {
        if (obj.contains(key)) {
            result.deviceId = obj[key].toString();
            break;
        }
    }

    return result;
}

//Csv格式检测
SensorData DataParser::parseCsv(const QString &data)
{
    SensorData result;

    // 使用正则表达式匹配，支持设备ID
    QRegularExpression pattern("^\\s*(-?\\d+(?:\\.\\d+)?)\\s*,"
                               "\\s*(-?\\d+(?:\\.\\d+)?)(?:\\s*,"
                               "\\s*([^,]+))?\\s*$");

    /*解读正则表达式
     * ^\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)(?:\s*,\s*([^,]+))?\s*$
     * ^匹配字符串开始位置  \s*匹配空白符（允许字段前后有空格） $结束字符
     * (-?\d+(?:\.\d+)?)---温度
     * (-?\d+(?:\.\d+)?)--湿度
     * (?:\s*,\s*([^,]+))?--设备ID(可选）
     * ([^,]+)一个不包含逗号的非空字符串
     * */

    QRegularExpressionMatch match = pattern.match(data.trimmed());

    if (match.hasMatch()) {
        // 获取温度
        QString tempStr = match.captured(1);
        bool tempOk;
        double temp = tempStr.toDouble(&tempOk);
        if (tempOk) {
            result.temperature = temp;
        }

        // 获取湿度
        QString humStr = match.captured(2);
        bool humOk;
        double hum = humStr.toDouble(&humOk);
        if (humOk) {
            result.humidity = hum;
        }

        // 获取设备ID（可选）
        if (match.capturedLength(3) > 0) {
            result.deviceId = match.captured(3).trimmed();
        }
    } else {
        // 如果正则匹配失败，回退到原来的简单分割方式
        QStringList parts = data.split(',');
        if (parts.size() >= 2) {
            result.temperature = parts[0].trimmed().toDouble();
            result.humidity = parts[1].trimmed().toDouble();

            // 如果有第三个部分，作为设备ID
            if (parts.size() >= 3) {
                result.deviceId = parts[2].trimmed();
            }
        }
    }

    return result;
}

//键值对格式检测
SensorData DataParser::parseKeyValue(const QString &data)
{
    SensorData result;
    bool ok;
    QStringList pairs = data.split(',');
    for (const QString &pair : pairs) {
        QStringList kv = pair.split('=');
        if (kv.size() == 2) {
            QString key = kv[0].trimmed().toLower();
            QString value = kv[1].trimmed();

            double numValue = value.toDouble(&ok);
            if(!ok) continue;  //跳过非法值
            if (key == "temp" || key == "temperature" || key == "t") {
                result.temperature = numValue;
            } else if (key == "hum" || key == "humidity" || key == "h") {
                result.humidity = numValue;
            } else if (key == "device" || key == "deviceid" || key == "id") {
                result.deviceId = value;
            }
        }
    }
    return result;
}

//合理数据范围
bool DataParser::validateData(const SensorData &data)
{

    // 温度验证：-50°C到100°C为合理范围
    if (data.temperature < -50 || data.temperature > 100) {
        qWarning() << "温度数据异常:" << data.temperature;
        return false;
    }

    // 湿度验证：0%到100%为合理范围
    if (data.humidity < 0 || data.humidity > 100) {
        qWarning() << "湿度数据异常:" << data.humidity;
        return false;
    }

    return true;
}

