// SPDX-License-Identifier: MIT
#pragma once

#include <QJsonObject>
#include <QMap>
#include <QStringList>

#include <cstdint>

namespace SmoothScrollKWin
{
using ProfileMap = QMap<QString, uint32_t>;

inline ProfileMap readProfileMap(const QJsonObject& object, uint32_t fallback)
{
  ProfileMap result;
  for (auto it = object.begin(); it != object.end(); ++it)
  {
    result.insert(it.key(), static_cast<uint32_t>(it.value().toDouble(fallback)));
  }
  return result;
}

// Fixed priority makes overlapping effect transitions independent of stacking order.
inline QString interactionEffect(const QStringList& activeEffects)
{
  const QStringList supported = { QStringLiteral("overview"),
                                  QStringLiteral("windowview"),
                                  QStringLiteral("desktopgrid"),
                                  QStringLiteral("presentwindows"),
                                  QStringLiteral("presentwindows_current"),
                                  QStringLiteral("presentwindows_all") };
  for (const QString& name : supported)
  {
    if (activeEffects.contains(name))
    {
      return name;
    }
  }
  return {};
}

inline uint32_t shellProfile(const QString& effect, const QString& windowType, const ProfileMap& effects,
                             const ProfileMap& windowTypes, uint32_t fallback, QString* reason = nullptr)
{
  const ProfileMap& profiles = effect.isEmpty() ? windowTypes : effects;
  const QString key = effect.isEmpty() ? windowType : effect;
  const auto it = key.isEmpty() ? profiles.end() : profiles.find(key);
  if (reason)
  {
    *reason = it == profiles.end() ? QStringLiteral("shell_default") :
              effect.isEmpty()     ? QStringLiteral("window_type_rule") :
                                     QStringLiteral("effect_rule");
  }
  return it == profiles.end() ? fallback : it.value();
}
}  // namespace SmoothScrollKWin
