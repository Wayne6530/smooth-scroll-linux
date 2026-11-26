// SPDX-License-Identifier: MIT
#include "rules_page.h"
#include "config_ui.h"
#include "effect_settings.h"

#include <KLocalizedString>
#include <QCheckBox>
#include <QComboBox>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

namespace SmoothScrollKWin
{
namespace
{
const QStringList modes{ "contains", "equals", "starts", "ends", "regex" };
QStringList ruleModeLabels()
{
  return { i18nd("kwin_smooth_scroll", "Contains"), i18nd("kwin_smooth_scroll", "Equals"), i18nd("kwin_smooth_scroll", "Starts with"), i18nd("kwin_smooth_scroll", "Ends with"), i18nd("kwin_smooth_scroll", "Regular expression") };
}
QString titleLabel(const QJsonObject& rule)
{
  return ruleModeLabels().value(modes.indexOf(rule.value("mode").toString("regex"))) + ": " +
         rule.value("title").toString();
}
QJsonObject stored(QTreeWidgetItem* item)
{
  return item->data(0, Qt::UserRole).toJsonObject();
}
}  // namespace
RulesPage::RulesPage(QWidget* parent) : QWidget(parent), tree(new QTreeWidget)
{
  auto* layout = new QVBoxLayout(this);
  tree->setHeaderLabels({ i18nd("kwin_smooth_scroll", "Application / title exception"), i18nd("kwin_smooth_scroll", "Profile ID") });
  tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  layout->addWidget(tree);
  auto* buttons = new QHBoxLayout;
  layout->addLayout(buttons);
  const auto button = [buttons](const QString& text) {
    auto* b = new QPushButton(text);
    buttons->addWidget(b);
    return b;
  };
  auto* add = button(i18nd("kwin_smooth_scroll", "Select window…"));
  auto* title = button(i18nd("kwin_smooth_scroll", "Add title…"));
  auto* edit = button(i18nd("kwin_smooth_scroll", "Edit title…"));
  auto* remove = button(i18nd("kwin_smooth_scroll", "Remove"));
  auto* up = button(i18nd("kwin_smooth_scroll", "Up"));
  auto* down = button(i18nd("kwin_smooth_scroll", "Down"));
  down->setObjectName(QStringLiteral("moveDown"));
  auto update = [=, this]() {
    auto* item = tree->currentItem();
    auto* parentItem = item ? item->parent() : nullptr;
    title->setEnabled(item);
    edit->setEnabled(parentItem);
    remove->setEnabled(item);
    up->setEnabled(parentItem && parentItem->indexOfChild(item) > 0);
    down->setEnabled(parentItem && parentItem->indexOfChild(item) + 1 < parentItem->childCount());
  };
  connect(tree, &QTreeWidget::currentItemChanged, this, update);
  connect(tree, &QTreeWidget::itemChanged, this, &RulesPage::edited);
  connect(add, &QPushButton::clicked, this, [this] {
    pick(this, [this](const QVariantMap& info) {
      const auto key = info.value("key").toString(), value = info.value("value").toString();
      const int index = WindowIdentity::fromPick(info).matchingRuleIndex(rules());
      if (index >= 0)
      {
        tree->setCurrentItem(tree->topLevelItem(index));
        return;
      }
      tree->setCurrentItem(append(nullptr, { { key, value }, { "profile_id", defaultProfile() } }));
      Q_EMIT edited();
    });
  });
  connect(title, &QPushButton::clicked, this, [this] {
    auto* row = tree->currentItem();
    titleDialog(row->parent() ? row->parent() : row, nullptr);
  });
  connect(edit, &QPushButton::clicked, this, [this] {
    auto* row = tree->currentItem();
    titleDialog(row->parent(), row);
  });
  connect(remove, &QPushButton::clicked, this, [this, update] {
    delete tree->currentItem();
    update();
    Q_EMIT edited();
  });
  auto move = [this, update](int delta) {
    auto* row = tree->currentItem();
    auto* parentItem = row->parent();
    const int index = parentItem->indexOfChild(row);
    // Recreate from the draft so the embedded profile editor moves with the row.
    auto values = rules();
    const int appIndex = tree->indexOfTopLevelItem(parentItem);
    auto app = values[appIndex].toObject();
    auto titles = app.value("titles").toArray();
    titles.insert(index + delta, titles.takeAt(index));
    app.insert("titles", titles);
    values[appIndex] = app;
    setRules(values);
    tree->setCurrentItem(tree->topLevelItem(appIndex)->child(index + delta));
    update();
    Q_EMIT edited();
  };
  connect(up, &QPushButton::clicked, this, [move] { move(-1); });
  connect(down, &QPushButton::clicked, this, [move] { move(1); });
  update();
}

QTreeWidgetItem* RulesPage::append(QTreeWidgetItem* parentItem, QJsonObject rule)
{
  const auto titles = rule.take("titles").toArray();
  auto* row = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(tree);
  row->setData(0, Qt::UserRole, rule);
  row->setCheckState(0, rule.value("enabled").toBool(true) ? Qt::Checked : Qt::Unchecked);
  row->setText(0, parentItem ? titleLabel(rule) : rule.value("app").toString(rule.value("class").toString()));
  auto* parentSpin = parentItem ? qobject_cast<QDoubleSpinBox*>(tree->itemWidget(parentItem, 1)) : nullptr;
  auto* spin = profileSpin(rule.value("profile_id").toDouble(parentSpin ? parentSpin->value() : 0));
  tree->setItemWidget(row, 1, spin);
  connect(spin, &QDoubleSpinBox::valueChanged, this, [this, row](double id) {
    auto value = stored(row);
    value.insert("profile_id", id);
    row->setData(0, Qt::UserRole, value);
    Q_EMIT edited();
  });
  if (parentSpin)
    connect(parentSpin, &QDoubleSpinBox::valueChanged, spin, [row, spin](double id) {
      if (!stored(row).contains("profile_id"))
      {
        const QSignalBlocker block(spin);
        spin->setValue(id);
      }
    });
  for (const auto& value : titles)
    append(row, value.toObject());
  row->setExpanded(true);
  return row;
}

void RulesPage::setRules(const QJsonArray& rules)
{
  const QSignalBlocker block(this);
  tree->clear();
  for (const auto& rule : rules)
    append(nullptr, rule.toObject());
}
QJsonArray RulesPage::rules() const
{
  const auto read = [this](QTreeWidgetItem* row) {
    auto value = stored(row);
    value.insert("enabled", row->checkState(0) == Qt::Checked);

    return value;
  };
  QJsonArray result;
  for (int i = 0; i < tree->topLevelItemCount(); ++i)
  {
    auto* app = tree->topLevelItem(i);
    auto rule = read(app);
    QJsonArray titles;
    for (int j = 0; j < app->childCount(); ++j)
      titles.append(read(app->child(j)));
    rule.insert("titles", titles);
    result.append(rule);
  }
  return result;
}

void RulesPage::pick(QWidget* owner, std::function<void(QVariantMap)> selected)
{
  QPointer<QWidget> host = window(), editor = owner->window();
  if (editor != host)
    editor->hide();
  host->hide();
  auto request = QDBusMessage::createMethodCall("org.kde.KWin", "/KWin", "org.kde.KWin", "queryWindowInfo");
  auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 120000), owner);
  connect(watcher, &QDBusPendingCallWatcher::finished, owner, [=](QDBusPendingCallWatcher* done) {
    QDBusPendingReply<QVariantMap> reply = *done;
    done->deleteLater();
    if (!host || !editor)
      return;
    host->show();
    if (editor != host)
      editor->show();
    editor->raise();
    editor->activateWindow();
    if (reply.isError())
    {
      if (!reply.error().name().contains("Cancel", Qt::CaseInsensitive))
        QMessageBox::information(editor, i18nd("kwin_smooth_scroll", "Select window"), reply.error().message());
      return;
    }
    auto call = effectCall("pickedWindow");
    call << reply.value().value("uuid").toString();
    QDBusReply<QVariantMap> result = QDBusConnection::sessionBus().call(call, QDBus::Block, 1500);
    if (!result.isValid() || result.value().isEmpty())
    {
      QMessageBox::information(editor, i18nd("kwin_smooth_scroll", "Select window"),
                               result.isValid() ? i18nd("kwin_smooth_scroll", "Select a regular application window.") : i18nd("kwin_smooth_scroll", "Enable the Smooth Scroll desktop effect to select windows."));
      return;
    }
    selected(result.value());
  });
}
void RulesPage::titleDialog(QTreeWidgetItem* application, QTreeWidgetItem* exception)
{
  const auto app = stored(application);
  const auto original = exception ? stored(exception) : QJsonObject();
  auto* dialog = new QDialog(window());
  dialog->setWindowTitle(i18nd("kwin_smooth_scroll", "Window title exception"));
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  auto* form = new QFormLayout(dialog);
  auto* mode = new QComboBox;
  mode->addItems(ruleModeLabels());
  mode->setCurrentIndex(modes.indexOf(original.value("mode").toString(exception ? "regex" : "contains")));
  form->addRow(i18nd("kwin_smooth_scroll", "Match method"), mode);
  auto* text = new QLineEdit(original.value("title").toString());
  auto* line = new QHBoxLayout;
  line->addWidget(text);
  auto* fromWindow = new QPushButton(i18nd("kwin_smooth_scroll", "From window…"));
  line->addWidget(fromWindow);
  form->addRow(i18nd("kwin_smooth_scroll", "Title text"), line);
  auto* insensitive = new QCheckBox(i18nd("kwin_smooth_scroll", "Ignore case"));
  insensitive->setChecked(original.value("ignore_case").toBool());
  form->addRow(insensitive);
  auto* profile =
      profileSpin(qobject_cast<QDoubleSpinBox*>(tree->itemWidget(exception ? exception : application, 1))->value());
  form->addRow(i18nd("kwin_smooth_scroll", "Profile ID"), profile);
  auto* error = new QLabel;
  error->setWordWrap(true);
  form->addRow(error);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  form->addRow(buttons);
  const auto value = [=] {
    return QJsonObject{ { "mode", modes.value(mode->currentIndex()) },
                        { "title", text->text() },
                        { "profile_id", profile->value() },
                        { "ignore_case", insensitive->isChecked() } };
  };
  auto validate = [=] {
    const auto regex = titleExpression(value());
    buttons->button(QDialogButtonBox::Save)->setEnabled(!text->text().isEmpty() && regex.isValid());
    error->setText(regex.isValid() ? QString() : regex.errorString());
  };
  connect(text, &QLineEdit::textChanged, dialog, validate);
  connect(mode, &QComboBox::currentIndexChanged, dialog, validate);
  connect(fromWindow, &QPushButton::clicked, dialog, [=, this] {
    pick(dialog, [=](const QVariantMap& info) {
      if (WindowIdentity::fromPick(info).matches(app.value("app").toString(), app.value("class").toString()))
        text->setText(info.value("title").toString());
      else
        QMessageBox::information(dialog, i18nd("kwin_smooth_scroll", "Select window"), i18nd("kwin_smooth_scroll", "Select a window of this application."));
    });
  });
  connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, dialog, [=, this] {
    auto rule = value();
    if (exception && !original.contains("profile_id") &&
        profile->value() == qobject_cast<QDoubleSpinBox*>(tree->itemWidget(application, 1))->value())
      rule.remove("profile_id");
    if (exception)
    {
      exception->setData(0, Qt::UserRole, rule);
      exception->setText(0, titleLabel(rule));
      qobject_cast<QDoubleSpinBox*>(tree->itemWidget(exception, 1))->setValue(profile->value());
    }
    else
    {
      tree->setCurrentItem(append(application, rule));
      application->setExpanded(true);
    }
    Q_EMIT edited();
    dialog->accept();
  });
  validate();
  dialog->open();
}
}  // namespace SmoothScrollKWin
