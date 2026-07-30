#include "core/MacroRepository.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {

void setError(QString* error, const QString& message) {
  if (error) *error = message;
}

bool safeId(const QString& id) {
  return !id.trimmed().isEmpty() && !id.contains('/') && !id.contains('\\');
}

}  // namespace

MacroRepository::MacroRepository(QString rootPath)
    : rootPath_(std::move(rootPath)) {
  if (rootPath_.isEmpty()) {
    rootPath_ = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                    .filePath("macros");
  }
}

bool MacroRepository::save(const MacroSequence& sequence, QString* error) const {
  if (!MacroTypes::validate(sequence, error)) return false;
  if (!QDir().mkpath(rootPath_)) {
    setError(error, "无法创建宏保存目录。");
    return false;
  }

  QSaveFile file(filePath(sequence.id));
  if (!file.open(QIODevice::WriteOnly)) {
    setError(error, QString("无法写入宏文件：%1").arg(file.errorString()));
    return false;
  }
  const QByteArray data = QJsonDocument(MacroTypes::toJson(sequence)).toJson(
      QJsonDocument::Indented);
  if (file.write(data) != data.size() || !file.commit()) {
    setError(error, QString("无法保存宏文件：%1").arg(file.errorString()));
    return false;
  }
  return true;
}

QVector<MacroSequence> MacroRepository::loadAll(QStringList* warnings) const {
  QVector<MacroSequence> sequences;
  QDir directory(rootPath_);
  if (!directory.exists()) return sequences;

  for (const QString& fileName : directory.entryList({"*.json"}, QDir::Files,
                                                      QDir::Name)) {
    QFile file(directory.filePath(fileName));
    if (!file.open(QIODevice::ReadOnly)) {
      if (warnings) warnings->append(QString("%1：无法读取。").arg(fileName));
      continue;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
      if (warnings) warnings->append(QString("%1：文件格式损坏。").arg(fileName));
      continue;
    }
    QString error;
    auto sequence = MacroTypes::fromJson(document.object(), &error);
    if (!sequence) {
      if (warnings) warnings->append(QString("%1：%2").arg(fileName, error));
      continue;
    }
    sequences.append(std::move(*sequence));
  }

  std::sort(sequences.begin(), sequences.end(),
            [](const MacroSequence& lhs, const MacroSequence& rhs) {
              return lhs.modifiedAt > rhs.modifiedAt;
            });
  return sequences;
}

bool MacroRepository::rename(const QString& id, const QString& newName,
                             QString* error) const {
  if (!safeId(id) || newName.trimmed().isEmpty()) {
    setError(error, "宏名称不能为空。");
    return false;
  }

  QFile file(filePath(id));
  if (!file.open(QIODevice::ReadOnly)) {
    setError(error, "找不到要重命名的宏。");
    return false;
  }
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();
  QString parseMessage;
  auto sequence = parseError.error == QJsonParseError::NoError && document.isObject()
                      ? MacroTypes::fromJson(document.object(), &parseMessage)
                      : std::nullopt;
  if (!sequence) {
    setError(error, parseMessage.isEmpty() ? "宏文件格式损坏。" : parseMessage);
    return false;
  }
  sequence->name = newName.trimmed();
  sequence->modifiedAt = QDateTime::currentDateTimeUtc();
  return save(*sequence, error);
}

bool MacroRepository::remove(const QString& id, QString* error) const {
  if (!safeId(id)) {
    setError(error, "宏标识无效。");
    return false;
  }
  QFile file(filePath(id));
  if (!file.exists()) {
    setError(error, "找不到要删除的宏。");
    return false;
  }
  if (!file.remove()) {
    setError(error, QString("无法删除宏：%1").arg(file.errorString()));
    return false;
  }
  return true;
}

QString MacroRepository::rootPath() const {
  return rootPath_;
}

QString MacroRepository::filePath(const QString& id) const {
  return QDir(rootPath_).filePath(id + ".json");
}
