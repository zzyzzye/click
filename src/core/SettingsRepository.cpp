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

void SettingsRepository::saveProfile(const ClickProfile& profile) {
  settings_.beginGroup(profileKey(profile.name));
  const QVariantMap data = ClickTypes::toVariantMap(profile);
  for (auto it = data.cbegin(); it != data.cend(); ++it) {
    settings_.setValue(it.key(), it.value());
  }
  settings_.endGroup();
  settings_.sync();
}

std::optional<ClickProfile> SettingsRepository::loadProfile(const QString& name) const {
  const QVariantMap data = readProfileMap(name);
  if (data.isEmpty()) {
    return std::nullopt;
  }
  return ClickTypes::fromVariantMap(data);
}

bool SettingsRepository::renameProfile(const QString& oldName, const QString& newName) {
  if (oldName == newName || oldName.isEmpty() || newName.isEmpty() || !hasProfile(oldName) ||
      hasProfile(newName)) {
    return false;
  }

  auto profile = loadProfile(oldName);
  if (!profile.has_value()) {
    return false;
  }

  profile->name = newName;
  saveProfile(*profile);
  deleteProfile(oldName);
  return true;
}

bool SettingsRepository::deleteProfile(const QString& name) {
  if (!hasProfile(name)) {
    return false;
  }
  settings_.remove(profileKey(name));
  settings_.sync();
  return true;
}

void SettingsRepository::saveLastUsedProfile(const ClickProfile& profile) {
  settings_.beginGroup("lastUsed");
  const QVariantMap data = ClickTypes::toVariantMap(profile);
  for (auto it = data.cbegin(); it != data.cend(); ++it) {
    settings_.setValue(it.key(), it.value());
  }
  settings_.endGroup();
  settings_.sync();
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

