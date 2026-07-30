#pragma once

#include <QSettings>
#include <QStringList>
#include <optional>

#include "core/ClickTypes.h"

class SettingsRepository {
 public:
  explicit SettingsRepository(const QString& organization = "OpenAI",
                              const QString& application = "QtClicker");

  QStringList profileNames() const;
  bool hasProfile(const QString& name) const;
  void saveProfile(const ClickProfile& profile);
  std::optional<ClickProfile> loadProfile(const QString& name) const;
  bool renameProfile(const QString& oldName, const QString& newName);
  bool deleteProfile(const QString& name);
  void saveLastUsedProfile(const ClickProfile& profile);
  std::optional<ClickProfile> loadLastUsedProfile() const;
  bool macroSafetyAcknowledged() const;
  void setMacroSafetyAcknowledged(bool acknowledged);

 private:
  QVariantMap readProfileMap(const QString& name) const;
  QString profileKey(const QString& name) const;

  mutable QSettings settings_;
};

