#include "core/FlowRepository.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJsonDocument>

FlowRepository::FlowRepository(QString root) : root_(std::move(root)) {
  if (root_.isEmpty())
    root_ =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("flows");
}
bool FlowRepository::save(const FlowDocument& d, QString* error) const {
  if (!FlowTypes::validate(d, error))
    return false;
  if (!QDir().mkpath(root_)) {
    if (error)
      *error = "无法创建流程目录，请检查写入权限。";
    return false;
  }
  QSaveFile file(QDir(root_).filePath(d.id + ".json"));
  const auto data = QJsonDocument(FlowTypes::toJson(d)).toJson();
  if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
    if (error)
      *error = "流程保存失败，请检查磁盘空间和写入权限；编辑内容仍保留。";
    return false;
  }
  return true;
}
QVector<FlowDocument> FlowRepository::loadAll(QStringList* warnings) const {
  QVector<FlowDocument> result;
  QDir dir(root_);
  for (const auto& name : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
    QFile f(dir.filePath(name));
    QString error;
    if (!f.open(QIODevice::ReadOnly) || f.size() > 64 * 1024 * 1024) {
      if (warnings)
        warnings->append(name + "：文件不可读或过大。");
      continue;
    }
    auto doc = FlowTypes::fromJson(QJsonDocument::fromJson(f.readAll()).object(), &error);
    if (doc)
      result.append(*doc);
    else if (warnings)
      warnings->append(name + "：" + error);
  }
  return result;
}
