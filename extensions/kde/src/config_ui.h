// SPDX-License-Identifier: MIT
#pragma once
#include <KLocalizedString>
#include <QDBusMessage>
#include <QHash>
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
  // KWin window types and effect names are keyed by their internal id; map the
  // known ones to translatable labels and fall back to a readable form.
  static const QHash<QString, QString> translated{
    { "desktop", i18nd("kwin_smooth_scroll", "Desktop") },
    { "dock", i18nd("kwin_smooth_scroll", "Dock") },
    { "applet_popup", i18nd("kwin_smooth_scroll", "Applet popup") },
    { "menu", i18nd("kwin_smooth_scroll", "Menu") },
    { "dropdown_menu", i18nd("kwin_smooth_scroll", "Dropdown menu") },
    { "popup_menu", i18nd("kwin_smooth_scroll", "Popup menu") },
    { "combo_box", i18nd("kwin_smooth_scroll", "Combo box") },
    { "tooltip", i18nd("kwin_smooth_scroll", "Tooltip") },
    { "notification", i18nd("kwin_smooth_scroll", "Notification") },
    { "critical_notification", i18nd("kwin_smooth_scroll", "Critical notification") },
    { "on_screen_display", i18nd("kwin_smooth_scroll", "On screen display") },
    { "lock_screen", i18nd("kwin_smooth_scroll", "Lock screen") },
    { "input_method", i18nd("kwin_smooth_scroll", "Input method") },
    { "outline", i18nd("kwin_smooth_scroll", "Outline") },
    { "dnd_icon", i18nd("kwin_smooth_scroll", "Dnd icon") },
    { "toolbar", i18nd("kwin_smooth_scroll", "Toolbar") },
    { "splash", i18nd("kwin_smooth_scroll", "Splash") },
    { "utility", i18nd("kwin_smooth_scroll", "Utility") },
    { "unknown", i18nd("kwin_smooth_scroll", "Unknown") },
    { "overview", i18nd("kwin_smooth_scroll", "Overview") },
    { "windowview", i18nd("kwin_smooth_scroll", "Windowview") },
    { "desktopgrid", i18nd("kwin_smooth_scroll", "Desktopgrid") },
    { "presentwindows", i18nd("kwin_smooth_scroll", "Presentwindows") },
    { "presentwindows_current", i18nd("kwin_smooth_scroll", "Presentwindows current") },
    { "presentwindows_all", i18nd("kwin_smooth_scroll", "Presentwindows all") },
  };
  if (const auto it = translated.constFind(key); it != translated.constEnd())
    return *it;
  key.replace('_', ' ');
  if (!key.isEmpty())
    key[0] = key[0].toUpper();
  return key;
}
}  // namespace SmoothScrollKWin
