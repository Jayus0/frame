#ifndef EAGLE_CORE_CONFIGFORMAT_H
#define EAGLE_CORE_CONFIGFORMAT_H

#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtCore/QByteArray>

namespace Eagle {
namespace Core {

/**
 * @brief 配置格式类型
 */
enum class ConfigFormat {
    JSON,   // JSON格式
    YAML,   // YAML格式
    INI     // INI格式
};

/**
 * @brief 配置格式解析器
 * 
 * 提供不同格式配置文件的解析和转换功能
 */
class ConfigFormatParser {
public:
    /**
     * @brief 检测配置格式
     * @param filePath 文件路径
     * @param content 文件内容（可选，如果提供则优先使用内容检测）
     * @return 检测到的格式
     */
    static ConfigFormat detectFormat(const QString& filePath, const QByteArray& content = QByteArray());
    
    /**
     * @brief 从文件加载配置
     * @param filePath 文件路径
     * @param format 格式（如果为Auto，则自动检测）
     * @return 配置数据
     */
    static QVariantMap loadFromFile(const QString& filePath, ConfigFormat format = ConfigFormat::JSON);
    
    /**
     * @brief 从内容解析配置
     * @param content 配置内容
     * @param format 格式
     * @return 配置数据
     */
    static QVariantMap parseContent(const QByteArray& content, ConfigFormat format);
    
    /**
     * @brief 保存配置到文件
     * @param config 配置数据
     * @param filePath 文件路径
     * @param format 格式（如果为Auto，则根据文件扩展名自动选择）
     * @return 是否成功
     */
    static bool saveToFile(const QVariantMap& config, const QString& filePath, ConfigFormat format = ConfigFormat::JSON);
    
    /**
     * @brief 将配置转换为指定格式的字符串
     * @param config 配置数据
     * @param format 目标格式
     * @return 格式化的字符串
     */
    static QByteArray formatContent(const QVariantMap& config, ConfigFormat format);
    
    /**
     * @brief 格式转换
     * @param content 源内容
     * @param sourceFormat 源格式
     * @param targetFormat 目标格式
     * @return 转换后的内容
     */
    static QByteArray convertFormat(const QByteArray& content, ConfigFormat sourceFormat, ConfigFormat targetFormat);
    
    /**
     * @brief 获取格式的文件扩展名
     */
    static QString getFileExtension(ConfigFormat format);
    
    /**
     * @brief 从文件扩展名获取格式
     */
    static ConfigFormat formatFromExtension(const QString& filePath);
    
private:
    // JSON解析
    static QVariantMap parseJson(const QByteArray& content);
    static QByteArray formatJson(const QVariantMap& config);
    
    // YAML解析（简化实现）
    static QVariantMap parseYaml(const QByteArray& content);
    static QByteArray formatYaml(const QVariantMap& config);
    
    // INI解析
    static QVariantMap parseIni(const QByteArray& content);
    static QByteArray formatIni(const QVariantMap& config);
    
    // 辅助方法
    static QString escapeYamlString(const QString& str);
    static QString unescapeYamlString(const QString& str);
    static QString indentYaml(int level);
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_CONFIGFORMAT_H
