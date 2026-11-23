// SPDX-License-Identifier: MIT
#include "effect_settings.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <KConfig>
#include <KConfigGroup>
using namespace SmoothScrollKWin;
int main(int argc, char** argv)
{
  QTemporaryDir dir;
  if (!dir.isValid())
    return 1;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  QCoreApplication app(argc, argv);
  if (!sameWindowId("app.desktop", "app") || !sameWindowId("app", "app.desktop") ||
      sameWindowId("", "") || sameWindowId("app", "other"))
    qFatal("Window picker and rule matcher require equivalent, nonempty IDs");
  const WindowIdentity firstWindow{ {}, "Example", {}, {} };
  const WindowIdentity secondWindow{ "org.example.Editor", "Example", "example-process", "example-window" };
  const QJsonObject classRule{ { "class", "Example" } };
  const QJsonObject appRule{ { "app", "org.example.Editor.desktop" } };
  auto picked = secondWindow.toMap();
  picked.insert("key", "app");
  picked.insert("value", secondWindow.desktopFileName);
  const auto identity = WindowIdentity::fromPick(picked);
  if (firstWindow.matchingRuleIndex({ classRule }) != 0 || identity.matchingRuleIndex({ classRule, appRule }) != 0 ||
      !identity.matches({}, "Example") || identity.matches("example-process", {}) ||
      !identity.matches({}, "example-process") || !identity.matches("example-window", {}))
    qFatal("Picking a second window must reuse the same runtime app or class rule");
  auto disabled = classRule;
  disabled.insert("enabled", false);
  if (identity.matchingRuleIndex({ disabled, appRule }) != 1 || identity.matchingRuleIndex({ disabled }) != 0)
    qFatal("Prefer the active matching rule while allowing disabled rules to be selected again");
  const auto defaults = defaultSettings();
  if (readSettings() != defaults)
    qFatal("Unexpected initial settings");
  const QJsonObject title{ { "mode", "starts" }, { "title", "A.B" }, { "ignore_case", true }, { "profile_id", 17 } };
  const auto regex = titleExpression(title);
  if (!regex.match("a.b Test").hasMatch() || regex.match("A0B Test").hasMatch() || regex.match("xA.B").hasMatch())
    qFatal("Title matching must respect literal text, mode and case");
  auto next = defaults;
  next["profile_rules"] =
      QJsonArray{ QJsonObject{ { "app", "test.desktop" }, { "profile_id", 0 }, { "titles", QJsonArray{ title } } } };
  next["shell_profile_id"] = 4294967295.;
  next["effect_profiles"] = QJsonObject{ { "overview", 0 } };
  QString error;
  if (!writeSettings(next, defaults, &error) || readSettings() != next)
    qFatal("Round-trip failed");
  if (writeSettings(defaults, defaults, &error) || readSettings() != next)
    qFatal("Stale write");
  if (!writeSettings(defaults, next, &error) || readSettings() != defaults)
    qFatal("Reset failed");
  KConfig config("smooth-scroll-kwinrc", KConfig::SimpleConfig);
  KConfigGroup profiles(&config, "Profiles");
  const QByteArray malformed("[{");
  profiles.writeEntry("Rules", malformed);
  if (!config.sync())
    qFatal("Test setup failed");
  const auto broken = readSettings();
  auto edit = broken;
  edit["shell_profile_id"] = 9;
  if (broken["profile_rules"].isArray() || !writeSettings(edit, broken, &error))
    qFatal("Unrelated edits must work with damaged rules");
  config.reparseConfiguration();
  if (profiles.readEntry("Rules", QByteArray()) != malformed)
    qFatal("Unrelated edits overwrote damaged rules");
}
