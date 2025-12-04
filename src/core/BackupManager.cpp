#include "eagle/core/BackupManager.h"
#include "BackupManager_p.h"
#include "eagle/core/ConfigManager.h"
#include "eagle/core/Logger.h"
#include <algorithm>
#include <QtCore/QMutexLocker>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDateTime>
#include <QtCore/QUuid>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>

namespace Eagle {
namespace Core {

BackupManager::BackupManager(ConfigManager* configManager, QObject* parent)
    : QObject(parent)
    , d(new BackupManager::Private(configManager))
{
    if (!configManager) {
        Logger::error("BackupManager", "ConfigManager不能为空");
        return;
    }
    
    // 设置默认备份目录
    QString defaultBackupDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/backups";
    d->policy.backupDir = defaultBackupDir;
    
    // 确保备份目录存在
    QDir dir;
    if (!dir.exists(d->policy.backupDir)) {
        dir.mkpath(d->policy.backupDir);
    }
    
    // 连接配置变更信号
    connect(configManager, &ConfigManager::configChanged,
            this, &BackupManager::onConfigChanged, Qt::QueuedConnection);
    
    // 连接定时器
    connect(d->scheduleTimer, &QTimer::timeout,
            this, &BackupManager::onScheduledBackup, Qt::QueuedConnection);
    
    // 加载已有备份信息（延迟加载，因为此时policy可能还未设置）
    QTimer::singleShot(100, this, [this]() {
        auto* d = d_func();
        QMutexLocker locker(&d->mutex);
        
        QDir backupDir(d->policy.backupDir);
        if (!backupDir.exists()) {
            return;
        }
        
        QStringList filters;
        filters << QString("%1_*.info").arg(d->policy.backupPrefix);
        QFileInfoList infoFiles = backupDir.entryInfoList(filters, QDir::Files);
        
        for (const QFileInfo& fileInfo : infoFiles) {
            QString fileName = fileInfo.baseName();
            QString backupId = fileName.mid(d->policy.backupPrefix.length() + 1);
            locker.unlock();
            BackupInfo info = loadBackupInfo(backupId);
            locker.relock();
            if (info.isValid() && QFile::exists(info.filePath)) {
                d->backups[backupId] = info;
            }
        }
    });
    
    Logger::info("BackupManager", "备份管理器初始化完成");
}

BackupManager::~BackupManager()
{
    delete d;
}

bool BackupManager::setPolicy(const BackupPolicy& policy)
{
    if (!policy.isValid()) {
        Logger::error("BackupManager", "无效的备份策略");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->policy = policy;
    
    // 确保备份目录存在
    QDir dir;
    if (!dir.exists(d->policy.backupDir)) {
        dir.mkpath(d->policy.backupDir);
    }
    
    // 更新定时器
    if (d->autoBackupEnabled && d->policy.enabled) {
        d->scheduleTimer->setInterval(d->policy.scheduleIntervalMinutes * 60 * 1000);
        if (!d->scheduleTimer->isActive()) {
            d->scheduleTimer->start();
        }
    } else {
        d->scheduleTimer->stop();
    }
    
    // 清理旧备份
    cleanupOldBackups();
    
    Logger::info("BackupManager", QString("备份策略已更新: 目录=%1, 最大备份数=%2")
        .arg(d->policy.backupDir).arg(d->policy.maxBackups));
    
    return true;
}

BackupPolicy BackupManager::getPolicy() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->policy;
}

QString BackupManager::createBackup(BackupType type, const QString& name, const QString& description)
{
    auto* d = d_func();
    
    if (!d->configManager) {
        Logger::error("BackupManager", "ConfigManager未初始化");
        return QString();
    }
    
    // 生成备份ID
    QString backupId = generateBackupId();
    
    // 创建备份信息
    BackupInfo info;
    info.id = backupId;
    info.name = name.isEmpty() ? QString("备份_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")) : name;
    info.type = type;
    info.trigger = BackupTrigger::Manual;
    info.createTime = QDateTime::currentDateTime();
    info.description = description;
    info.filePath = generateBackupFilePath(backupId);
    
    // 获取当前配置
    QVariantMap currentConfig = d->configManager->getAll();
    
    // 如果是增量备份，计算差异
    if (type == BackupType::Incremental && !d->lastBackupSnapshot.isEmpty()) {
        QVariantMap diff;
        for (auto it = currentConfig.begin(); it != currentConfig.end(); ++it) {
            if (!d->lastBackupSnapshot.contains(it.key()) || 
                d->lastBackupSnapshot[it.key()] != it.value()) {
                diff[it.key()] = it.value();
            }
        }
        currentConfig = diff;
        info.metadata["baseBackupId"] = d->backups.isEmpty() ? QString() : d->backups.last().id;
    }
    
    // 保存备份文件
    QFile file(info.filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error("BackupManager", QString("无法创建备份文件: %1").arg(info.filePath));
        emit backupFailed(backupId, "无法创建备份文件");
        return QString();
    }
    
    QJsonDocument doc(QJsonObject::fromVariantMap(currentConfig));
    file.write(doc.toJson());
    file.close();
    
    // 获取文件大小
    QFileInfo fileInfo(info.filePath);
    info.size = fileInfo.size();
    
    // 保存备份信息
    {
        QMutexLocker locker(&d->mutex);
        d->backups[backupId] = info;
        d->lastBackupSnapshot = d->configManager->getAll();
        d->lastBackupTime = QDateTime::currentDateTime();
        saveBackupInfo(info);
    }
    
    Logger::info("BackupManager", QString("备份已创建: %1 (%2)").arg(info.name).arg(backupId));
    emit backupCreated(backupId, info);
    
    // 清理旧备份
    cleanupOldBackups();
    
    return backupId;
}

bool BackupManager::deleteBackup(const QString& backupId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->backups.contains(backupId)) {
        Logger::warning("BackupManager", QString("备份不存在: %1").arg(backupId));
        return false;
    }
    
    BackupInfo info = d->backups[backupId];
    
    // 删除备份文件
    if (QFile::exists(info.filePath)) {
        if (!QFile::remove(info.filePath)) {
            Logger::error("BackupManager", QString("无法删除备份文件: %1").arg(info.filePath));
            return false;
        }
    }
    
    // 删除备份信息文件
    QString infoFilePath = info.filePath + ".info";
    if (QFile::exists(infoFilePath)) {
        QFile::remove(infoFilePath);
    }
    
    d->backups.remove(backupId);
    
    Logger::info("BackupManager", QString("备份已删除: %1").arg(backupId));
    emit backupDeleted(backupId);
    
    return true;
}

bool BackupManager::deleteOldBackups(int keepCount)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (keepCount < 0) {
        keepCount = d->policy.maxBackups;
    }
    
    if (d->backups.size() <= keepCount) {
        return true;
    }
    
    // 按创建时间排序
    QList<BackupInfo> sortedBackups = d->backups.values();
    std::sort(sortedBackups.begin(), sortedBackups.end(),
              [](const BackupInfo& a, const BackupInfo& b) {
                  return a.createTime < b.createTime;
              });
    
    // 删除最旧的备份
    int toDelete = sortedBackups.size() - keepCount;
    for (int i = 0; i < toDelete; ++i) {
        deleteBackup(sortedBackups[i].id);
    }
    
    return true;
}

QList<BackupInfo> BackupManager::listBackups() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->backups.values();
}

BackupInfo BackupManager::getBackupInfo(const QString& backupId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->backups.value(backupId);
}

bool BackupManager::backupExists(const QString& backupId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->backups.contains(backupId);
}

bool BackupManager::restoreBackup(const QString& backupId, bool verifyBeforeRestore)
{
    if (verifyBeforeRestore && !verifyBackup(backupId)) {
        Logger::error("BackupManager", QString("备份验证失败: %1").arg(backupId));
        emit backupFailed(backupId, "备份验证失败");
        return false;
    }
    
    auto* d = d_func();
    
    BackupInfo info;
    {
        QMutexLocker locker(&d->mutex);
        if (!d->backups.contains(backupId)) {
            Logger::error("BackupManager", QString("备份不存在: %1").arg(backupId));
            return false;
        }
        info = d->backups[backupId];
    }
    
    return restoreFromFile(info.filePath, verifyBeforeRestore);
}

bool BackupManager::restoreFromFile(const QString& filePath, bool verifyBeforeRestore)
{
    if (verifyBeforeRestore && !verifyBackupFile(filePath)) {
        Logger::error("BackupManager", QString("备份文件验证失败: %1").arg(filePath));
        return false;
    }
    
    auto* d = d_func();
    
    if (!d->configManager) {
        Logger::error("BackupManager", "ConfigManager未初始化");
        return false;
    }
    
    // 读取备份文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::error("BackupManager", QString("无法打开备份文件: %1").arg(filePath));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        Logger::error("BackupManager", QString("备份文件格式错误: %1").arg(error.errorString()));
        return false;
    }
    
    QVariantMap config = doc.object().toVariantMap();
    
    // 恢复配置
    if (!d->configManager->updateConfig(config, ConfigManager::Global)) {
        Logger::error("BackupManager", "配置恢复失败");
        emit backupFailed(QString(), "配置恢复失败");
        return false;
    }
    
    Logger::info("BackupManager", QString("配置已从备份恢复: %1").arg(filePath));
    emit backupRestored(QString(), true);
    
    return true;
}

bool BackupManager::verifyBackup(const QString& backupId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->backups.contains(backupId)) {
        return false;
    }
    
    BackupInfo info = d->backups[backupId];
    return verifyBackupFile(info.filePath);
}

bool BackupManager::verifyBackupFile(const QString& filePath) const
{
    if (!QFile::exists(filePath)) {
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        return false;
    }
    
    return doc.isObject();
}

void BackupManager::setAutoBackupEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->autoBackupEnabled = enabled;
    
    if (enabled && d->policy.enabled) {
        d->scheduleTimer->setInterval(d->policy.scheduleIntervalMinutes * 60 * 1000);
        d->scheduleTimer->start();
        Logger::info("BackupManager", "自动备份已启用");
    } else {
        d->scheduleTimer->stop();
        Logger::info("BackupManager", "自动备份已禁用");
    }
}

bool BackupManager::isAutoBackupEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->autoBackupEnabled && d->policy.enabled;
}

QString BackupManager::triggerScheduledBackup()
{
    auto* d = d_func();
    BackupType type = d->policy.defaultType;
    return createBackup(type, QString(), "定时备份");
}

void BackupManager::onConfigChanged(const QString& key)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->policy.backupOnChange && d->autoBackupEnabled && d->policy.enabled) {
        // 延迟备份，避免频繁配置变更导致过多备份
        QTimer::singleShot(5000, this, [this]() {
            createBackup(BackupType::Incremental, QString(), "配置变更触发");
        });
    }
}

void BackupManager::onScheduledBackup()
{
    triggerScheduledBackup();
}

QString BackupManager::generateBackupId() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return QString("%1_%2").arg(timestamp).arg(uuid);
}

QString BackupManager::generateBackupFilePath(const QString& backupId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    QString fileName = QString("%1_%2.json").arg(d->policy.backupPrefix).arg(backupId);
    return QDir(d->policy.backupDir).filePath(fileName);
}

bool BackupManager::saveBackupInfo(const BackupInfo& info) const
{
    QString infoFilePath = info.filePath + ".info";
    QFile file(infoFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QJsonObject obj;
    obj["id"] = info.id;
    obj["name"] = info.name;
    obj["type"] = static_cast<int>(info.type);
    obj["trigger"] = static_cast<int>(info.trigger);
    obj["createTime"] = info.createTime.toString(Qt::ISODate);
    obj["description"] = info.description;
    obj["size"] = info.size;
    obj["filePath"] = info.filePath;
    obj["metadata"] = QJsonObject::fromVariantMap(info.metadata);
    
    QJsonDocument doc(obj);
    file.write(doc.toJson());
    file.close();
    
    return true;
}

BackupInfo BackupManager::loadBackupInfo(const QString& backupId) const
{
    const auto* d = d_func();
    QString filePath = generateBackupFilePath(backupId);
    QString infoFilePath = filePath + ".info";
    
    if (!QFile::exists(infoFilePath)) {
        return BackupInfo();
    }
    
    QFile file(infoFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return BackupInfo();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        return BackupInfo();
    }
    
    QJsonObject obj = doc.object();
    BackupInfo info;
    info.id = obj["id"].toString();
    info.name = obj["name"].toString();
    info.type = static_cast<BackupType>(obj["type"].toInt());
    info.trigger = static_cast<BackupTrigger>(obj["trigger"].toInt());
    info.createTime = QDateTime::fromString(obj["createTime"].toString(), Qt::ISODate);
    info.description = obj["description"].toString();
    info.size = obj["size"].toVariant().toLongLong();
    info.filePath = obj["filePath"].toString();
    info.metadata = obj["metadata"].toObject().toVariantMap();
    
    return info;
}


bool BackupManager::cleanupOldBackups()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->backups.size() <= d->policy.maxBackups) {
        return true;
    }
    
    return deleteOldBackups(d->policy.maxBackups);
}

} // namespace Core
} // namespace Eagle
