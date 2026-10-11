#include "core/SettingsRepository.h"

#include "core/ClickTypes.h"

SettingsRepository::SettingsRepository(const QString& organization,
                                       const QString& application)
    : settings_(organization, application) {}

QStringList SettingsRepository::profileNames() const {
  settings_.beginGroup("profiles");
  const QStringList groups = settings_.childGroups();
  settings_.endGroup();
  return groups;
}

bool SettingsRepository::hasProfile(const QString& name) const {
  return !readProfileMap(name).isEmpty();
}

bool SettingsRepository::validProfileName(const QString& name) {
  return !name.trimmed().isEmpty() && name == name.trimmed() && !name.contains('/') &&
         !name.contains('\\');
}

bool SettingsRepository::saveProfile(const ClickProfile& profile) {
  if (!validProfileName(profile.name))
    return false;
  settings_.beginGroup(profileKey(profile.name));
  const QVariantMap data = ClickTypes::toVariantMap(profile);
  for (auto it = data.cbegin(); it != data.cend(); ++it) {
    settings_.setValue(it.key(), it.value());
  }
  settings_.endGroup();
  settings_.sync();
  return settings_.status() == QSettings::NoError;
}

std::optional<ClickProfile> SettingsRepository::loadProfile(const QString& name) const {
  const QVariantMap data = readProfileMap(name);
  if (data.isEmpty()) {
    return std::nullopt;
  }
  return ClickTypes::fromVariantMap(data);
}

bool SettingsRepository::renameProfile(const QString& oldName, const QString& newName) {
  if (oldName == newName || !validProfileName(newName) || !hasProfile(oldName) ||
      hasProfile(newName)) {
    return false;
  }

  auto profile = loadProfile(oldName);
  if (!profile.has_value()) {
    return false;
  }

  profile->name = newName;
  // 新名称写入成功后才删除原预设，写入失败时保留原数据。
  return saveProfile(*profile) && deleteProfile(oldName);
}

bool SettingsRepository::deleteProfile(const QString& name) {
  if (!hasProfile(name)) {
    return false;
  }
  settings_.remove(profileKey(name));
  settings_.sync();
  return settings_.status() == QSettings::NoError;
}

bool SettingsRepository::saveLastUsedProfile(const ClickProfile& profile) {
  settings_.beginGroup("lastUsed");
  const QVariantMap data = ClickTypes::toVariantMap(profile);
  for (auto it = data.cbegin(); it != data.cend(); ++it) {
    settings_.setValue(it.key(), it.value());
  }
  settings_.endGroup();
  settings_.sync();
  return settings_.status() == QSettings::NoError;
}

std::optional<ClickProfile> SettingsRepository::loadLastUsedProfile() const {
  settings_.beginGroup("lastUsed");
  const QStringList keys = settings_.childKeys();
  QVariantMap data;
  for (const QString& key : keys) {
    data.insert(key, settings_.value(key));
  }
  settings_.endGroup();
  if (data.isEmpty()) {
    return std::nullopt;
  }
  return ClickTypes::fromVariantMap(data);
}

bool SettingsRepository::macroSafetyAcknowledged() const {
  return settings_.value("macro/safetyAcknowledged", false).toBool();
}

void SettingsRepository::setMacroSafetyAcknowledged(bool acknowledged) {
  settings_.setValue("macro/safetyAcknowledged", acknowledged);
  settings_.sync();
}

QVariantMap SettingsRepository::readProfileMap(const QString& name) const {
  if (!validProfileName(name))
    return {};
  settings_.beginGroup(profileKey(name));
  const QStringList keys = settings_.childKeys();
  QVariantMap data;
  for (const QString& key : keys) {
    data.insert(key, settings_.value(key));
  }
  settings_.endGroup();
  return data;
}

QString SettingsRepository::profileKey(const QString& name) const {
  return QString("profiles/%1").arg(name);
}

