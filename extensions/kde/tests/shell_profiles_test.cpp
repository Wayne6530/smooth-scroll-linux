// SPDX-License-Identifier: MIT
#include "shell_profiles.h"

#include <QJsonDocument>
#include <iostream>

using namespace SmoothScrollKWin;

int main()
{
  int failures = 0;
  const auto check = [&failures](bool passed, const char* message) {
    if (!passed)
    {
      std::cerr << message << '\n';
      ++failures;
    }
  };
  const auto root = QJsonDocument::fromJson(R"({
    "window_type_profiles": {"applet_popup": 0, "popup_menu": 3},
    "effect_profiles": {"overview": 2, "blur": 9}
  })")
                        .object();
  const auto windows = readProfileMap(root.value("window_type_profiles").toObject(), 1);
  const auto effects = readProfileMap(root.value("effect_profiles").toObject(), 1);
  QString reason;
  check(shellProfile({}, "applet_popup", effects, windows, 1, &reason) == 0, "zero is an explicit profile");
  check(reason == "window_type_rule", "window rule diagnostic");
  check(shellProfile({}, "popup_menu", effects, windows, 1) == 3, "menu override");
  check(shellProfile({}, "dock", effects, windows, 1, &reason) == 1, "unmapped type fallback");
  check(reason == "shell_default", "fallback diagnostic");
  check(shellProfile({}, {}, effects, windows, 4) == 4, "no-window fallback");
  check(shellProfile("overview", "applet_popup", effects, windows, 1, &reason) == 2, "effect takes priority");
  check(reason == "effect_rule", "effect rule diagnostic");
  check(shellProfile("windowview", "applet_popup", effects, windows, 1) == 1,
        "unmapped effect must not fall through to underlying window");
  check(shellProfile("overview", {}, {}, {}, 1) == 1, "empty maps retain old behavior");
  check(readProfileMap({}, 1).isEmpty(), "missing maps are empty");
  check(interactionEffect({ "blur", "kwin_smooth_scroll" }).isEmpty(), "visual effects cannot select profiles");
  check(interactionEffect({ "blur", "windowview" }) == "windowview", "supported interaction effect");
  check(interactionEffect({ "windowview", "overview" }) == "overview", "deterministic transition priority");
  check(interactionEffect({ "overview", "windowview" }) == "overview", "priority is order independent");
  for (const QString effect : { "desktopgrid", "presentwindows", "presentwindows_current", "presentwindows_all" })
  {
    check(interactionEffect({ effect }) == effect, "existing effect remains supported");
  }
  return failures ? 1 : 0;
}
