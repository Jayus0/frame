#include "eagle/core/ConfigManager.h"
#include "ConfigManager_p.h"
#include "eagle/core/ConfigEncryption.h"
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
}

ConfigManager::~ConfigManager()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    delete d_ptr;
}

bool ConfigManager::loadFromFile(const QString& filePath, ConfigLevel level)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("ConfigManager", QString("无法打开配置文件: %1").arg(filePath));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    return loadFromJson(data, level);
}

bool ConfigManager::loadFromJson(const QByteArray& json, ConfigLevel level)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError) {
        Logger::error("ConfigManager", QString("JSON解析错误: %1").arg(error.errorString()));
        return false;
    }
    
    if (!doc.isObject()) {
        Logger::error("ConfigManager", "JSON文档不是对象");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QVariantMap config = doc.object().toVariantMap();
    
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
        for (auto it = config.begin(); it != config.end(); ++it) {
            QVariant oldValue = targetConfig->value(it.key());
            (*targetConfig)[it.key()] = it.value();
            if (oldValue != it.value()) {
                emit configChanged(it.key(), oldValue, it.value());
            }
        }
        emit configReloaded();
        return true;
    }
    
    return false;
}

bool ConfigManager::saveToFile(const QString& filePath, ConfigLevel level)
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
    {
        auto* d = d_func();
        QMutexLocker locker(&d->mutex);
        if (d->encryptionEnabled && !d->sensitiveKeys.isEmpty()) {
            configToSave = ConfigEncryption::encryptConfig(configToSave, d->sensitiveKeys, d->encryptionKey);
        }
    }
    
    QJsonObject jsonObj = QJsonObject::fromVariantMap(configToSave);
    QJsonDocument doc(jsonObj);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::error("ConfigManager", QString("无法写入配置文件: %1").arg(filePath));
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    Logger::info("ConfigManager", QString("配置保存成功: %1").arg(filePath));
    return true;
}

bool ConfigManager::validateConfig(const QVariantMap& config, const QString& schemaPath) const
{
    // 简化实现，实际应该使用JSON Schema验证
    Q_UNUSED(config)
    Q_UNUSED(schemaPath)
    Logger::warning("ConfigManager", "配置验证功能未完全实现");
    return true;
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
