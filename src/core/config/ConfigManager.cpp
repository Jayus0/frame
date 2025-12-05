#include "eagle/core/ConfigManager.h"
#include "ConfigManager_p.h"
#include "eagle/core/ConfigEncryption.h"
#include "eagle/core/ConfigSchema.h"
#include "eagle/core/ConfigVersion.h"
#include "eagle/core/ConfigFormat.h"
#include "eagle/core/Logger.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>

namespace Eagle {
namespace Core {

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
    , d_ptr(new ConfigManagerPrivate)
{
    auto* d = d_func();
    d->versionManager = new ConfigVersionManager(this);
}

ConfigManager::~ConfigManager()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    delete d_ptr;
}

bool ConfigManager::loadFromFile(const QString& filePath, ConfigLevel level, ConfigFormat format)
{
    // 如果格式为JSON但未指定，自动检测
    if (format == ConfigFormat::JSON) {
        format = ConfigFormatParser::formatFromExtension(filePath);
    }
    
    QVariantMap config = ConfigFormatParser::loadFromFile(filePath, format);
    if (config.isEmpty()) {
        Logger::error("ConfigManager", QString("无法加载配置文件: %1").arg(filePath));
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    switch (level) {
    case Global:
        d->globalConfig = config;
        break;
    case User:
        d->userConfig = config;
        break;
    default:
        Logger::warning("ConfigManager", "不支持的配置级别");
        return false;
    }
    
    Logger::info("ConfigManager", QString("配置加载成功，级别: %1, 格式: %2")
        .arg(level).arg(static_cast<int>(format)));
    emit configReloaded();
    return true;
}

bool ConfigManager::loadFromJson(const QByteArray& json, ConfigLevel level)
{
    QVariantMap config = ConfigFormatParser::parseContent(json, ConfigFormat::JSON);
    if (config.isEmpty()) {
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    switch (level) {
    case Global:
        d->globalConfig = config;
        break;
    case User:
        d->userConfig = config;
        break;
    default:
        Logger::warning("ConfigManager", "不支持的配置级别");
        return false;
    }
    
    Logger::info("ConfigManager", QString("配置加载成功，级别: %1").arg(level));
    emit configReloaded();
    return true;
}

bool ConfigManager::loadFromYaml(const QByteArray& yaml, ConfigLevel level)
{
    QVariantMap config = ConfigFormatParser::parseContent(yaml, ConfigFormat::YAML);
    if (config.isEmpty()) {
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    switch (level) {
    case Global:
        d->globalConfig = config;
        break;
    case User:
        d->userConfig = config;
        break;
    default:
        Logger::warning("ConfigManager", "不支持的配置级别");
        return false;
    }
    
    Logger::info("ConfigManager", QString("YAML配置加载成功，级别: %1").arg(level));
    emit configReloaded();
    return true;
}

bool ConfigManager::loadFromIni(const QByteArray& ini, ConfigLevel level)
{
    QVariantMap config = ConfigFormatParser::parseContent(ini, ConfigFormat::INI);
    if (config.isEmpty()) {
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    switch (level) {
    case Global:
        d->globalConfig = config;
        break;
    case User:
        d->userConfig = config;
        break;
    default:
        Logger::warning("ConfigManager", "不支持的配置级别");
        return false;
    }
    
    Logger::info("ConfigManager", QString("INI配置加载成功，级别: %1").arg(level));
    emit configReloaded();
    return true;
}

void ConfigManager::loadFromEnvironment()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList keys = env.keys();
    
    for (const QString& key : keys) {
        if (key.startsWith("EAGLE_")) {
            QString configKey = key.mid(6).toLower().replace("_", ".");
            QString value = env.value(key);
            d->globalConfig[configKey] = value;
        }
    }
    
    Logger::info("ConfigManager", "环境变量配置加载完成");
}

QVariant ConfigManager::get(const QString& key, const QVariant& defaultValue) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 按优先级查找：插件配置 > 用户配置 > 全局配置
    // 这里简化处理，实际应该支持插件级别的配置
    if (d->userConfig.contains(key)) {
        return d->userConfig[key];
    }
    if (d->globalConfig.contains(key)) {
        return d->globalConfig[key];
    }
    
    return defaultValue;
}

void ConfigManager::set(const QString& key, const QVariant& value, ConfigLevel level)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QVariant oldValue;
    QVariantMap* targetConfig = nullptr;
    
    switch (level) {
    case Global:
        oldValue = d->globalConfig.value(key);
        targetConfig = &d->globalConfig;
        break;
    case User:
        oldValue = d->userConfig.value(key);
        targetConfig = &d->userConfig;
        break;
    default:
        Logger::warning("ConfigManager", "不支持的配置级别");
        return;
    }
    
    if (targetConfig) {
        (*targetConfig)[key] = value;
        if (oldValue != value) {
            emit configChanged(key, oldValue, value);
        }
    }
}

QVariantMap ConfigManager::getAll() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QVariantMap result = d->globalConfig;
    // 用户配置覆盖全局配置
    for (auto it = d->userConfig.begin(); it != d->userConfig.end(); ++it) {
        result[it.key()] = it.value();
    }
    return result;
}

QVariantMap ConfigManager::getByPrefix(const QString& prefix) const
{
    QVariantMap all = getAll();
    QVariantMap result;
    
    for (auto it = all.begin(); it != all.end(); ++it) {
        if (it.key().startsWith(prefix)) {
            QString key = it.key().mid(prefix.length());
            if (key.startsWith(".")) {
                key = key.mid(1);
            }
            result[key] = it.value();
        }
    }
    
    return result;
}

bool ConfigManager::updateConfig(const QVariantMap& config, ConfigLevel level)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QVariantMap* targetConfig = nullptr;
    switch (level) {
    case Global:
        targetConfig = &d->globalConfig;
        break;
    case User:
        targetConfig = &d->userConfig;
        break;
    default:
        return false;
    }
    
    if (targetConfig) {
        // 保存当前配置用于版本管理
        QVariantMap oldConfig = *targetConfig;
        
        for (auto it = config.begin(); it != config.end(); ++it) {
            QVariant oldValue = targetConfig->value(it.key());
            (*targetConfig)[it.key()] = it.value();
            if (oldValue != it.value()) {
                emit configChanged(it.key(), oldValue, it.value());
            }
        }
        
        // 创建新版本（如果版本管理启用）
        if (d->versionManager && d->versionManager->isEnabled()) {
            QVariantMap newConfig = *targetConfig;
            locker.unlock();
            d->versionManager->createVersion(newConfig, "system", "配置更新");
            locker.relock();
        }
        
        emit configReloaded();
        return true;
    }
    
    return false;
}

bool ConfigManager::saveToFile(const QString& filePath, ConfigLevel level, ConfigFormat format)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QVariantMap* sourceConfig = nullptr;
    switch (level) {
    case Global:
        sourceConfig = &d->globalConfig;
        break;
    case User:
        sourceConfig = &d->userConfig;
        break;
    default:
        return false;
    }
    
    if (!sourceConfig) {
        return false;
    }
    
    QVariantMap configToSave = *sourceConfig;
    
    // 如果启用加密，加密敏感字段
    if (d->encryptionEnabled && !d->sensitiveKeys.isEmpty()) {
        configToSave = ConfigEncryption::encryptConfig(configToSave, d->sensitiveKeys, d->encryptionKey);
    }
    
    // 如果格式为JSON但未指定，根据文件扩展名自动选择
    if (format == ConfigFormat::JSON) {
        format = ConfigFormatParser::formatFromExtension(filePath);
    }
    
    locker.unlock();
    
    // 保存到文件
    bool success = ConfigFormatParser::saveToFile(configToSave, filePath, format);
    
    // 创建新版本（如果版本管理启用）
    if (success && d->versionManager && d->versionManager->isEnabled()) {
        d->versionManager->createVersion(configToSave, "system", QString("保存到文件: %1").arg(filePath));
    }
    
    if (success) {
        Logger::info("ConfigManager", QString("配置保存成功: %1 (格式: %2)").arg(filePath)
            .arg(static_cast<int>(format)));
    }
    
    return success;
}

bool ConfigManager::validateConfig(const QVariantMap& config, const QString& schemaPath) const
{
    if (schemaPath.isEmpty()) {
        Logger::warning("ConfigManager", "Schema路径为空，跳过验证");
        return true;
    }
    
    ConfigSchema schema;
    if (!schema.loadFromFile(schemaPath)) {
        Logger::error("ConfigManager", QString("无法加载Schema文件: %1").arg(schemaPath));
        return false;
    }
    
    SchemaValidationResult result = schema.validate(config);
    if (!result.valid) {
        Logger::error("ConfigManager", QString("配置验证失败，发现 %1 个错误:").arg(result.errors.size()));
        for (const SchemaValidationError& error : result.errors) {
            Logger::error("ConfigManager", QString("  [%1] %2: %3").arg(error.path, error.code, error.message));
        }
        return false;
    }
    
    Logger::info("ConfigManager", "配置验证通过");
    return true;
}

void ConfigManager::setSchemaPath(const QString& schemaPath)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->schemaPath = schemaPath;
    Logger::info("ConfigManager", QString("设置Schema路径: %1").arg(schemaPath));
}

QString ConfigManager::schemaPath() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->schemaPath;
}

ConfigVersionManager* ConfigManager::versionManager() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->versionManager;
}

int ConfigManager::createConfigVersion(const QString& author, const QString& description)
{
    auto* d = d_func();
    if (!d->versionManager || !d->versionManager->isEnabled()) {
        Logger::warning("ConfigManager", "版本管理未启用");
        return 0;
    }
    
    QMutexLocker locker(&d->mutex);
    QVariantMap currentConfig = getAll();
    locker.unlock();
    
    return d->versionManager->createVersion(currentConfig, author, description);
}

bool ConfigManager::rollbackConfig(int version)
{
    auto* d = d_func();
    if (!d->versionManager || !d->versionManager->isEnabled()) {
        Logger::warning("ConfigManager", "版本管理未启用");
        return false;
    }
    
    QVariantMap rolledBackConfig = d->versionManager->rollbackToVersion(version);
    if (rolledBackConfig.isEmpty()) {
        return false;
    }
    
    // 应用回滚的配置
    QMutexLocker locker(&d->mutex);
    d->globalConfig = rolledBackConfig;
    locker.unlock();
    
    emit configReloaded();
    Logger::info("ConfigManager", QString("配置回滚到版本: %1").arg(version));
    return true;
}

QList<ConfigVersion> ConfigManager::getConfigVersions(int limit) const
{
    const auto* d = d_func();
    if (!d->versionManager) {
        return QList<ConfigVersion>();
    }
    return d->versionManager->getVersions(limit);
}

ConfigVersion ConfigManager::getConfigVersion(int version) const
{
    const auto* d = d_func();
    if (!d->versionManager) {
        return ConfigVersion();
    }
    return d->versionManager->getVersion(version);
}

QList<ConfigDiff> ConfigManager::compareConfigVersions(int version1, int version2) const
{
    const auto* d = d_func();
    if (!d->versionManager) {
        return QList<ConfigDiff>();
    }
    return d->versionManager->compareVersions(version1, version2);
}

void ConfigManager::watchConfig(const QString& key, QObject* receiver, const char* method)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!receiver || !method) {
        return;
    }
    
    QByteArray normalizedMethod = QMetaObject::normalizedSignature(method);
    d->watchers[key].append(qMakePair(receiver, normalizedMethod));
    
    // 连接配置变更信号
    connect(this, &ConfigManager::configChanged, receiver, [=](const QString& changedKey, const QVariant&, const QVariant&) {
        if (changedKey == key) {
            const QMetaObject* metaObj = receiver->metaObject();
            int methodIndex = metaObj->indexOfMethod(normalizedMethod.constData());
            if (methodIndex != -1) {
                QMetaMethod method = metaObj->method(methodIndex);
                method.invoke(receiver, Qt::QueuedConnection);
            }
        }
    });
}

void ConfigManager::setEncryptionEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->encryptionEnabled = enabled;
    Logger::info("ConfigManager", QString("配置加密%1").arg(enabled ? "启用" : "禁用"));
}

void ConfigManager::setSensitiveKeys(const QStringList& keys)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->sensitiveKeys = keys;
    Logger::info("ConfigManager", QString("设置敏感配置键: %1").arg(keys.join(", ")));
}

void ConfigManager::setEncryptionKey(const QString& key)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->encryptionKey = key;
    ConfigEncryption::setDefaultKey(key);
    Logger::info("ConfigManager", "设置加密密钥");
}

} // namespace Core
} // namespace Eagle
