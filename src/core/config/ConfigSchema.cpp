#include "eagle/core/ConfigSchema.h"
#include "eagle/core/Logger.h"
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QRegExp>
#include <QtCore/QDebug>

namespace Eagle {
namespace Core {

ConfigSchema::ConfigSchema()
    : m_rootProperty(nullptr)
{
}

ConfigSchema::~ConfigSchema()
{
    if (m_rootProperty) {
        delete m_rootProperty;
    }
}

bool ConfigSchema::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("ConfigSchema", QString("无法打开Schema文件: %1").arg(filePath));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    return loadFromJson(data);
}

bool ConfigSchema::loadFromJson(const QByteArray& json)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError) {
        Logger::error("ConfigSchema", QString("JSON Schema解析错误: %1").arg(error.errorString()));
        return false;
    }
    
    if (!doc.isObject()) {
        Logger::error("ConfigSchema", "JSON Schema必须是对象");
        return false;
    }
    
    QVariantMap schemaObj = doc.object().toVariantMap();
    
    // 解析标题和描述
    m_title = schemaObj.value("title").toString();
    m_description = schemaObj.value("description").toString();
    
    // 解析根属性
    m_rootProperty = parseProperty(schemaObj);
    if (!m_rootProperty) {
        Logger::error("ConfigSchema", "无法解析Schema属性");
        return false;
    }
    
    Logger::info("ConfigSchema", QString("Schema加载成功: %1").arg(m_title));
    return true;
}

SchemaProperty* ConfigSchema::parseProperty(const QVariantMap& schemaObj)
{
    SchemaProperty* property = new SchemaProperty();
    
    // 解析类型
    QVariant typeVar = schemaObj.value("type");
    if (typeVar.type() == QVariant::String) {
        property->type = parseType(typeVar.toString());
    } else if (typeVar.type() == QVariant::List) {
        // 支持类型数组（取第一个）
        QVariantList typeList = typeVar.toList();
        if (!typeList.isEmpty()) {
            property->type = parseType(typeList.first().toString());
        }
    }
    
    // 解析描述
    property->description = schemaObj.value("description").toString();
    
    // 解析默认值
    if (schemaObj.contains("default")) {
        property->defaultValue = schemaObj.value("default");
    }
    
    // 解析必需字段
    property->required = schemaObj.value("required").toBool();
    
    // 解析枚举值
    if (schemaObj.contains("enum")) {
        property->enumValues = schemaObj.value("enum").toList();
    }
    
    // 解析数值范围
    if (schemaObj.contains("minimum")) {
        property->minimum = schemaObj.value("minimum");
    }
    if (schemaObj.contains("maximum")) {
        property->maximum = schemaObj.value("maximum");
    }
    
    // 解析长度限制
    if (schemaObj.contains("minLength")) {
        property->minLength = schemaObj.value("minLength").toInt();
    }
    if (schemaObj.contains("maxLength")) {
        property->maxLength = schemaObj.value("maxLength").toInt();
    }
    
    // 解析正则表达式
    if (schemaObj.contains("pattern")) {
        property->pattern = schemaObj.value("pattern").toString();
    }
    
    // 解析子属性（用于object类型）
    if (schemaObj.contains("properties")) {
        QVariantMap properties = schemaObj.value("properties").toMap();
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            QVariantMap propSchema = it.value().toMap();
            SchemaProperty* subProp = parseProperty(propSchema);
            if (subProp) {
                property->properties[it.key()] = *subProp;
                delete subProp;  // 复制后删除
            }
        }
    }
    
    // 解析必需字段列表（用于object类型）
    if (schemaObj.contains("required")) {
        QVariantList requiredList = schemaObj.value("required").toList();
        for (const QVariant& req : requiredList) {
            QString reqKey = req.toString();
            if (property->properties.contains(reqKey)) {
                property->properties[reqKey].required = true;
            }
        }
    }
    
    // 解析数组元素定义（用于array类型）
    if (schemaObj.contains("items")) {
        QVariantMap itemsSchema = schemaObj.value("items").toMap();
        property->items = parseProperty(itemsSchema);
    }
    
    // 解析依赖字段
    if (schemaObj.contains("dependencies")) {
        QVariantMap deps = schemaObj.value("dependencies").toMap();
        for (auto it = deps.begin(); it != deps.end(); ++it) {
            if (it.value().type() == QVariant::List) {
                QVariantList depList = it.value().toList();
                for (const QVariant& dep : depList) {
                    property->dependencies.append(dep.toString());
                }
            }
        }
    }
    
    return property;
}

SchemaType ConfigSchema::parseType(const QString& typeStr) const
{
    if (typeStr == "string") return SchemaType::String;
    if (typeStr == "number") return SchemaType::Number;
    if (typeStr == "integer") return SchemaType::Integer;
    if (typeStr == "boolean") return SchemaType::Boolean;
    if (typeStr == "object") return SchemaType::Object;
    if (typeStr == "array") return SchemaType::Array;
    if (typeStr == "null") return SchemaType::Null;
    return SchemaType::Any;
}

SchemaValidationResult ConfigSchema::validate(const QVariantMap& config) const
{
    SchemaValidationResult result;
    
    if (!m_rootProperty) {
        result.addError("", "Schema未加载或无效", "schema_invalid");
        return result;
    }
    
    return validateObject(config, m_rootProperty, "");
}

SchemaValidationResult ConfigSchema::validateObject(const QVariantMap& obj, const SchemaProperty* property, 
                                                   const QString& path) const
{
    SchemaValidationResult result;
    
    if (property->type != SchemaType::Object && property->type != SchemaType::Any) {
        result.addError(path, QString("期望类型为object，实际为: %1").arg(obj.value("type").toString()), "type_mismatch");
        return result;
    }
    
    // 检查必需字段
    for (auto it = property->properties.begin(); it != property->properties.end(); ++it) {
        const QString& key = it.key();
        const SchemaProperty& prop = it.value();
        QString fullPath = path.isEmpty() ? key : QString("%1.%2").arg(path, key);
        
        if (prop.required && !obj.contains(key)) {
            result.addError(fullPath, QString("必需字段缺失: %1").arg(key), "required");
        }
    }
    
    // 验证每个字段
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString& key = it.key();
        QString fullPath = path.isEmpty() ? key : QString("%1.%2").arg(path, key);
        
        if (property->properties.contains(key)) {
            const SchemaProperty& prop = property->properties[key];
            SchemaValidationResult propResult = validateValue(it.value(), &prop, fullPath);
            if (!propResult.valid) {
                result.errors.append(propResult.errors);
                result.valid = false;
            }
        } else if (property->type != SchemaType::Any) {
            // 如果Schema定义了properties，不允许额外字段
            result.addError(fullPath, QString("未知字段: %1").arg(key), "unknown_field");
        }
    }
    
    // 检查依赖关系
    for (auto it = property->properties.begin(); it != property->properties.end(); ++it) {
        const SchemaProperty& prop = it.value();
        if (!prop.dependencies.isEmpty() && obj.contains(it.key())) {
            if (!checkDependencies(obj, prop.dependencies, path)) {
                result.addError(path, QString("字段 %1 的依赖字段不满足").arg(it.key()), "dependencies");
            }
        }
    }
    
    return result;
}

SchemaValidationResult ConfigSchema::validateArray(const QVariantList& arr, const SchemaProperty* property, 
                                                  const QString& path) const
{
    SchemaValidationResult result;
    
    if (property->type != SchemaType::Array && property->type != SchemaType::Any) {
        result.addError(path, "期望类型为array", "type_mismatch");
        return result;
    }
    
    // 检查长度限制
    if (property->minLength >= 0 && arr.size() < property->minLength) {
        result.addError(path, QString("数组长度 %1 小于最小值 %2").arg(arr.size()).arg(property->minLength), "min_length");
    }
    if (property->maxLength >= 0 && arr.size() > property->maxLength) {
        result.addError(path, QString("数组长度 %1 大于最大值 %2").arg(arr.size()).arg(property->maxLength), "max_length");
    }
    
    // 验证数组元素
    if (property->items) {
        for (int i = 0; i < arr.size(); ++i) {
            QString itemPath = QString("%1[%2]").arg(path).arg(i);
            SchemaValidationResult itemResult = validateValue(arr[i], property->items, itemPath);
            if (!itemResult.valid) {
                result.errors.append(itemResult.errors);
                result.valid = false;
            }
        }
    }
    
    return result;
}

SchemaValidationResult ConfigSchema::validateValue(const QVariant& value, const SchemaProperty* property, 
                                                   const QString& path) const
{
    SchemaValidationResult result;
    
    // 处理null值
    if (value.isNull() || !value.isValid()) {
        if (property->type == SchemaType::Null || property->type == SchemaType::Any) {
            return result;  // null值允许
        }
        result.addError(path, "值不能为null", "null_value");
        return result;
    }
    
    // 根据类型验证
    switch (property->type) {
    case SchemaType::String: {
        if (value.type() != QVariant::String) {
            result.addError(path, QString("期望类型为string，实际为: %1").arg(value.typeName()), "type_mismatch");
            return result;
        }
        SchemaValidationResult strResult = validateString(value.toString(), property, path);
        if (!strResult.valid) {
            result.errors.append(strResult.errors);
            result.valid = false;
        }
        break;
    }
    case SchemaType::Number:
    case SchemaType::Integer: {
        bool ok;
        double num = value.toDouble(&ok);
        if (!ok) {
            result.addError(path, QString("期望类型为number，实际为: %1").arg(value.typeName()), "type_mismatch");
            return result;
        }
        SchemaValidationResult numResult = validateNumber(num, property, path);
        if (!numResult.valid) {
            result.errors.append(numResult.errors);
            result.valid = false;
        }
        break;
    }
    case SchemaType::Boolean: {
        if (value.type() != QVariant::Bool) {
            result.addError(path, QString("期望类型为boolean，实际为: %1").arg(value.typeName()), "type_mismatch");
            return result;
        }
        break;
    }
    case SchemaType::Object: {
        if (value.type() != QVariant::Map) {
            result.addError(path, QString("期望类型为object，实际为: %1").arg(value.typeName()), "type_mismatch");
            return result;
        }
        SchemaValidationResult objResult = validateObject(value.toMap(), property, path);
        if (!objResult.valid) {
            result.errors.append(objResult.errors);
            result.valid = false;
        }
        break;
    }
    case SchemaType::Array: {
        if (value.type() != QVariant::List) {
            result.addError(path, QString("期望类型为array，实际为: %1").arg(value.typeName()), "type_mismatch");
            return result;
        }
        SchemaValidationResult arrResult = validateArray(value.toList(), property, path);
        if (!arrResult.valid) {
            result.errors.append(arrResult.errors);
            result.valid = false;
        }
        break;
    }
    case SchemaType::Any:
        // 任意类型，不验证
        break;
    default:
        break;
    }
    
    // 检查枚举值
    if (!property->enumValues.isEmpty()) {
        bool found = false;
        for (const QVariant& enumVal : property->enumValues) {
            if (enumVal == value) {
                found = true;
                break;
            }
        }
        if (!found) {
            result.addError(path, QString("值不在枚举列表中").arg(path), "enum");
        }
    }
    
    return result;
}

SchemaValidationResult ConfigSchema::validateString(const QString& str, const SchemaProperty* property, 
                                                    const QString& path) const
{
    SchemaValidationResult result;
    
    // 检查长度限制
    if (property->minLength >= 0 && str.length() < property->minLength) {
        result.addError(path, QString("字符串长度 %1 小于最小值 %2").arg(str.length()).arg(property->minLength), "min_length");
    }
    if (property->maxLength >= 0 && str.length() > property->maxLength) {
        result.addError(path, QString("字符串长度 %1 大于最大值 %2").arg(str.length()).arg(property->maxLength), "max_length");
    }
    
    // 检查正则表达式
    if (!property->pattern.isEmpty()) {
        QRegExp regex(property->pattern);
        if (!regex.exactMatch(str)) {
            result.addError(path, QString("字符串不匹配模式: %1").arg(property->pattern), "pattern");
        }
    }
    
    return result;
}

SchemaValidationResult ConfigSchema::validateNumber(double num, const SchemaProperty* property, 
                                                    const QString& path) const
{
    SchemaValidationResult result;
    
    // 检查数值范围
    if (property->minimum.isValid()) {
        double minVal = property->minimum.toDouble();
        if (num < minVal) {
            result.addError(path, QString("数值 %1 小于最小值 %2").arg(num).arg(minVal), "minimum");
        }
    }
    if (property->maximum.isValid()) {
        double maxVal = property->maximum.toDouble();
        if (num > maxVal) {
            result.addError(path, QString("数值 %1 大于最大值 %2").arg(num).arg(maxVal), "maximum");
        }
    }
    
    return result;
}

bool ConfigSchema::checkDependencies(const QVariantMap& config, const QStringList& dependencies, 
                                     const QString& path) const
{
    for (const QString& dep : dependencies) {
        QString depPath = path.isEmpty() ? dep : QString("%1.%2").arg(path, dep);
        if (!config.contains(dep)) {
            return false;
        }
    }
    return true;
}

} // namespace Core
} // namespace Eagle
