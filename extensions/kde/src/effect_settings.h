// SPDX-License-Identifier: MIT
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVariant>

namespace SmoothScrollKWin
{
struct WindowIdentity
{
  QString desktopFileName, resourceClass, resourceName, windowClass;
  bool matches(const QString& app, const QString& ruleClass) const;
  int matchingRuleIndex(const QJsonArray& rules) const;
  QVariantMap toMap() const;
  static WindowIdentity fromPick(const QVariantMap& info);
};

struct Field
{
  QString group, key, label;
  QVariant initial;
  double minimum = 0, maximum = 0;
};
const QList<Field>& settingsFields();
QJsonObject defaultSettings();
QJsonObject readSettings();
bool writeSettings(const QJsonObject& settings, const QJsonObject& baseline, QString* error);
bool sameWindowId(const QString& left, const QString& right);
QJsonObject applySettingsEdits(QJsonObject latest, const QJsonObject& baseline, const QJsonObject& draft);
QRegularExpression titleExpression(const QJsonObject& title);
}  // namespace SmoothScrollKWin
