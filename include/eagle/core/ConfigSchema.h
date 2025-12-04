#ifndef EAGLE_CORE_CONFIGSCHEMA_H
#define EAGLE_CORE_CONFIGSCHEMA_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QVariantList>
#include <QtCore/QMap>

namespace Eagle {
namespace Core {

/**
 * @brief JSON Schema类型
 */
enum class SchemaType {
    String,
    Number,
    Integer,
    Boolean,
    Object,
    Array,
    Null,
    Any  // 任意类型
};

/**
 * @brief Schema验证错误
 */
struct SchemaValidationError {
    QString path;           // 错误路径（如：user.name）
    QString message;        // 错误消息
    QString code;           // 错误代码（如：required, type, format等）
    QVariant value;         // 导致错误的值
    
    SchemaValidationError()
    {}
    
    SchemaValidationError(const QString& p, const QString& m, const QString& c = QString())
        : path(p), message(m), code(c)
    {}
};

/**
 * @brief Schema验证结果
 */
struct SchemaValidationResult {
    bool valid;                             // 是否有效
    QList<SchemaValidationError> errors;   // 错误列表
    
    SchemaValidationResult()
        : valid(true)
    {}
    
    void addError(const QString& path, const QString& message, const QString& code = QString()) {
        valid = false;
        errors.append(SchemaValidationError(path, message, code));
    }
    
    void addError(const SchemaValidationError& error) {
        valid = false;
        errors.append(error);
    }
};

/**
 * @brief Schema属性定义
 */
struct SchemaProperty {
    SchemaType type;                    // 类型
    QString description;                // 描述
    QVariant defaultValue;             // 默认值
    bool required;                      // 是否必需
    QVariantList enumValues;           // 枚举值
    QVariant minimum;                  // 最小值（用于number/integer）
    QVariant maximum;                  // 最大值（用于number/integer）
    int minLength;                     // 最小长度（用于string/array）
    int maxLength;                     // 最大长度（用于string/array）
    QString pattern;                   // 正则表达式（用于string）
    QMap<QString, SchemaProperty> properties;  // 子属性（用于object）
    SchemaProperty* items;             // 数组元素定义（用于array）
    QStringList dependencies;          // 依赖字段列表
    
    SchemaProperty()
        : type(SchemaType::Any)
        , required(false)
        , minLength(-1)
        , maxLength(-1)
        , items(nullptr)
    {}
    
    ~SchemaProperty() {
        if (items) {
            delete items;
        }
    }
};

/**
 * @brief JSON Schema定义
 */
class ConfigSchema {
public:
    explicit ConfigSchema();
    ~ConfigSchema();
    
    /**
     * @brief 从JSON加载Schema
     */
    bool loadFromJson(const QByteArray& json);
    
    /**
     * @brief 从文件加载Schema
     */
    bool loadFromFile(const QString& filePath);
    
    /**
     * @brief 验证配置是否符合Schema
     */
    SchemaValidationResult validate(const QVariantMap& config) const;
    
    /**
     * @brief 获取Schema的根属性
     */
    const SchemaProperty* rootProperty() const { return m_rootProperty; }
    
    /**
     * @brief 获取Schema的标题
     */
    QString title() const { return m_title; }
    
    /**
     * @brief 获取Schema的描述
     */
    QString description() const { return m_description; }
    
    /**
     * @brief 检查Schema是否有效
     */
    bool isValid() const { return m_rootProperty != nullptr; }
    
private:
    SchemaProperty* m_rootProperty;
    QString m_title;
    QString m_description;
    
    // 解析辅助方法
    SchemaProperty* parseProperty(const QVariantMap& schemaObj);
    SchemaType parseType(const QString& typeStr) const;
    SchemaValidationResult validateValue(const QVariant& value, const SchemaProperty* property, 
                                        const QString& path) const;
    SchemaValidationResult validateObject(const QVariantMap& obj, const SchemaProperty* property, 
                                         const QString& path) const;
    SchemaValidationResult validateArray(const QVariantList& arr, const SchemaProperty* property, 
                                        const QString& path) const;
    SchemaValidationResult validateString(const QString& str, const SchemaProperty* property, 
                                         const QString& path) const;
    SchemaValidationResult validateNumber(double num, const SchemaProperty* property, 
                                         const QString& path) const;
    bool checkDependencies(const QVariantMap& config, const QStringList& dependencies, 
                          const QString& path) const;
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_CONFIGSCHEMA_H
