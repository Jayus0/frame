#include "eagle/core/ConfigFormat.h"
#include "eagle/core/Logger.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>
#include <QtCore/QTextStream>
#include <QtCore/QRegExp>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryFile>

namespace Eagle {
namespace Core {

ConfigFormat ConfigFormatParser::detectFormat(const QString& filePath, const QByteArray& content)
{
    // 如果提供了内容，尝试从内容检测
    if (!content.isEmpty()) {
        QByteArray trimmed = content.trimmed();
        
        // JSON检测：以 { 或 [ 开头
        if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
            return ConfigFormat::JSON;
        }
        
        // YAML检测：包含 : 且不是JSON格式
        if (trimmed.contains(':') && !trimmed.startsWith('{') && !trimmed.startsWith('[')) {
            // 简单的YAML检测：包含键值对模式
            QString str = QString::fromUtf8(trimmed);
            if (str.contains(QRegExp("^\\s*\\w+\\s*:"))) {
                return ConfigFormat::YAML;
            }
        }
        
        // INI检测：包含 [section] 模式
        QString str = QString::fromUtf8(trimmed);
        if (str.contains(QRegExp("^\\s*\\[.*\\]\\s*$", Qt::CaseInsensitive))) {
            return ConfigFormat::INI;
        }
    }
    
    // 根据文件扩展名检测
    return formatFromExtension(filePath);
}

ConfigFormat ConfigFormatParser::formatFromExtension(const QString& filePath)
{
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    
    if (ext == "json") {
        return ConfigFormat::JSON;
    } else if (ext == "yaml" || ext == "yml") {
        return ConfigFormat::YAML;
    } else if (ext == "ini" || ext == "conf" || ext == "cfg") {
        return ConfigFormat::INI;
    }
    
    // 默认返回JSON
    return ConfigFormat::JSON;
}

QString ConfigFormatParser::getFileExtension(ConfigFormat format)
{
    switch (format) {
    case ConfigFormat::JSON:
        return "json";
    case ConfigFormat::YAML:
        return "yaml";
    case ConfigFormat::INI:
        return "ini";
    default:
        return "json";
    }
}

QVariantMap ConfigFormatParser::loadFromFile(const QString& filePath, ConfigFormat format)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("ConfigFormatParser", QString("无法打开配置文件: %1").arg(filePath));
        return QVariantMap();
    }
    
    QByteArray content = file.readAll();
    file.close();
    
    // 如果格式为JSON但未指定，自动检测
    if (format == ConfigFormat::JSON) {
        format = detectFormat(filePath, content);
    }
    
    return parseContent(content, format);
}

QVariantMap ConfigFormatParser::parseContent(const QByteArray& content, ConfigFormat format)
{
    switch (format) {
    case ConfigFormat::JSON:
        return parseJson(content);
    case ConfigFormat::YAML:
        return parseYaml(content);
    case ConfigFormat::INI:
        return parseIni(content);
    default:
        Logger::warning("ConfigFormatParser", "未知的配置格式，使用JSON解析");
        return parseJson(content);
    }
}

bool ConfigFormatParser::saveToFile(const QVariantMap& config, const QString& filePath, ConfigFormat format)
{
    // 如果格式为JSON但未指定，根据文件扩展名自动选择
    if (format == ConfigFormat::JSON) {
        format = formatFromExtension(filePath);
    }
    
    QByteArray content = formatContent(config, format);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::error("ConfigFormatParser", QString("无法写入配置文件: %1").arg(filePath));
        return false;
    }
    
    file.write(content);
    file.close();
    
    return true;
}

QByteArray ConfigFormatParser::formatContent(const QVariantMap& config, ConfigFormat format)
{
    switch (format) {
    case ConfigFormat::JSON:
        return formatJson(config);
    case ConfigFormat::YAML:
        return formatYaml(config);
    case ConfigFormat::INI:
        return formatIni(config);
    default:
        return formatJson(config);
    }
}

QByteArray ConfigFormatParser::convertFormat(const QByteArray& content, ConfigFormat sourceFormat, ConfigFormat targetFormat)
{
    QVariantMap config = parseContent(content, sourceFormat);
    return formatContent(config, targetFormat);
}

// ==================== JSON解析 ====================

QVariantMap ConfigFormatParser::parseJson(const QByteArray& content)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError) {
        Logger::error("ConfigFormatParser", QString("JSON解析错误: %1").arg(error.errorString()));
        return QVariantMap();
    }
    
    if (!doc.isObject()) {
        Logger::error("ConfigFormatParser", "JSON文档不是对象");
        return QVariantMap();
    }
    
    return doc.object().toVariantMap();
}

QByteArray ConfigFormatParser::formatJson(const QVariantMap& config)
{
    QJsonObject obj = QJsonObject::fromVariantMap(config);
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Indented);
}

// ==================== YAML解析（简化实现） ====================

QVariantMap ConfigFormatParser::parseYaml(const QByteArray& content)
{
    // 简化的YAML解析实现
    // 注意：这是一个基础实现，不支持完整的YAML规范
    // 生产环境建议使用yaml-cpp等专业库
    
    QVariantMap result;
    QStringList lines = QString::fromUtf8(content).split('\n');
    QString currentSection;
    
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        
        // 跳过空行和注释
        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            continue;
        }
        
        // 处理section（如 "database:")
        if (trimmed.endsWith(':') && !trimmed.contains(' ')) {
            currentSection = trimmed.left(trimmed.length() - 1).trimmed();
            if (!result.contains(currentSection)) {
                result[currentSection] = QVariantMap();
            }
            continue;
        }
        
        // 处理键值对
        int colonPos = trimmed.indexOf(':');
        if (colonPos > 0) {
            QString key = trimmed.left(colonPos).trimmed();
            QString value = trimmed.mid(colonPos + 1).trimmed();
            
            // 移除引号
            if ((value.startsWith('"') && value.endsWith('"')) ||
                (value.startsWith('\'') && value.endsWith('\''))) {
                value = value.mid(1, value.length() - 2);
            }
            
            // 尝试转换为数字或布尔值
            QVariant variantValue = value;
            bool ok;
            int intValue = value.toInt(&ok);
            if (ok) {
                variantValue = intValue;
            } else {
                double doubleValue = value.toDouble(&ok);
                if (ok) {
                    variantValue = doubleValue;
                } else if (value.toLower() == "true" || value.toLower() == "yes") {
                    variantValue = true;
                } else if (value.toLower() == "false" || value.toLower() == "no") {
                    variantValue = false;
                }
            }
            
            if (!currentSection.isEmpty()) {
                QVariantMap section = result[currentSection].toMap();
                section[key] = variantValue;
                result[currentSection] = section;
            } else {
                result[key] = variantValue;
            }
        }
    }
    
    return result;
}

QByteArray ConfigFormatParser::formatYaml(const QVariantMap& config)
{
    // 简化的YAML格式化实现
    QByteArray result;
    QTextStream stream(&result);
    
    for (auto it = config.begin(); it != config.end(); ++it) {
        const QString& key = it.key();
        const QVariant& value = it.value();
        
        if (value.type() == QVariant::Map) {
            // 嵌套对象
            stream << key << ":\n";
            QVariantMap nested = value.toMap();
            for (auto nestedIt = nested.begin(); nestedIt != nested.end(); ++nestedIt) {
                stream << "  " << nestedIt.key() << ": " << nestedIt.value().toString() << "\n";
            }
        } else {
            stream << key << ": " << value.toString() << "\n";
        }
    }
    
    return result;
}

QString ConfigFormatParser::escapeYamlString(const QString& str)
{
    // 简化的YAML字符串转义
    QString result = str;
    result.replace("\\", "\\\\");
    result.replace("\"", "\\\"");
    result.replace("\n", "\\n");
    return result;
}

QString ConfigFormatParser::unescapeYamlString(const QString& str)
{
    // 简化的YAML字符串反转义
    QString result = str;
    result.replace("\\n", "\n");
    result.replace("\\\"", "\"");
    result.replace("\\\\", "\\");
    return result;
}

QString ConfigFormatParser::indentYaml(int level)
{
    return QString(level * 2, ' ');
}

// ==================== INI解析 ====================

QVariantMap ConfigFormatParser::parseIni(const QByteArray& content)
{
    // 使用临时文件解析INI
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        Logger::error("ConfigFormatParser", "无法创建临时文件");
        return QVariantMap();
    }
    
    tempFile.write(content);
    tempFile.flush();
    
    QSettings settings(tempFile.fileName(), QSettings::IniFormat);
    QVariantMap result;
    
    // 获取所有键
    QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        result[key] = settings.value(key);
    }
    
    // 获取所有组
    QStringList groups = settings.childGroups();
    for (const QString& group : groups) {
        settings.beginGroup(group);
        QVariantMap groupMap;
        QStringList groupKeys = settings.allKeys();
        for (const QString& key : groupKeys) {
            groupMap[key] = settings.value(key);
        }
        result[group] = groupMap;
        settings.endGroup();
    }
    
    return result;
}

QByteArray ConfigFormatParser::formatIni(const QVariantMap& config)
{
    // 使用临时文件格式化INI
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        Logger::error("ConfigFormatParser", "无法创建临时文件");
        return QByteArray();
    }
    tempFile.close();
    
    QSettings settings(tempFile.fileName(), QSettings::IniFormat);
    
    // 写入配置
    for (auto it = config.begin(); it != config.end(); ++it) {
        const QString& key = it.key();
        const QVariant& value = it.value();
        
        if (value.type() == QVariant::Map) {
            // 嵌套对象作为组
            settings.beginGroup(key);
            QVariantMap nested = value.toMap();
            for (auto nestedIt = nested.begin(); nestedIt != nested.end(); ++nestedIt) {
                settings.setValue(nestedIt.key(), nestedIt.value());
            }
            settings.endGroup();
        } else {
            settings.setValue(key, value);
        }
    }
    
    settings.sync();
    
    // 读取格式化后的内容
    QFile file(tempFile.fileName());
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray content = file.readAll();
        file.close();
        return content;
    }
    
    return QByteArray();
}

} // namespace Core
} // namespace Eagle
