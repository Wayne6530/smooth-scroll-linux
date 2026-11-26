// SPDX-License-Identifier: MIT
#include "config_ui.h"
#include "effect_settings.h"
#include "rules_page.h"

#include <KLocalizedString>
#include <KCModule>
#include <KPluginFactory>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace SmoothScrollKWin
{
class ConfigModule : public KCModule
{
  Q_OBJECT
public:
  ConfigModule(QObject* parent, const KPluginMetaData& meta) : KCModule(parent, meta)
  {
    tabs = new QTabWidget;
    (new QVBoxLayout(widget()))->addWidget(tabs);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ConfigModule::refreshStatus);
    timer->start(2000);
    load();
  }
  void load() override
  {
    baseline = readSettings();
    showSettings(baseline);
    setNeedsSave(false);
  }
  void defaults() override
  {
    showSettings(defaultSettings());
    changed();
  }
  void save() override
  {
    if (rulesEdited)
      draft.insert("profile_rules", rules->rules());
    auto expected = baseline;
    auto next = draft;
    QString error;
    while (!writeSettings(next, expected, &error))
    {
      const auto latest = readSettings();
      const bool conflict = latest != expected;
      QMessageBox warning(QMessageBox::Warning, "Smooth Scroll", error, QMessageBox::NoButton, widget());
      auto* retry = warning.addButton(conflict ? i18nd("kwin_smooth_scroll", "Save my changes") : i18nd("kwin_smooth_scroll", "Retry"), QMessageBox::AcceptRole);
      auto* discard = warning.addButton(i18nd("kwin_smooth_scroll", "Discard my changes"), QMessageBox::DestructiveRole);
      if (conflict)
        warning.setInformativeText(i18nd("kwin_smooth_scroll", "Saving keeps your edits and other changes to untouched settings. If you edited application rules, your rule list replaces the saved list."));
      warning.setDefaultButton(retry);
      warning.exec();
      if (warning.clickedButton() == discard)
      {
        load();
        return;
      }
      if (warning.clickedButton() == retry && conflict)
      {
        expected = latest;
        next = applySettingsEdits(latest, baseline, draft);
      }
    }
    baseline = next;
    showSettings(next);
    setNeedsSave(false);
    auto request =
        QDBusMessage::createMethodCall("org.kde.KWin", "/Effects", "org.kde.kwin.Effects", "reconfigureEffect");
    request << QStringLiteral("kwin_smooth_scroll");
    QDBusConnection::sessionBus().asyncCall(request);
  }

private:
  void changed()
  {
    if (!building)
      setNeedsSave(true);
  }
  QVBoxLayout* page(const QString& name)
  {
    auto* body = new QWidget;
    auto* area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidget(body);
    tabs->addTab(area, name);
    return new QVBoxLayout(body);
  }
  QDoubleSpinBox* profile(QFormLayout* form, const QString& label, const QString& key)
  {
    auto* input = profileSpin(draft.value(key).toDouble());
    form->addRow(label, input);
    connect(input, &QDoubleSpinBox::valueChanged, this, [this, key](double id) {
      draft.insert(key, id);
      changed();
    });
    return input;
  }
  void mapping(QVBoxLayout* page, const QString& title, const QString& key, const QStringList& names)
  {
    auto* group = new QGroupBox(title);
    page->addWidget(group);
    auto* form = new QFormLayout(group);
    auto* list = new QComboBox;
    for (const auto& name : names)
      list->addItem(displayKey(name), name);
    form->addRow(i18nd("kwin_smooth_scroll", "Target"), list);
    auto* enabled = new QCheckBox(i18nd("kwin_smooth_scroll", "Override Shell profile"));
    form->addRow(enabled);
    auto* id = profileSpin(0);
    form->addRow(i18nd("kwin_smooth_scroll", "Profile ID"), id);
    auto refresh = [=, this] {
      const auto map = draft.value(key).toObject();
      const auto name = list->currentData().toString();
      QSignalBlocker blockToggle(enabled), blockId(id);
      enabled->setChecked(map.contains(name));
      id->setEnabled(enabled->isChecked());
      id->setValue(map.value(name).toDouble(draft.value("shell_profile_id").toDouble()));
    };
    auto update = [=, this] {
      auto map = draft.value(key).toObject();
      const auto name = list->currentData().toString();
      if (enabled->isChecked())
        map.insert(name, id->value());
      else
        map.remove(name);
      draft.insert(key, map);
      refresh();
      changed();
    };
    connect(list, &QComboBox::currentIndexChanged, this, refresh);
    connect(enabled, &QCheckBox::toggled, this, update);
    connect(id, &QDoubleSpinBox::valueChanged, this, update);
    connect(shellId, &QDoubleSpinBox::valueChanged, this, refresh);
    refresh();
  }
  void showSettings(QJsonObject values)
  {
    building = true;
    rulesEdited = false;
    draft = values;
    const auto index = tabs->currentIndex();
    while (tabs->count())
      delete tabs->widget(0);
    auto* main = page(i18nd("kwin_smooth_scroll", "Application rules"));
    auto* help = new QLabel(i18nd("kwin_smooth_scroll", "Use the numeric profile IDs from the Web configurator."));
    help->setWordWrap(true);
    main->addWidget(help);
    status = new QLabel(i18nd("kwin_smooth_scroll", "Reading profile status…"));
    main->addWidget(status);
    auto* defaults = new QFormLayout;
    main->addLayout(defaults);
    profile(defaults, i18nd("kwin_smooth_scroll", "Default application profile"), "default_profile_id");
    rules = new RulesPage;
    rules->defaultProfile = [this] { return draft.value("default_profile_id").toDouble(); };
    rules->setRules(draft.value("profile_rules").toArray());
    main->addWidget(rules);
    if (!draft.value("profile_rules").isArray())
    {
      main->addWidget(new QLabel(i18nd("kwin_smooth_scroll", "Saved application rules could not be read. Other settings can still be saved.")));
      rules->setEnabled(false);
    }
    connect(rules, &RulesPage::edited, this, [this] {
      rulesEdited = true;
      changed();
    });

    auto* shell = page(i18nd("kwin_smooth_scroll", "Desktop and effects"));
    auto* form = new QFormLayout;
    shell->addLayout(form);
    shellId = profile(form, i18nd("kwin_smooth_scroll", "Shell profile ID"), "shell_profile_id");
    mapping(shell, i18nd("kwin_smooth_scroll", "Window type"), "window_type_profiles",
            { "desktop", "dock", "applet_popup", "menu", "dropdown_menu", "popup_menu", "combo_box", "tooltip",
              "notification", "critical_notification", "on_screen_display", "lock_screen", "input_method", "outline",
              "dnd_icon", "toolbar", "splash", "utility", "unknown" });
    mapping(
        shell, i18nd("kwin_smooth_scroll", "Active effect"), "effect_profiles",
        { "overview", "windowview", "desktopgrid", "presentwindows", "presentwindows_current", "presentwindows_all" });
    shell->addStretch();
    auto* behavior = page(i18nd("kwin_smooth_scroll", "Behavior"));
    auto* indicators = page(i18nd("kwin_smooth_scroll", "Indicators"));
    QFormLayout* fields = nullptr;
    QString current;
    for (const auto& f : settingsFields())
    {
      if (f.group != current)
      {
        current = f.group;
        const QMap<QString, QString> titles{ { "arrow", i18nd("kwin_smooth_scroll", "Drag View arrow") },
                                             { "dot", i18nd("kwin_smooth_scroll", "Inertia dot") },
                                             { "scroll", i18nd("kwin_smooth_scroll", "Scrolling") },
                                             { "passthrough", i18nd("kwin_smooth_scroll", "Compatibility indicator") } };
        auto* group = new QGroupBox(titles.value(current, displayKey(current)));
        (current == "scroll" ? behavior : indicators)->addWidget(group);
        fields = new QFormLayout(group);
      }
      addField(fields, f);
    }
    behavior->addStretch();
    indicators->addStretch();
    tabs->setCurrentIndex(qMax(index, 0));
    building = false;
    refreshStatus();
  }
  void addField(QFormLayout* form, const Field& f)
  {
    auto update = [this, f](QJsonValue value) {
      auto group = draft.value(f.group).toObject();
      group.insert(f.key, value);
      draft.insert(f.group, group);
      changed();
    };
    const auto value = draft.value(f.group).toObject().value(f.key);
    if (f.initial.typeId() == QMetaType::Bool)
    {
      auto* check = new QCheckBox(f.label);
      check->setChecked(value.toBool());
      form->addRow(check);
      connect(check, &QCheckBox::toggled, this, [update](bool v) { update(v); });
    }
    else if (f.initial.typeId() == QMetaType::QString)
    {
      auto* color = new QPushButton(value.toString());
      form->addRow(f.label, color);
      connect(color, &QPushButton::clicked, this, [=] {
        const auto selected = QColorDialog::getColor(QColor(color->text()), color);
        if (!selected.isValid())
          return;
        color->setText(selected.name());
        update(selected.name());
      });
    }
    else
    {
      auto* spin = new QDoubleSpinBox;
      const bool integer = f.initial.typeId() == QMetaType::Int;
      spin->setDecimals(integer ? 0 : 3);
      spin->setSingleStep(integer ? 1 : 0.01);
      spin->setRange(f.minimum, f.maximum);
      spin->setValue(value.toDouble());
      form->addRow(f.label, spin);
      connect(spin, &QDoubleSpinBox::valueChanged, this, [update](double v) { update(v); });
    }
  }
  void refreshStatus()
  {
    if (statusPending)
      return;
    statusPending = true;
    auto* watcher =
        new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(effectCall("profileStatus"), 1500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* done) {
      QDBusPendingReply<QVariantMap> reply = *done;
      done->deleteLater();
      statusPending = false;
      if (reply.isError())
        status->setText(i18nd("kwin_smooth_scroll", "Enable the Smooth Scroll effect for window selection and live status."));
      else
      {
        const auto state = reply.value();
        if (!state.value("available").toBool())
          status->setText(i18nd("kwin_smooth_scroll", "Daemon or input device unavailable"));
        else
        {
          auto text = i18nd("kwin_smooth_scroll", "Requested: %1    Active: %2", state.value("requested").toUInt(),
                            state.value("active").toUInt());
          if (state.value("requested") != state.value("active"))
            text += QStringLiteral("    ") + i18nd("kwin_smooth_scroll", "Not yet applied");
          status->setText(text);
        }
      }
    });
  }
  QTabWidget* tabs;
  QTimer* timer;
  RulesPage* rules = nullptr;
  QLabel* status = nullptr;
  QDoubleSpinBox* shellId = nullptr;
  QJsonObject draft, baseline;
  bool building = false, statusPending = false, rulesEdited = false;
};
}  // namespace SmoothScrollKWin
using SmoothScrollKWin::ConfigModule;
K_PLUGIN_CLASS_WITH_JSON(ConfigModule, "config_module.json")
#include "config_module.moc"
