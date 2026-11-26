// SPDX-License-Identifier: MIT
#include "effect_settings.h"
#include <KCModule>
#include <KPluginFactory>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>

using namespace SmoothScrollKWin;
int main(int argc, char** argv)
{
  QTemporaryDir dir;
  if (!dir.isValid() || argc != 2)
    return 1;
  qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
  QApplication app(argc, argv);
  auto initial = defaultSettings();
  const QJsonObject first{ { "mode", "contains" }, { "title", "first" }, { "profile_id", 2 }, { "enabled", true } };
  const QJsonObject second{ { "mode", "contains" }, { "title", "second" }, { "enabled", true } };
  initial["profile_rules"] = QJsonArray{ QJsonObject{
      { "app", "example" }, { "profile_id", 7 }, { "enabled", true }, { "titles", QJsonArray{ first, second } } } };
  QString error;
  if (!writeSettings(initial, defaultSettings(), &error))
    qFatal("%s", qPrintable(error));
  QWidget parent;
  const auto plugin =
      KPluginFactory::instantiatePlugin<KCModule>(KPluginMetaData(QString::fromLocal8Bit(argv[1])), &parent);
  if (!plugin)
    qFatal("%s", qPrintable(plugin.errorString));
  auto* module = plugin.plugin;
  auto* view = module->widget()->findChild<QTreeWidget*>();
  if (!view || view->topLevelItemCount() != 1)
    qFatal("Application rule was not loaded");
  auto* inherited = qobject_cast<QDoubleSpinBox*>(view->itemWidget(view->topLevelItem(0)->child(1), 1));
  auto* parentId = qobject_cast<QDoubleSpinBox*>(view->itemWidget(view->topLevelItem(0), 1));
  if (inherited->value() != 7)
    qFatal("Inherited title profile was not displayed");
  parentId->setValue(8);
  if (inherited->value() != 8)
    qFatal("Changing parent did not update inherited profile");
  auto* firstTitle = view->topLevelItem(0)->child(0);
  view->setCurrentItem(firstTitle);
  auto* spin = qobject_cast<QDoubleSpinBox*>(view->itemWidget(firstTitle, 1));
  spin->setValue(4294967295.);
  if (auto* moveDown = module->widget()->findChild<QPushButton*>(QStringLiteral("moveDown")))
    moveDown->click();
  else
    qFatal("Missing move-down button");
  if (readSettings() != initial)
    qFatal("Draft leaked before Apply");
  auto concurrent = initial;
  concurrent["shell_profile_id"] = 11;
  if (!writeSettings(concurrent, initial, &error))
    qFatal("Concurrent setup failed");
  QTimer::singleShot(0, [] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (box)
      for (auto* button : box->buttons())
        if (box->buttonRole(button) == QMessageBox::AcceptRole)
        {
          button->click();
          return;
        }
    qFatal("Missing conflict resolution dialog");
  });
  module->save();
  if (readSettings()["shell_profile_id"].toInt() != 11)
    qFatal("Resolving conflict overwrote untouched setting");
  auto titles = readSettings()["profile_rules"].toArray().first().toObject()["titles"].toArray();
  if (titles.size() != 2 || titles[0].toObject().contains("profile_id") || titles[1].toObject()["title"] != "first" ||
      titles[1].toObject()["profile_id"].toInteger() != 4294967295LL)
    qFatal("Reordered rule lost its profile");
  module->defaults();
  module->save();
  if (readSettings() != defaultSettings())
    qFatal("Defaults did not reset the module");
}
