// SPDX-License-Identifier: MIT
#include "effect_settings.h"
#include <KConfig>
#include <KConfigGroup>
#include <QLockFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>

namespace SmoothScrollKWin
{
const QList<Field>& settingsFields()
{
  static const QList<Field> fields = {
    { "dot", "enabled", "Enabled", true },
    { "dot", "offset_x", "Offset x (px)", 12, -10000, 10000 },
    { "dot", "offset_y", "Offset y (px)", 0, -10000, 10000 },
    { "dot", "size", "Size (px)", 8, 1, 512 },
    { "dot", "color", "Color", QStringLiteral("#4ea1ff") },
    { "dot", "min_alpha", "Minimum opacity", 0.16, 0.0, 1.0 },
    { "dot", "min_alpha_speed", "Minimum opacity speed", 100, 0, 65535 },
    { "dot", "max_alpha", "Maximum opacity", 0.9, 0.0, 1.0 },
    { "dot", "max_alpha_speed", "Maximum opacity speed", 3200, 0, 65535 },
    { "free_spin", "enabled", "Enabled", true },
    { "free_spin", "color", "Color", QStringLiteral("#4ea1ff") },
    { "free_spin", "alpha", "Opacity", 0.9, 0.0, 1.0 },
    { "free_spin", "ring_gap", "Ring gap (px)", 1, 0, 32 },
    { "free_spin", "ring_width", "Ring width (px)", 2, 1, 16 },
    { "arrow", "enabled", "Enabled", true },
    { "arrow", "offset_x", "Offset x (px)", 12, -10000, 10000 },
    { "arrow", "offset_y", "Offset y (px)", 0, -10000, 10000 },
    { "arrow", "size", "Size (px)", 22, 1, 512 },
    { "arrow", "color", "Color", QStringLiteral("#4ea1ff") },
    { "arrow", "alpha", "Opacity", 0.9, 0.0, 1.0 },
    { "arrow", "padding_scale", "Padding scale", 0.12, 0.0, 1.0 },
    { "arrow", "min_padding", "Min padding", 2, 0, 512 },
    { "arrow", "head_size_scale", "Head size scale", 0.18, 0.0, 1.0 },
    { "arrow", "min_head_size", "Min head size", 3, 0, 512 },
    { "arrow", "line_width_scale", "Line width scale", 0.08, 0.0, 1.0 },
    { "arrow", "min_line_width", "Min line width", 1, 0, 512 },
    { "arrow", "head_width_scale", "Head width scale", 0.5, 0.0, 4.0 },
    { "auto_scroll", "enabled", "Enabled", true },
    { "auto_scroll", "offset_x", "Offset x (px)", 12, -256, 256 },
    { "auto_scroll", "offset_y", "Offset y (px)", 0, -256, 256 },
    { "auto_scroll", "size", "Size (px)", 24, 20, 256 },
    { "auto_scroll", "color", "Color", QStringLiteral("#cbd5e1") },
    { "auto_scroll", "dot_color", "Dot color", QStringLiteral("#4ea1ff") },
    { "auto_scroll", "alpha", "Opacity", 0.95, 0.0, 1.0 },
    { "auto_scroll", "dot_size", "Dot size (px)", 6, 2, 64 },
    { "passthrough", "enabled", "Enabled", true },
    { "passthrough", "offset_x", "Offset x (px)", 12, -10000, 10000 },
    { "passthrough", "offset_y", "Offset y (px)", 0, -10000, 10000 },
    { "passthrough", "size", "Size (px)", 12, 1, 512 },
    { "passthrough", "padding_scale", "Padding scale", 0.22, 0.0, 1.0 },
    { "passthrough", "min_padding", "Min padding", 3, 0, 512 },
    { "passthrough", "line_width_scale", "Line width scale", 0.12, 0.0, 1.0 },
    { "passthrough", "min_line_width", "Min line width", 2, 0, 512 },
    { "passthrough", "color", "Color", QStringLiteral("#ff5c5c") },
    { "passthrough", "alpha", "Opacity", 0.9, 0.0, 1.0 },
    { "scroll", "stop_on_pointer_leave_window", "Stop inertia when leaving a window", true },
    { "scroll", "notify_direct_mode", "Notify when direct mode changes", true },
    { "scroll", "poll_interval_ms", "Polling interval (ms)", 4, 1, 1000 },
  };
  return fields;
}
namespace
{
const QString settingsFile = QStringLiteral("smooth-scroll-kwinrc");
QJsonObject scalarSettings(KConfig* config)
{
  QJsonObject result;
  for (const auto& f : settingsFields())
  {
    auto group = result.value(f.group).toObject();
    const auto value = config ? KConfigGroup(config, f.group).readEntry(f.key, f.initial) : f.initial;
    group.insert(f.key, QJsonValue::fromVariant(value));
    result.insert(f.group, group);
  }
  return result;
}
QJsonObject snapshot(KConfig* config)
{
  auto result = scalarSettings(config);
  for (const auto& key : { "window_type_profiles", "effect_profiles" })
  {
    QJsonObject map;
    if (config)
    {
      KConfigGroup group(config, key);
      for (const auto& name : group.keyList())
        map.insert(name, double(group.readEntry(name, 0u)));
    }
    result.insert(key, map);
  }
  const KConfigGroup profiles = config ? KConfigGroup(config, "Profiles") : KConfigGroup();
  result.insert("default_profile_id", double(config ? profiles.readEntry("Default", 0u) : 0u));
  result.insert("shell_profile_id", double(config ? profiles.readEntry("Shell", 1u) : 1u));
  const auto raw = profiles.isValid() ? profiles.readEntry("Rules", QByteArray("[]")) : QByteArray("[]");
  const auto parsed = QJsonDocument::fromJson(raw);
  // Keep malformed bytes distinguishable from an intentionally empty rule list.
  result.insert("profile_rules", parsed.isArray() ? QJsonValue(parsed.array()) :
                                                   QJsonValue(QString::fromLatin1(raw.toBase64())));
  return result;
}
}  // namespace
QJsonObject defaultSettings()
{
  return snapshot(nullptr);
}

QJsonObject readSettings()
{
  KConfig config(settingsFile, KConfig::SimpleConfig);
  return snapshot(&config);
}

bool writeSettings(const QJsonObject& settings, const QJsonObject& baseline, QString* error)
{
  const auto dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  QDir().mkpath(dir);
  QLockFile lock(dir + "/" + settingsFile + ".edit-lock");
  if (!lock.tryLock())
  {
    *error = QStringLiteral("Another editor is saving. Try again.");
    return false;
  }
  KConfig config(settingsFile, KConfig::SimpleConfig);
  if (snapshot(&config) != baseline)
  {
    *error = QStringLiteral("Settings changed in another window. Reload the settings before saving.");
    return false;
  }
  for (const auto& f : settingsFields())
    KConfigGroup(&config, f.group).writeEntry(f.key, settings.value(f.group).toObject().value(f.key).toVariant());
  KConfigGroup profiles(&config, "Profiles");
  profiles.writeEntry("Default", uint(settings.value("default_profile_id").toInteger()));
  profiles.writeEntry("Shell", uint(settings.value("shell_profile_id").toInteger()));
  if (settings.value("profile_rules") != baseline.value("profile_rules"))
    profiles.writeEntry("Rules", QJsonDocument(settings.value("profile_rules").toArray()).toJson(QJsonDocument::Compact));
  for (const auto& key : { "window_type_profiles", "effect_profiles" })
  {
    KConfigGroup group(&config, key);
    const auto map = settings.value(key).toObject();
    for (const auto& old : group.keyList())
      if (!map.contains(old))
        group.deleteEntry(old);
    for (auto it = map.begin(); it != map.end(); ++it)
      group.writeEntry(it.key(), uint(it.value().toInteger()));
  }
  if (!config.sync())
  {
    *error = QStringLiteral("Could not save the settings.");
    return false;
  }
  return true;
}

bool sameWindowId(const QString& left, const QString& right)
{
  if (left.isEmpty() || right.isEmpty())
    return false;
  const auto suffix = QLatin1String(".desktop");
  return left == right || (!left.endsWith(suffix) && right == left + suffix) ||
         (!right.endsWith(suffix) && left == right + suffix);
}

bool WindowIdentity::matches(const QString& app, const QString& ruleClass) const
{
  if (!app.isEmpty())
    return sameWindowId(app, desktopFileName) || sameWindowId(app, resourceClass) || sameWindowId(app, windowClass);
  return sameWindowId(ruleClass, resourceClass) || sameWindowId(ruleClass, resourceName) ||
         sameWindowId(ruleClass, windowClass);
}

int WindowIdentity::matchingRuleIndex(const QJsonArray& rules) const
{
  int disabled = -1;
  for (int i = 0; i < rules.size(); ++i)
  {
    const auto rule = rules.at(i).toObject();
    if (!matches(rule.value("app").toString(), rule.value("class").toString()))
      continue;
    if (rule.value("enabled").toBool(true))
      return i;
    if (disabled < 0)
      disabled = i;
  }
  return disabled;
}

QVariantMap WindowIdentity::toMap() const
{
  return { { "desktop_file_name", desktopFileName }, { "resource_class", resourceClass },
           { "resource_name", resourceName }, { "window_class", windowClass } };
}

WindowIdentity WindowIdentity::fromPick(const QVariantMap& info)
{
  if (info.contains("desktop_file_name"))
    return { info.value("desktop_file_name").toString(), info.value("resource_class").toString(),
             info.value("resource_name").toString(), info.value("window_class").toString() };

  // A running effect from before the upgrade only supplies its preferred matcher.
  WindowIdentity identity;
  if (info.value("key") == "app")
    identity.desktopFileName = info.value("value").toString();
  else if (info.value("key") == "class")
    identity.resourceClass = info.value("value").toString();
  return identity;
}

QJsonObject applySettingsEdits(QJsonObject latest, const QJsonObject& baseline, const QJsonObject& draft)
{
  for (auto it = baseline.begin(); it != baseline.end(); ++it)
    if (!draft.contains(it.key()))
      latest.remove(it.key());
  for (auto it = draft.begin(); it != draft.end(); ++it)
  {
    const auto old = baseline.value(it.key());
    if (old == it.value())
      continue;
    if (old.isObject() && it.value().isObject())
      latest.insert(it.key(), applySettingsEdits(latest.value(it.key()).toObject(), old.toObject(), it.value().toObject()));
    else
      latest.insert(it.key(), it.value());
  }
  return latest;
}

QRegularExpression titleExpression(const QJsonObject& rule)
{
  auto pattern = rule.value("title").toString();
  const auto mode = rule.value("mode").toString("regex");
  if (mode != "regex")
  {
    pattern = QRegularExpression::escape(pattern);
    if (mode == "equals" || mode == "starts")
      pattern.prepend(QStringLiteral("\\A"));
    if (mode == "equals" || mode == "ends")
      pattern.append(QStringLiteral("\\z"));
  }
  return QRegularExpression(pattern, rule.value("ignore_case").toBool() ? QRegularExpression::CaseInsensitiveOption :
                                                                          QRegularExpression::NoPatternOption);
}
}  // namespace SmoothScrollKWin
