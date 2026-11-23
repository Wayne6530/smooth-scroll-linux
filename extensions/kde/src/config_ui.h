// SPDX-License-Identifier: MIT
#pragma once
#include <QDBusMessage>
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QPushButton>

namespace SmoothScrollKWin
{
inline QDoubleSpinBox* profileSpin(double value)
{
  auto* spin = new QDoubleSpinBox;
  spin->setDecimals(0);
  spin->setRange(0, 4294967295.0);
  spin->setValue(value);
  return spin;
}
inline QDBusMessage effectCall(const QString& method)
{
  return QDBusMessage::createMethodCall("org.smooth_scroll.KWinEffect", "/SmoothScrollKWinEffect", {}, method);
}
inline QString displayKey(QString key)
{
  key.replace('_', ' ');
  if (!key.isEmpty())
    key[0] = key[0].toUpper();
  return key;
}
}  // namespace SmoothScrollKWin
