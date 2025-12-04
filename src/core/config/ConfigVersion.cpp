#include "eagle/core/ConfigVersion.h"
#include "ConfigVersion_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QCryptographicHash>
#include <QtCore/QStandardPaths>
#include <QtCore/QSet>
#include <algorithm>

namespace Eagle {
namespace Core {

ConfigVersionManager::ConfigVersionManager(QObject* parent)
    : QObject(parent)
    , d(new ConfigVersionManager::Private)
{
    // 设置默认存储路径
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    defaultPath += "/config_versions";
    d->storagePath = defaultPath;
    
    // 确保目录存在
    QDir dir;
    if (!dir.exists(d->storagePath)) {
        dir.mkpath(d->storagePath);
    }
    
    // 加载已存在的版本
    QList<int> existingVersions = getAllVersionNumbers();
    if (!existingVersions.isEmpty()) {
        std::sort(existingVersions.begin(), existingVersions.end());
        d->currentVersion = existingVersions.last();
        
        // 加载所有版本到内存（可选，按需加载）
        for (int version : existingVersions) {
            ConfigVersion v = loadVersionFromFile(version);
            if (v.isValid()) {
                d->versions[version] = v;
            }
        }
    }
    
    Logger::info("ConfigVersionManager", QString("配置版本管理器初始化完成，当前版本: %1").arg(d->currentVersion));
}

ConfigVersionManager::~ConfigVersionManager()
{
    delete d;
}

QString ConfigVersionManager::calculateConfigHash(const QVariantMap& config) const
{
    QJsonDocument doc = QJsonDocument::fromVariant(config);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(json);
    return hash.result().toHex();
}

int ConfigVersionManager::createVersion(const QVariantMap& config, const QString& author, 
                                       const QString& description, int version)
{
    if (!isEnabled()) {
        return 0;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 确定版本号
    if (version <= 0) {
        version = d->currentVersion + 1;
    } else {
        // 如果指定了版本号，检查是否已存在
        if (d->versions.contains(version)) {
            Logger::warning("ConfigVersionManager", QString("版本 %1 已存在").arg(version));
            return 0;
        }
    }
    
    // 计算配置哈希
    QString configHash = calculateConfigHash(config);
    
    // 检查是否与当前版本相同（避免重复版本）
    if (d->currentVersion > 0) {
        ConfigVersion current = d->versions.value(d->currentVersion);
        if (current.configHash == configHash) {
            Logger::info("ConfigVersionManager", "配置未变更，跳过版本创建");
            return d->currentVersion;
        }
    }
    
    // 创建版本对象
    ConfigVersion versionObj;
    versionObj.version = version;
    versionObj.timestamp = QDateTime::currentDateTime();
    versionObj.author = author.isEmpty() ? "system" : author;
    versionObj.description = description;
    versionObj.config = config;
    versionObj.configHash = configHash;
    
    // 保存到内存和文件
    d->versions[version] = versionObj;
    d->currentVersion = version;
    
    locker.unlock();
    saveVersionToFile(versionObj);
    
    Logger::info("ConfigVersionManager", QString("创建配置版本: %1 (作者: %2)")
        .arg(version).arg(versionObj.author));
    
    emit versionCreated(version, versionObj.author);
    return version;
}

int ConfigVersionManager::currentVersion() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->currentVersion;
}

QList<ConfigVersion> ConfigVersionManager::getVersions(int limit) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<ConfigVersion> result;
    QList<int> versionNumbers = d->versions.keys();
    std::sort(versionNumbers.begin(), versionNumbers.end(), std::greater<int>());
    
    int count = 0;
    for (int version : versionNumbers) {
        ConfigVersion v = d->versions.value(version);
        if (!v.isValid()) {
            // 如果内存中没有，尝试从文件加载
            v = loadVersionFromFile(version);
        }
        if (v.isValid()) {
            result.append(v);
            count++;
            if (limit > 0 && count >= limit) {
                break;
            }
        }
    }
    
    return result;
}

ConfigVersion ConfigVersionManager::getVersion(int version) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->versions.contains(version)) {
        return d->versions.value(version);
    }
    
    // 尝试从文件加载
    locker.unlock();
    return loadVersionFromFile(version);
}

QVariantMap ConfigVersionManager::rollbackToVersion(int version)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->versions.contains(version)) {
        // 尝试从文件加载
        ConfigVersion v = loadVersionFromFile(version);
        if (!v.isValid()) {
            Logger::error("ConfigVersionManager", QString("版本 %1 不存在").arg(version));
            return QVariantMap();
        }
        d->versions[version] = v;
    }
    
    ConfigVersion targetVersion = d->versions.value(version);
    int oldVersion = d->currentVersion;
    
    // 创建回滚版本（作为新版本）
    ConfigVersion rollbackVersion;
    rollbackVersion.version = d->currentVersion + 1;
    rollbackVersion.timestamp = QDateTime::currentDateTime();
    rollbackVersion.author = "system";
    rollbackVersion.description = QString("回滚到版本 %1").arg(version);
    rollbackVersion.config = targetVersion.config;
    rollbackVersion.configHash = targetVersion.configHash;
    
    d->versions[rollbackVersion.version] = rollbackVersion;
    d->currentVersion = rollbackVersion.version;
    
    locker.unlock();
    saveVersionToFile(rollbackVersion);
    
    Logger::info("ConfigVersionManager", QString("回滚配置: %1 -> %2 (目标版本: %3)")
        .arg(oldVersion).arg(rollbackVersion.version).arg(version));
    
    emit versionRolledBack(oldVersion, rollbackVersion.version);
    return targetVersion.config;
}

QList<ConfigDiff> ConfigVersionManager::compareVersions(int version1, int version2) const
{
    ConfigVersion v1 = getVersion(version1);
    ConfigVersion v2 = getVersion(version2);
    
    if (!v1.isValid() || !v2.isValid()) {
        Logger::error("ConfigVersionManager", "无法对比版本：版本不存在");
        return QList<ConfigDiff>();
    }
    
    return compareWithVersion(version1, v2.config);
}

QList<ConfigDiff> ConfigVersionManager::compareWithVersion(int version, const QVariantMap& currentConfig) const
{
    ConfigVersion v = getVersion(version);
    if (!v.isValid()) {
        Logger::error("ConfigVersionManager", QString("版本 %1 不存在").arg(version));
        return QList<ConfigDiff>();
    }
    
    QList<ConfigDiff> diffs;
    QSet<QString> allKeys;
    
    // 收集所有键
    for (auto it = v.config.begin(); it != v.config.end(); ++it) {
        allKeys.insert(it.key());
    }
    for (auto it = currentConfig.begin(); it != currentConfig.end(); ++it) {
        allKeys.insert(it.key());
    }
    
    // 对比每个键
    for (const QString& key : allKeys) {
        ConfigDiff diff;
        diff.key = key;
        
        bool inOld = v.config.contains(key);
        bool inNew = currentConfig.contains(key);
        QVariant oldValue = v.config.value(key);
        QVariant newValue = currentConfig.value(key);
        
        if (!inOld && inNew) {
            // 新增
            diff.changeType = "added";
            diff.newValue = newValue;
            diffs.append(diff);
        } else if (inOld && !inNew) {
            // 删除
            diff.changeType = "removed";
            diff.oldValue = oldValue;
            diffs.append(diff);
        } else if (inOld && inNew) {
            // 修改
            if (oldValue != newValue) {
                diff.changeType = "modified";
                diff.oldValue = oldValue;
                diff.newValue = newValue;
                diffs.append(diff);
            }
        }
    }
    
    return diffs;
}

bool ConfigVersionManager::deleteVersion(int version)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->versions.contains(version)) {
        // 尝试从文件加载
        ConfigVersion v = loadVersionFromFile(version);
        if (!v.isValid()) {
            Logger::warning("ConfigVersionManager", QString("版本 %1 不存在").arg(version));
            return false;
        }
    }
    
    // 不能删除当前版本
    if (version == d->currentVersion) {
        Logger::warning("ConfigVersionManager", "不能删除当前版本");
        return false;
    }
    
    d->versions.remove(version);
    
    // 删除文件
    QString filePath = QString("%1/version_%2.json").arg(d->storagePath).arg(version);
    QFile file(filePath);
    if (file.exists()) {
        file.remove();
    }
    
    Logger::info("ConfigVersionManager", QString("删除配置版本: %1").arg(version));
    
    locker.unlock();
    emit versionDeleted(version);
    return true;
}

int ConfigVersionManager::cleanupOldVersions(int keepCount)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<int> versionNumbers = d->versions.keys();
    std::sort(versionNumbers.begin(), versionNumbers.end(), std::greater<int>());
    
    int deletedCount = 0;
    for (int i = keepCount; i < versionNumbers.size(); ++i) {
        int version = versionNumbers[i];
        if (version != d->currentVersion) {
            locker.unlock();
            if (deleteVersion(version)) {
                deletedCount++;
            }
            locker.relock();
        }
    }
    
    Logger::info("ConfigVersionManager", QString("清理旧版本: 删除 %1 个版本").arg(deletedCount));
    return deletedCount;
}

void ConfigVersionManager::setStoragePath(const QString& path)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->storagePath = path;
    
    // 确保目录存在
    QDir dir;
    if (!dir.exists(d->storagePath)) {
        dir.mkpath(d->storagePath);
    }
    
    Logger::info("ConfigVersionManager", QString("设置版本存储路径: %1").arg(path));
}

QString ConfigVersionManager::storagePath() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->storagePath;
}

void ConfigVersionManager::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    Logger::info("ConfigVersionManager", QString("版本管理%1").arg(enabled ? "启用" : "禁用"));
}

bool ConfigVersionManager::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void ConfigVersionManager::saveVersionToFile(const ConfigVersion& version) const
{
    const auto* d = d_func();
    QString filePath = QString("%1/version_%2.json").arg(d->storagePath).arg(version.version);
    
    QJsonObject obj;
    obj["version"] = version.version;
    obj["timestamp"] = version.timestamp.toString(Qt::ISODate);
    obj["author"] = version.author;
    obj["description"] = version.description;
    obj["config"] = QJsonObject::fromVariantMap(version.config);
    obj["configHash"] = version.configHash;
    
    QJsonDocument doc(obj);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    } else {
        Logger::error("ConfigVersionManager", QString("无法保存版本文件: %1").arg(filePath));
    }
}

ConfigVersion ConfigVersionManager::loadVersionFromFile(int version) const
{
    const auto* d = d_func();
    QString filePath = QString("%1/version_%2.json").arg(d->storagePath).arg(version);
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return ConfigVersion();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        Logger::error("ConfigVersionManager", QString("JSON解析错误: %1").arg(error.errorString()));
        return ConfigVersion();
    }
    
    QJsonObject obj = doc.object();
    ConfigVersion versionObj;
    versionObj.version = obj["version"].toInt();
    versionObj.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
    versionObj.author = obj["author"].toString();
    versionObj.description = obj["description"].toString();
    versionObj.config = obj["config"].toObject().toVariantMap();
    versionObj.configHash = obj["configHash"].toString();
    
    return versionObj;
}

QList<int> ConfigVersionManager::getAllVersionNumbers() const
{
    const auto* d = d_func();
    QDir dir(d->storagePath);
    QStringList filters;
    filters << "version_*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    QList<int> versions;
    for (const QFileInfo& fileInfo : files) {
        QString fileName = fileInfo.baseName();
        QString versionStr = fileName.mid(8); // "version_".length() = 8
        bool ok;
        int version = versionStr.toInt(&ok);
        if (ok) {
            versions.append(version);
        }
    }
    
    return versions;
}

} // namespace Core
} // namespace Eagle
