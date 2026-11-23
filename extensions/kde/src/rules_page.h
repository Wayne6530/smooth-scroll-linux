// SPDX-License-Identifier: MIT
#pragma once
#include <QJsonArray>
#include <QTreeWidget>
#include <functional>

namespace SmoothScrollKWin
{
class RulesPage : public QWidget
{
  Q_OBJECT
public:
  explicit RulesPage(QWidget* parent = nullptr);
  void setRules(const QJsonArray& rules);
  QJsonArray rules() const;
  std::function<double()> defaultProfile;
Q_SIGNALS:
  void edited();

private:
  QTreeWidgetItem* append(QTreeWidgetItem* parent, QJsonObject rule);
  void pick(QWidget* owner, std::function<void(QVariantMap)> selected);
  void titleDialog(QTreeWidgetItem* application, QTreeWidgetItem* exception);
  QTreeWidget* tree;
};
}  // namespace SmoothScrollKWin
